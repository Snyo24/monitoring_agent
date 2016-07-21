#ifndef _MYSQLC_H_
#define _MYSQLC_H_

#include <mysql.h>

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS "alcls7856@"
#define DB_NAME "snyo"

void *mysql_routine(void *_agent);

size_t getResultByQuery(void *buf, char *query, MYSQL *mysql);

#endif