#include "unsent.h"

#include <stdio.h>
#include <sys/stat.h>

#include <zlog.h>

#include "util.h"

#define UNSENT_SENDING "log/UNSENT_SENDING"

#define UNSENT_BEGIN -1
#define UNSENT_END   1

#define zlog_unsent(cat, format, ...) zlog(cat,__FILE__,sizeof(__FILE__)-1,__func__,sizeof(__func__)-1,__LINE__,19,format,##__VA_ARGS__)

int  unsent_clear();
int  unsent_load();
void unsent_drop_sending();
char *unsent_file(int i);

void *unsent_tag;
char unsent_path[BFSZ] = "log";
char unsent_name[BFSZ];
char unsent_end[BFSZ];

FILE *unsent_sending_fp;
int  unsent_json_loaded;
char unsent_json[32768];

int file_exist(char *filename) {
	struct stat st;
	return !stat(filename, &st);
}

int unsent_init() {
	//DEBUG(zlog_debug(usent_tag, "Clear old data"));
    unsent_tag = zlog_get_category("unsent");
	snprintf(unsent_end, 50, "%s/unsent.%d", unsent_path, UNSENT_END);
	if(unsent_clear() < 0) return -1;
		//zlog_warn(usent_tag, "Fail to clear old data");

    return 0;
}

int unsent_clear() {
	int success = 0;
	for(int i=UNSENT_BEGIN; i<=UNSENT_END; ++i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent))
			success |= remove(unsent);
	}
	if(file_exist(UNSENT_SENDING))
		success |= remove(UNSENT_SENDING);

	return !success - 1;
}

int unsent_load() {
	if(!file_exist(unsent_file(UNSENT_BEGIN)) && !file_exist(unsent_file(0)))
		return -1;
	for(int i=UNSENT_END; i>=UNSENT_BEGIN; --i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent)) {
			if(rename(unsent, UNSENT_SENDING))
				return -1;
			unsent_sending_fp = fopen(UNSENT_SENDING, "r");
			return (unsent_sending_fp != NULL)-1;
		}
	}
	return -1;
}

void unsent_drop_sending() {
	fclose(unsent_sending_fp);
	unsent_sending_fp = NULL;
	unsent_json_loaded = 0;
	remove(UNSENT_SENDING);
}

char *unsent_file(int i) {
	if(i >= 0) {
		snprintf(unsent_name, 50, "%s/unsent.%d", unsent_path, i);
	} else {
		snprintf(unsent_name, 50, "%s/unsent", unsent_path);
	}
	return unsent_name;
}

void unsent_send() {
    if(!unsent_sending_fp) {
        if(unsent_load() < 0) return;
    } else if(file_exist(unsent_end)) {
        unsent_drop_sending();
        if(unsent_load() < 0) return;
    }

    //DEBUG(zlog_debug(usent_tag, "POST unsent JSON"));
    while(unsent_json_loaded || fgets(unsent_json, 32768, unsent_sending_fp)) {
        unsent_json_loaded = 1;
        //if(sender_post(sndr, unsent_json, METRIC) < 0) {
            //zlog_error(usent_tag, "POST unsent fail");
            //return;
        //}
        unsent_json_loaded = 0;
    }
    //zlog_error(usent_tag, "Fail to get unsent JSON");
    unsent_drop_sending();
}
