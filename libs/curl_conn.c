#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include "logger.h"

CURL* c_init(LOGGER log, const char* url) {
	CURL* curl = curl_easy_init();

	if (!curl) {
		logprintf(log, LOG_ERROR, "Cannot init curl.\n");
		return NULL;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	//curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/46.0.2490.86 Safari/537.36");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	return curl;
}

int curl_perform(LOGGER log, CURL* curl) {
	CURLcode res = curl_easy_perform(curl);

	/* Check for errors */ 
	if (res != CURLE_OK) {
		logprintf(log, LOG_ERROR, "curl_easy_perform() failed: %s\n",
		curl_easy_strerror(res));
		return 0;
	}

	return 1;
}

int get_request(LOGGER log, const char* url, void* write_func, void* data, char* user, char* password) {

	CURL* curl = c_init(log, url);

	if (!curl) {
		return 0;
	}

	if (user && password) {
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_USERNAME, user);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_func);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);

	if (!curl_perform(log, curl)) {
		curl_easy_cleanup(curl);
		return 0;
	}

	curl_easy_cleanup(curl);

	return 1;
}
