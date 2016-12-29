/**
 * @file mysql.c
 * @author Snyo
 */
#include "plugins/mysql.h"
#include "plugin.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <mysql/mysql.h>

#include "metadata.h"
#include "packet.h"
#include "sender.h"
#include "util.h"

#define MYSQL_TICK 4.973F

typedef struct mysql_module_t {
    unsigned on : 1;

    char host[BFSZ];
    char user[BFSZ];
    char pass[BFSZ];
    unsigned int port;

	MYSQL *mysql;
    unsigned long tid;

} mysql_module_t;

int mysql_prep(void *_m);
int mysql_fini(void *_m);
int mysql_gather(void *_p, packet_t *pkt);

int _mysql_gather_crud(mysql_module_t *m, packet_t *pkt);
int _mysql_gather_query(mysql_module_t *m, packet_t *pkt);
int _mysql_gather_innodb(mysql_module_t *m, packet_t *pkt);
int _mysql_gather_thread(mysql_module_t *m, packet_t *pkt);
int _mysql_gather_replica(mysql_module_t *m, packet_t *pkt);

MYSQL_RES *query_result(MYSQL *mysql, const char *query);

int load_mysql_module(plugin_t *p, int argc, char *argv[]) {
    if(!p || argc!=1) return -1;

    mysql_module_t *m = malloc(sizeof(mysql_module_t));
    if(!m) return -1;

    if(sscanf((char *)(argv+0), "%128[^/]/%u/%128[^/]/%128[^/]\n", m->host, &m->port, m->user, m->pass) != 4) {
        free(m);
        return -1;
    }
    m->on = 1;

    p->tick = MYSQL_TICK;
    if(strcmp(m->host, "localhost") && strcmp(m->host, "127.0.0.1"))
        p->tip  = m->host;
    p->type = "mysql_ubuntu_1.0";

    p->prep = mysql_prep;
	p->fini = mysql_fini;
	p->gather = mysql_gather;
    
    p->module = m;

	return 0;
}

int mysql_prep(void *_m) {
    mysql_module_t *m = _m;

    if(!(m->mysql = mysql_init(NULL))) {
        return -1;
    }

    my_bool b = 1;
    if(0x00 || mysql_options(m->mysql, MYSQL_OPT_RECONNECT, &b) < 0
            || !mysql_real_connect(m->mysql, m->host, m->user, m->pass, NULL, m->port, NULL, 0)) {
        return -1;
    }
    
    m->on = 1;
    m->tid = mysql_thread_id(m->mysql);

    return 0;
}

int mysql_fini(void *_m) {
	if(!_m) return -1;

    mysql_module_t *m = _m;
    if(m->mysql)
        mysql_close(m->mysql);
    free(m);

    return 0;
}

int mysql_gather(void *_m, packet_t *pkt) {
    mysql_module_t *m = _m;
    if(mysql_ping(m->mysql)) {
        if(m->on) {
            m->on = 0;
            return EPLUGDOWN;
        } else {
            return ENODATA;
        }
    } else if(m->tid != mysql_thread_id(m->mysql)) {
        m->tid = mysql_thread_id(m->mysql);
        m->on = 1;
        return EPLUGUP;
    }

    return packet_gather(pkt, "curd",    _mysql_gather_crud, m)
        & packet_gather(pkt, "query",   _mysql_gather_query, m)
        & packet_gather(pkt, "innodb",  _mysql_gather_innodb, m)
        & packet_gather(pkt, "thread",  _mysql_gather_thread, m)
        & packet_gather(pkt, "replica", _mysql_gather_replica, m);
}

int _mysql_gather_crud(mysql_module_t *m, packet_t *pkt) {
	MYSQL_RES *res;
	MYSQL_ROW row;

    int error = ENODATA;
    int comma = 0;

    res = query_result(m->mysql, "show global status where variable_name in ('com_select','com_insert','com_update','com_delete');");
    if(res) {
        error = ENONE;
        while((row = mysql_fetch_row(res))) {
            packet_append(pkt, "%s\"%s\":%s", comma?",":"", row[0]+4, row[1]);
            comma = 1;
        }
        mysql_free_result(res);
    }
    
    res = query_result(m->mysql, "select 'alter' name, sum(variable_value) value from information_schema.global_status where variable_name like 'com_alter%' union all select 'create' name, sum(variable_value) value from information_schema.global_status where variable_name like 'com_create%' union all select 'drop' name, sum(variable_value) value from information_schema.global_status where variable_name like 'com_drop%';");
    if(res) {
        error = ENONE;
        while((row = mysql_fetch_row(res))) {
            packet_append(pkt, "%s\"%s\":%s", comma?",":"", row[0], row[1]);
            comma = 1;
        }
        mysql_free_result(res);
    }

    return error;
}

int _mysql_gather_query(mysql_module_t *m, packet_t *pkt) {
	MYSQL_RES *res;
	MYSQL_ROW row;

    int error = ENODATA;

    res = query_result(m->mysql, "show global status where variable_name in ('slow_queries');");
    if(res) {
        error = ENONE;
        while((row = mysql_fetch_row(res)))
            packet_append(pkt, "\"slow queries\":%s", row[1]);
        mysql_free_result(res);
    }
    /*
	res = query_result(((mysql_m_t *)p->m)->mysql, \
			"show global variables where variable_name in (\
		  'slow_query_log',\
			'slow_query_log_file'\
			);");
	int slow_query_log = 0;
	char slow_query_log_file[100];
	while((row = mysql_fetch_row(res))) {
		if(strncmp("slow_query_log_file", row[0], 19) == 0) {
			strncpy(slow_query_log_file, row[1], 100);
		} else if(strncmp("slow_query_log", row[0], 14) == 0) {
			slow_query_log = strncmp("ON", row[1], 2)==0;
		}
	}
	if(slow_query_log) {
		FILE *log = fopen(slow_query_log_file, "r");
		if(log) fclose(log);
	}
    */
    res = query_result(m->mysql, "select user_host,ifnull(sql_text, ''),TIME_TO_SEC(query_time),concat(UNIX_TIMESTAMP(start_time),'000'),rows_sent,rows_examined from mysql.slow_log where start_time>=now()-interval 30 minute and sql_text not like '%%#exem_moc#%%' limit 100;");
    if(!res) return error;
    int k = 0;
    struct {
        char user[BFSZ];
        char sql_text[BFSZ];
        unsigned long long query_time;
        epoch_t start_time;
        unsigned long rows_sent;
        unsigned long rows_examined;
    } slow_queries[BFSZ];

    while ((row = mysql_fetch_row(res))) {
        sscanf(row[0], "%s", slow_queries[k].user);
        sscanf(row[1], "%s", slow_queries[k].sql_text);
        sscanf(row[2], "%llu", &slow_queries[k].query_time);
        sscanf(row[3], "%llu", &slow_queries[k].start_time);
        sscanf(row[4], "%lu", &slow_queries[k].rows_sent);
        sscanf(row[5], "%lu", &slow_queries[k].rows_examined);
        k++;
    }
	mysql_free_result(res);

    if(k == 0) return error;

    packet_append(pkt, ",\"user\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", slow_queries[k].user);
    packet_append(pkt, "],\"sql\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", slow_queries[k].sql_text);
    packet_append(pkt, "],\"query_time\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", slow_queries[k].query_time);
    packet_append(pkt, "],\"start_time\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", slow_queries[k].start_time);
    packet_append(pkt, "],\"rows_sent\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%lu", i?",":"", slow_queries[k].rows_sent);
    packet_append(pkt, "],\"rows_examined\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%lu", i?",":"", slow_queries[k].rows_examined);
    packet_append(pkt, "]");

    return error;
}

int _mysql_gather_innodb(mysql_module_t *m, packet_t *pkt) {
	MYSQL_RES *res;
    MYSQL_ROW row;

    int error = ENODATA;

    res = query_result(m->mysql, "show global variables where variable_name in ('innodb_buffer_pool_size');");
    if(res) {
        error = ENONE;
        while((row = mysql_fetch_row(res))) {
            packet_append(pkt, "\"%s\":", row[0]+14);
            packet_append(pkt, "%s", row[1]);
        }
        mysql_free_result(res);
    }

	res = query_result(m->mysql, "show global status where variable_name in ('innodb_buffer_pool_pages_dirty','innodb_buffer_pool_pages_free','innodb_buffer_pool_pages_data','innodb_buffer_pool_read_requests','innodb_buffer_pool_reads','innodb_buffer_pool_write_requests','innodb_pages_created','innodb_pages_read','innodb_pages_written');");
    if(res) {
        error = ENONE;
        while((row = mysql_fetch_row(res))) {
            packet_append(pkt, ",\"%s\":", row[0]+7);
            packet_append(pkt, "%s", row[1]);
        }
        mysql_free_result(res);
    }

    res = query_result(m->mysql, "show engine innodb status;");
    if(!res) return error;
    row = mysql_fetch_row(res);
    char *match = "Ibuf";
    unsigned long long total, free, used;
    for(int i=0; i<strlen(row[2]); ++i) {
        if(strncmp(row[2]+i, match, 4) == 0)
            if(sscanf(row[2]+i, "Ibuf: size %llu, free list len %llu, seg size %llu", &used, &free, &total) == 3) {
                error = ENONE;
                packet_append(pkt, ",\"cell_count\":%llu", total);
                packet_append(pkt, ",\"free_cells\":%llu", free);
                packet_append(pkt, ",\"used_cells\":%llu", used);
            }
    }
    mysql_free_result(res);

    return error;
}

int _mysql_gather_thread(mysql_module_t *m, packet_t *pkt) {
	MYSQL_RES *res;
    MYSQL_ROW row;

    int error = ENODATA;
    int comma = 0;

    res = query_result(m->mysql, "show global status where variable_name in ('connections','threads_connected','threads_running');");
    if(res) {
        error = ENONE;
        while((row = mysql_fetch_row(res))) {
            packet_append(pkt, "%s\"%s\":%s", comma?",":"", row[0], row[1]);
            comma = 1;
        }
        mysql_free_result(res);
    }

    res = query_result(m->mysql, "select a.id,ifnull(b.thread_id,''),ifnull(a.info,''),ifnull(a.user,''),ifnull(a.host,''),ifnull(a.db,''),a.time,ifnull(round(c.timer_wait/1000000000000,3),''),ifnull(c.event_id,''),ifnull(c.event_name,''),a.command,a.state from information_schema.processlist a left join performance_schema.threads b on a.id=b.processlist_id left join performance_schema.events_waits_current c on b.thread_id=c.thread_id where 1=1 and (a.info is null or a.info not like '%#exem_moc#%')");

    int k = 0;
    struct {
        unsigned long id;
        unsigned long thread_id;
        char info[BFSZ];
        char user[BFSZ];
        char host[BFSZ];
        char db[BFSZ];
        unsigned long long time;
        unsigned long long timer_wait;
        unsigned long event_id;
        char event_name[BFSZ];
        char command[BFSZ];
        char state[BFSZ];
    } threads[BFSZ];

    if(!res) return error;
    while ((row = mysql_fetch_row(res))) {
        sscanf(row[0], "%lu", &threads[k].id);
        sscanf(row[1], "%lu", &threads[k].thread_id);
        sscanf(row[2], "%s", threads[k].info);
        sscanf(row[3], "%s", threads[k].user);
        sscanf(row[4], "%s", threads[k].host);
        sscanf(row[5], "%s", threads[k].db);
        sscanf(row[6], "%llu", &threads[k].time);
        sscanf(row[7], "%llu", &threads[k].timer_wait);
        sscanf(row[8], "%lu", &threads[k].event_id);
        sscanf(row[9], "%s", threads[k].event_name);
        sscanf(row[10], "%s", threads[k].command);
        sscanf(row[11], "%s", threads[k].state);
        k++;
    }
    mysql_free_result(res);

    if(k == 0) return error;

    packet_append(pkt, "\"id\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%lu\"", i?",":"", threads[k].id);
    packet_append(pkt, "],\"thread_id\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%lu\"", i?",":"", threads[k].thread_id);
    packet_append(pkt, "],\"info\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", threads[k].info);
    packet_append(pkt, "],\"user\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", threads[k].user);
    packet_append(pkt, "],\"host\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", threads[k].host);
    packet_append(pkt, "],\"db\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", threads[k].db);
    packet_append(pkt, "],\"time\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%llu\"", i?",":"", threads[k].time);
    packet_append(pkt, "],\"timer_wait\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%llu\"", i?",":"", threads[k].timer_wait);
    packet_append(pkt, "],\"event_id\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%lu\"", i?",":"", threads[k].event_id);
    packet_append(pkt, "],\"event_name\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", threads[k].event_name);
    packet_append(pkt, "],\"command\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", threads[k].command);
    packet_append(pkt, "],\"state\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", threads[k].state);
    packet_append(pkt, "]");

    return error;
}

int _mysql_gather_replica(mysql_module_t *m, packet_t *pkt) {
    return ENODATA;
}

MYSQL_RES *query_result(MYSQL *mysql, const char *query) {
	if(mysql_query(mysql, query)) return NULL;
	return mysql_store_result(mysql);
}
