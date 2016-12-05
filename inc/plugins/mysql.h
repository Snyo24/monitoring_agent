/**
 * @file mysql.h
 * @author Snyo
 */
#ifndef _MYSQL_H_
#define _MYSQL_H_

void *mysql_plugin_init(int argc, char **argv);
void mysql_plugin_fini(void *_plugin);

#endif
