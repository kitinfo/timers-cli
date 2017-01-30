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

#define URL "http://api.kitinfo.de/timers/"

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

void print_meal(Config* config, ejson_struct* ejson) {

	char* meal = "";
	char* dish = "";
	bool bio = false;
	bool fish = false;
	bool pork = false;
	bool cow = false;
	bool cow_aw = false;
	bool vegan = false;
	bool veg = false;
	bool nodata = false;
	//char* info = "";
	double price = -1.00;
	time_t t;
	struct tm* time_tm;
	char closing_start[255];
	char closing_end[255];
	memset(closing_start, 0, sizeof(closing_start));
	memset(closing_start, 0, sizeof(closing_end));

	ejson_struct* next = ejson;

	while (next) {
		if (!strcmp(next->key, "meal")) {
			meal = next->value;
		} else if (!strcmp(next->key, "dish")) {
			dish = next->value;
		} else if (!strcmp(next->key, "bio")) {
			ejson_get_boolean(ejson, &bio);
		} else if (!strcmp(next->key, "fish")) {
			ejson_get_boolean(ejson, &fish);
		} else if (!strcmp(next->key, "pork")) {
			ejson_get_boolean(ejson, &pork);
		} else if (!strcmp(next->key, "cow")) {
			ejson_get_boolean(ejson, &cow);
		} else if (!strcmp(next->key, "cow_aw")) {
			ejson_get_boolean(ejson, &cow_aw);
		} else if (!strcmp(next->key, "vegan")) {
			ejson_get_boolean(ejson, &vegan);
		} else if (!strcmp(next->key, "veg")) {
			ejson_get_boolean(ejson, &veg);
		} else if (!strcmp(next->key, "info")) {
			//info = next->value;
		} else if (!strcmp(next->key, "nodata")) {
			ejson_get_boolean(ejson, &nodata);
		} else if (!strcmp(next->key, "price_1")) {

			price = strtod(next->value, NULL);
			//printf("price: %s\n", next->value);
			//if (ejson_get_double(ejson, &price) != EJSON_OK) {
			//	printf("Error\n");
			//}
		} else if (!strcmp(next->key, "closing_start")) {
			t = strtoul(next->value, NULL, 10);
			time_tm = localtime(&t);
			strftime(closing_start, sizeof(closing_start), "%Y-%m-%d", time_tm);
		} else if (!strcmp(next->key, "closing_end")) {
			t = strtoul(next->value, NULL, 10);
			time_tm = localtime(&t);
			strftime(closing_end, sizeof(closing_end), "%Y-%m-%d", time_tm);
		}

		next = next->next;
	}

	if (nodata) {
		printf("Geschlossen.\n");
		return;
	}

	if (strlen(meal) < 1) {

		if (closing_start[0] != 0 && closing_end[0] != 0) {
			printf("Linie vom %s bis zum %s geschlossen.\n", closing_start, closing_end);
		} else {
			printf("Essen nicht gefunden.\n");
		}
		return;
	}

	printf("%-50s %.*s", meal, count_str(meal), "                                    ");
	printf("%.2f\n", price);
	if (strlen(dish) > 0) {
		printf("(%s)\n", dish);
	}

}

void print_line(Config* config, ejson_struct* ejson) {

	ejson_struct* next = ejson;
	if (!ejson) {
		return;
	}


	while (next) {
		if (next->child) {
			print_meal(config, next->child);
		}

		next = next->next;
	}

	//printf("%s: %s\n", next->key, next->child->key);
}

ejson_struct* find_json_key(ejson_struct* ejson) {

	while (ejson) {
		if (ejson->key && !strcmp(ejson->key, "timers")) {
			return ejson;
		}
		ejson = ejson->next;
	}

	return NULL;
}


bool print_item(Config* config, ejson_struct* ejson) {

	if (!ejson || ejson->type != EJSON_OBJECT) {
		logprintf(config->log, LOG_ERROR, "json is not an object.\n");
		return false;
	}

	if (!ejson->child) {
		logprintf(config->log, LOG_ERROR, "Object has no childs.\n");
	}

	ejson = ejson->child;

	char* event = "";
	char* message = "";

	struct tm event_time = {0};

	while (ejson) {
		if (!ejson->key) {
			continue;
		} else if (!strcmp(ejson->key, "event")) {
			event = ejson->value;
		} else if (!strcmp(ejson->key, "message")) {
			message = ejson->value;
		} else if (!strcmp(ejson->key, "day")) {
			event_time.tm_mday = strtol(ejson->value, NULL, 10);
		} else if (!strcmp(ejson->key, "month")) {
			event_time.tm_mon = strtol(ejson->value, NULL, 10) - 1;
		} else if (!strcmp(ejson->key, "year")) {
			event_time.tm_year = strtol(ejson->value, NULL, 10) - 1900;
		} else if (!strcmp(ejson->key, "hour")) {
			event_time.tm_hour = strtol(ejson->value, NULL, 10);
		} else if (!strcmp(ejson->key, "minute")) {
			event_time.tm_min = strtol(ejson->value, NULL, 10);
		} else if (!strcmp(ejson->key, "second")) {
			event_time.tm_sec = strtol(ejson->value, NULL, 10);
		}
		ejson = ejson->next;
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

	char* word = NULL;
	if (diff < 60) {
		word = words[0];
	} else if (diff < 60 * 60) {
		diff /= 60;
		word = words[1];
	} else if (diff < 60 * 60 * 24) {
		diff /= 60 * 60 * 24;
		word = words[2];
	} else {
		diff /= 60 * 60 * 24;
		word = words[3];
	}

	printf("%s%s\n%s%zd %s left\n", config->pre, event, asctime(&event_time), diff, word);
	if (act - now < 0 && strlen(message) > 0) {
		printf(" (%s)", message);
	}
	printf("%s", config->post);

	fflush(stdout);
	return true;
}

void print_list(Config* config, ejson_struct* ejson) {

	if (!ejson) {
		return;
	}

	ejson = ejson->child;

	ejson = find_json_key(ejson);

	if (!ejson) {
		logprintf(config->log, LOG_ERROR, "Something is wrong with the json (Cannot find timers key).\n");
		return;
	}

	if (!ejson->child) {
		logprintf(config->log, LOG_ERROR, "Timers has no children.\n");
		return;
	}
	ejson_struct* next = ejson->child;
	int i = 0;

	while (next) {

		if (!print_item(config, next)) {
			logprintf(config->log, LOG_ERROR, "Cannot print item.\n");
			return;
		}
		sleep(10);
		next = next->next;
		i++;

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

		ejson_struct* ejson = NULL;

		if (ejson_parse(&ejson, config->data->memory) != EJSON_OK) {
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
		.verbosity = 0
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
