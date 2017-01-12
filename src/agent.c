#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <pthread.h>

#include <zlog.h>

#include "routine.h"
#include "metadata.h"
#include "sparser.h"
#include "scheduler.h"
#include "storage.h"
#include "plugin.h"
#include "util.h"

#define MAX_PLUGINS 5

int      pluginc;
plugin_t *plugins[MAX_PLUGINS] = {0};

int main(int argc, char **argv) {
    if(zlog_init(".zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(1);
    }

    zlog_category_t *tag = zlog_get_category("main");

    /* Metadata */
    if(metadata_init() < 0) {
        DEBUG(zlog_error(tag, "Fail to initialize metadata"));
        exit(1);
    }

    /* Plugins */
    if(!(pluginc = sparse("plugin.conf", plugins))) {
        DEBUG(zlog_error(tag, "No plugin found"));
        exit(1);
    }

    DIR *dirp = opendir("res");
    if(dirp) {
        struct dirent *ep;
        while((ep = readdir(dirp)) != NULL) {
            if(ep->d_ino != 0 && strcmp(ep->d_name, ".") && strcmp(ep->d_name, "..")) {
                char plugin_id[BFSZ];
                snprintf(plugin_id, BFSZ, "res/%s", ep->d_name);
                FILE *fp = fopen(plugin_id, "rb");
                if(!fp) continue;

                int size;
                char type[BFSZ];
                void *module;
                if(fread(&size, 1, sizeof(size), fp) != sizeof(size)) {
                    fclose(fp);
                    continue;
                }

                if(fread(type, 1, size, fp) != size) {
                    fclose(fp);
                    continue;
                }
                type[size] = '\0';

                if(fread(&size, 1, sizeof(size), fp) != sizeof(size)) {
                    fclose(fp);
                    continue;
                }

                module = malloc(size);
                if(fread(module, 1, size, fp) != size) {
                    fclose(fp);
                    free(module);
                    continue;
                }
                fclose(fp);

                for(int i=0; i<pluginc; i++) {
                    plugin_t *p = plugins[i];
                    if(!strcmp(p->type, type) && !p->cmp(module, (void *)p->module, size)) {
                        unsigned long long x = 0;
                        for(int j=0; j<strlen(ep->d_name); j++)
                            x = x * 10 + ep->d_name[j] - '0';
                        p->tid = x;
                        break;
                    }
                }
                free(module);
            }
        }
        closedir(dirp);
    }

    /* Scheduler & Sender */
    routine_t sch, st;

    scheduler_init(&sch);
    storage_init(&st);

    routine_start(&sch);
    routine_start(&st);

    /* Start plugins */
    for(int i=0; i<pluginc; i++) {
        void *p = plugins[i];
        if(p) routine_start(p);
    }

    while(routine_alive(&sch) && routine_alive(&st)) {
        if(routine_overdue(&sch))
            if(routine_ping(&sch) < 0)
                routine_restart(&sch);
        if(routine_overdue(&st))
            routine_ping(&st);
        snyo_sleep(0.07);
    }

    /* Finalize */
    scheduler_fini(&sch);
    storage_fini(&st);

    zlog_fini();
    
    return 0;
}
