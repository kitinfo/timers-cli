#pragma once
#include "logger.h"

int get_request(LOGGER log, const char* url, void* write_func, void* data, char* user, char* password);
