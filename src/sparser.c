#include "sparser.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <zlog.h>

#include "plugin.h"
#include "util.h"

int skip_until(FILE *conf, const char c) {
    void *err;
    char line[BFSZ];
    while((err = fgets(line, BFSZ, conf)) && line[0] != c);
    if(feof(conf)) return 2;
    if(!err) return 1;
    return 0;
}

int sparse(const char *filename, plugin_t **plugins) {
    int n = 0;

    zlog_category_t *tag = zlog_get_category("parser");

    DEBUG(zlog_debug(tag, "Parse %s", filename));

    FILE *conf = fopen(filename, "r");
    if(!conf) {
		zlog_error(tag, "Fail to open conf");
		return -1;
	}
    enum {
        NONE,
        PLUG,
        NAME,
        VAL
    } status = NONE;

	char line[BFSZ];

    char type[BFSZ];
    char dlname[BFSZ], smname[BFSZ];
    int argc = 0;
    char argv[10][BFSZ];
    void *dl;
    int (*load_module)(plugin_t *, int, char**) = 0;

    while(fgets(line, BFSZ, conf)) {
        char first, remain[BFSZ];
        sscanf(line, "%c%s", &first, remain);
        if(first == '\n' || first == '#') {
        } else if(first == '<') {
            if(status != NONE) {
                zlog_error(tag, "Unexpected character '<'");
                return -1;
            }
            argc = 0;

            status = PLUG;
            snprintf(type, BFSZ, "%s", remain);
            snprintf(dlname, BFSZ, "lib%s.so", type);
            snprintf(smname, BFSZ, "load_%s_module", type);
            if(0x00 || !(dl = dlopen(dlname, RTLD_LAZY | RTLD_NODELETE))
                    || !(load_module = dlsym(dl, smname))) {
                zlog_error(tag, "Cannot %s (from %s)", smname, dlname);
                switch(skip_until(conf, '>')) {
                case 1:
                    return -1;
                case 2:
                    zlog_error(tag, "Missing '>'");
                }
                status = NONE;
            } else {
                dlclose(dl);
            }
        } else if(first == '-') {
            if(status == PLUG) {
                if(strncmp(remain, "OFF", 3) == 0) {
                    switch(skip_until(conf, '>')) {
                    case 1:
                        return -1;
                    case 2:
                        zlog_error(tag, "Missing '>'");
                    }
                    status = NONE;
                } else if(strncmp(remain, "ON", 2) != 0) {
                    zlog_error(tag, "Status for %s is unknown: %s", type, remain);
                    return -1;
                }
            } else if(status == NAME) {
                snprintf(argv[argc++], BFSZ, "%s", remain);
                status = VAL;
            } else if(status == VAL) {
                snprintf(argv[argc++], BFSZ, "%s", remain);
            } else {
                zlog_error(tag, "Unexpected character '-'");
                return -1;
            }
        } else if(first == '!') {
            if(status != PLUG) {
                zlog_error(tag, "Unexpected character '!'");
                return -1;
            }
            status = NAME;
        } else if(first == '>') {
            if(status != VAL && status != PLUG) {
                zlog_error(tag, "Unexpected character '>'");
                return -1;
            }
            status = NONE;
            plugin_t *p = malloc(sizeof(plugin_t));
            memset(p, 0, sizeof(plugin_t));
            if(!p || plugin_init(p) < 0) {
                free(p);
                continue;
            }
            if(load_module(p, argc, (char **)argv) < 0) {
                free(p);
                continue;
            }
            plugins[n++] = p;
        } else {
            zlog_error(tag, "Unexpected character");
            return -1;
        }
    }
    fclose(conf);

    return n;
}
