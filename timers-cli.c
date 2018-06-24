#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <curl/curl.h>

#include "libs/curl_conn.h"
#include "libs/easy_args.h"
#include "libs/logger.h"
#include "libs/easy_json.h"

#define URL "https://api.kitinfo.de/timers/"

struct MemoryStruct {
	char* memory;
	size_t size;
};

typedef struct {
	LOGGER log;
	char* pre;
	char* post;
	int max;
	struct MemoryStruct* data;
} Config;


size_t handleData(char* ptr, size_t size, size_t nmemb, Config* config) {
	int length = size * nmemb;

	config->data->memory = realloc(config->data->memory, config->data->size + length + 1);

	// out of memory
	if (config->data->memory == NULL) {
		return 0;
	}

	memcpy(&(config->data->memory[config->data->size]), ptr, length);
	config->data->size += length;
	config->data->memory[config->data->size] = 0;

	return size * nmemb;
}

int count_str(char* src) {
	int counter = 0;
	while (*src != 0) {
		//printf("%02x ", (unsigned char) *src);
		switch(*src & 0xE0) {
			case 0xC0:
				counter++;
				break;
			case 0xE0:
				counter += 2;
				break;
		}
		src++;
	}
	return counter;
}

int convert_str_to_int(ejson_key* next, int* container) {
	int status;
	char* value;
	char* endptr;
	status = ejson_get_string(next->value, &value);
	if (status != EJSON_OK ) {
		return status;
	}
	*container = strtoul(value, &endptr, 10);
	if (value == endptr) {
		return 1;
	}
	return EJSON_OK;
}

bool print_item(Config* config, ejson_base* ejson) {

	if (!ejson || ejson->type != EJSON_OBJECT) {
		logprintf(config->log, LOG_ERROR, "The timers item is not an object.\n");
		return false;
	}

	char* event = "";
	char* message = "";

	struct tm event_time = {0};
	long i;
	int status;
	ejson_key* next;
	for (i = 0; i < ejson->object.length; i++) {
		next = ejson->object.keys[i];
		if (!next->key) {
			continue;
		} else if (!strcmp(next->key, "event")) {
			status = ejson_get_string(next->value, &event);
		} else if (!strcmp(next->key, "message")) {
			status = ejson_get_string(next->value, &message);
		} else if (!strcmp(next->key, "day")) {
			status = convert_str_to_int(next, &event_time.tm_mday);
		} else if (!strcmp(next->key, "month")) {
			status = convert_str_to_int(next, &event_time.tm_mon);
			event_time.tm_mon--;
		} else if (!strcmp(next->key, "year")) {
			status = convert_str_to_int(next, &event_time.tm_year);
			event_time.tm_year -= 1900;
		} else if (!strcmp(next->key, "hour")) {
			status = convert_str_to_int(next, &event_time.tm_hour);
		} else if (!strcmp(next->key, "minute")) {
			status = convert_str_to_int(next, &event_time.tm_min);
		} else if (!strcmp(next->key, "second")) {
			status = convert_str_to_int(next, &event_time.tm_sec);
		} else {
			status = EJSON_OK;
		}

		if (status != EJSON_OK) {
			logprintf(config->log, LOG_ERROR, "Cannot parse key: %s\n", next->key);
			return false;
		}
	}

	char* words[] = {
		"seconds",
		"minutes",
		"hours",
		"days"
	};

	time_t act = mktime(&event_time);
	time_t now = time(NULL);
	time_t diff = act - now;

	if (diff < -3600 * 24) {
		return true;
	}

	char* word = NULL;
	if (diff < 60) {
		word = words[0];
	} else if (diff < 60 * 60) {
		diff /= 60;
		word = words[1];
	} else if (diff < 60 * 60 * 24) {
		diff /= 60 * 60;
		word = words[2];
	} else {
		diff /= 60 * 60 * 24;
		word = words[3];
	}

	printf("%s%s\n%s", config->pre, event, asctime(&event_time));
	if (act - now > 0) {
		printf("%zd %s left\n", diff, word);
	}
	if (act - now < 3600 && strlen(message) > 0) {
		printf("(%s)", message);
	}
	printf("%s", config->post);

	fflush(stdout);
	sleep(10);
	return true;
}

void print_list(Config* config, ejson_base* ejson) {

	if (!ejson) {
		return;
	}

	if (ejson->type != EJSON_OBJECT) {
		logprintf(config->log, LOG_ERROR, "The json root must be an object.\n");
	}

	ejson = ejson_find_by_key(&ejson->object, "timers", false, false);

	if (!ejson) {
		logprintf(config->log, LOG_ERROR, "Something is wrong with the json (Cannot find timers key).\n");
		return;
	}

	if (ejson->type != EJSON_ARRAY) {
		logprintf(config->log, LOG_ERROR, "The type of the key 'timers' must be an array.\n");
		return;
	}


	if (ejson->array.length == 0) {
		printf("%sKeine Timers%s", config->pre, config->post);
		sleep(1200);
	}

	long i = 0;
	for (i = 0; i < ejson->array.length; i++) {

		if (!print_item(config, ejson->array.values[i])) {
			logprintf(config->log, LOG_ERROR, "Cannot print item.\n");
			printf("%sKann Timer nicht anzeigen%s", config->pre, config->post);
			sleep(60);
			return;
		}

		if (i > config->max) {
			break;
		}
	}
}

void run(Config* config, char* url) {
		config->data->memory = malloc(1);
		config->data->size = 0;
		int status = get_request(config->log, url, handleData, config, "", "");

		logprintf(config->log, LOG_INFO, "STATUS: %d\n", status);
		ejson_base* ejson = NULL;

		if (ejson_parse(config->data->memory, config->data->size, &ejson) != EJSON_OK) {
			logprintf(config->log, LOG_ERROR, "ERROR in parsing json.\n");
		} else {
			print_list(config, ejson);
		}

		ejson_cleanup(ejson);

		free(config->data->memory);
}

int main(int argc, char** argv) {

	LOGGER log = {
		.stream = stdout,
		.verbosity = 4
	};

	struct MemoryStruct ms = {
		.size = 0,
	};

	Config config = {
		.log = log,
		.pre = "\f",
		.post = "\n",
		.max = 10,
		.data = &ms
	};


	while (true) {
		run(&config, URL);
	}

	return 0;
}
