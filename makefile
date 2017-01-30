LDFLAGS=-lcurl
CFLAGS=-g -Wall

timers-cli: libs/easy_args.c libs/curl_conn.c libs/logger.c libs/easy_json.c

clean:
	rm timers-cli
