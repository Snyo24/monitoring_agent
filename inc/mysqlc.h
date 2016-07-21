#ifndef _MYSQLC_H_
#define _MYSQLC_H_

#include "snyohash.h"
#include <mysql.h>

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS "alcls7856@"
#define DB_NAME "snyo"

void *mysql_routine(void *_agent);

void get_mysql_metadata(MYSQL *mysql, hash_t *hash_table);
void get_CRUD_statistics(MYSQL *mysql, hash_t *metric_table);
MYSQL_RES * query_result(MYSQL *mysql, char *query);

#endif