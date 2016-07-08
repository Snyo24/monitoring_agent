#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <mysql.h>
#include <curl.h>
#include <json.h>

#define DB_HOST "127.0.0.1"
#define DB_USER "snyotest"
#define DB_PASS "snyo"
#define DB_NAME "snyo" // NULL is ok. WHY?

#define MAX_DATA 500

enum metricType {
	GAUGE,
	RATE,
};

struct metricValue {
	char *name;
	time_t ts;
	union {
		double value_DOUBLE;
	};
	enum metricType type;
};

char *collectingMetricsAs[] = {
    "mysql.net.connections",
    "mysql.net.max_connections", 
    "mysql.performance.open_files", 
    "mysql.performance.table_locks_waited", 
    "mysql.performance.threads_connected", 
    "mysql.performance.threads_running", 
    "mysql.innodb.data_reads",
    "mysql.innodb.data_writes",
    "mysql.innodb.os_log_fsyncs",
    "mysql.performance.slow_queries",
    "mysql.performance.questions",
    "mysql.performance.queries",
    "mysql.performance.com_select",
    "mysql.performance.com_insert",
    "mysql.performance.com_update",
    "mysql.performance.com_delete",
    "mysql.performance.com_insert_select",
    "mysql.performance.com_update_multi",
    "mysql.performance.com_delete_multi",
    "mysql.performance.com_replace_select",
    "mysql.performance.qcache_hits",
    "mysql.innodb.mutex_spin_waits",
    "mysql.innodb.mutex_spin_rounds",
    "mysql.innodb.mutex_os_waits",
    "mysql.performance.created_tmp_tables",
    "mysql.performance.created_tmp_disk_tables",
    "mysql.performance.created_tmp_files",
    "mysql.innodb.row_lock_waits",
    "mysql.innodb.row_lock_time",
    "mysql.innodb.current_row_locks", 
    "mysql.performance.open_tables"
};

char *collectingMetrics[] = {
	"Connections",
	"Max_used_connections",
	"Open_files",
	"Table_locks_waited",
	"Threads_connected",
	"Threads_running",
	"Innodb_data_reads",
	"Innodb_data_writes",
	"Innodb_os_log_fsyncs",
	"Slow_queries",
	"Questions",
	"Queries",
	"Com_select",
	"Com_insert",
	"Com_update",
	"Com_delete",
	"Com_insert_select",
	"Com_update_multi",
	"Com_delete_multi",
	"Com_replace_select",
	"Qcache_hits",
	"Innodb_mutex_spin_waits",
	"Innodb_mutex_spin_rounds",
	"Innodb_mutex_os_waits",
	"Created_tmp_tables",
	"Created_tmp_disk_tables",
	"Created_tmp_files",
	"Innodb_row_lock_waits",
	"Innodb_row_lock_time",
	"Innodb_current_row_locks",
	"Open_tables"
};

enum metricType collectingTypes[] = {
	RATE,
	GAUGE,
	GAUGE,
	GAUGE,
	GAUGE,
	GAUGE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	RATE,
	GAUGE,
	GAUGE
};

void timer(int sig) {
	printf("%d\n", sig);
	alarm(1);
}

int main() {
	MYSQL *mysql = NULL, con;
	MYSQL_RES *res;
	MYSQL_ROW row;
	signal(SIGALRM, timer);
	alarm(1);
	int query_stat;

	mysql_init(&con);

	mysql = mysql_real_connect(&con, DB_HOST, DB_USER, DB_PASS, DB_NAME, 3306, (char *)NULL, 0);

	if(mysql == NULL) {
		perror("CONNECTION FAILED");
		exit(1);
	}

	query_stat = mysql_query(mysql, "SHOW STATUS");
	if(query_stat != 0) {
		perror("QUERY ERROR");
		exit(2);
	}

	res = mysql_store_result(mysql);

	int key_blocks_unused=0;
	while((row = mysql_fetch_row(res))) {
		if(strcmp(row[0], "Key_blocks_unused") == 0)
			key_blocks_unused = atoi(row[1]);
		else for(int i=0; i<sizeof(collectingMetrics)/sizeof(char *); i++) {
			if(strcmp(row[0], collectingMetrics[i]) == 0) {
				if(atoi(row[1]) != 0) {
					printf(" (\'%s\',\n  %ld,\n  %s,\n  {\'hostname\': \'%s\', \'type\': \'%s\'}),\n",
							collectingMetricsAs[i], time(0), row[1],
							DB_USER, collectingTypes[i]==GAUGE?"gauge":"rate");
				}
				break;
			}
		}
	}

	query_stat = mysql_query(mysql, "SHOW VARIABLES LIKE \'key%%\';");
	if(query_stat != 0) {
		perror("QUERY ERROR");
		exit(2);
	}
	res = mysql_store_result(mysql);

	int key_cache_block_size=0;
	int key_buffer_size=0;
	while((row = mysql_fetch_row(res))) {
         if(strcmp(row[0], "key_cache_block_size") == 0)
			key_cache_block_size = atoi(row[1]);
         else if(strcmp(row[0], "key_buffer_size") == 0)
			key_buffer_size = atoi(row[1]);
	}
    double key_cache_utilization = 1 - ((double)(key_blocks_unused * key_cache_block_size) / key_buffer_size);
	printf(" (\'%s\',\n  %ld,\n  %f,\n  {\'hostname\': \'%s\', \'type\': \'%s\'})\n",
			"mysql.performance.key_cache_utilization", time(0), key_cache_utilization,
			DB_USER, "gauge");

	mysql_free_result(res);
	mysql_close(mysql);
	char host_name[80];
	int n = gethostname(host_name, sizeof(host_name));
	printf("%s\n", host_name);
	while(1);
	return 0;
}