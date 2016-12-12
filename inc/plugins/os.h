/**
 * @file os.h
 * @author Snyo
 */
#ifndef _OS_H_
#define _OS_H_

void *os_target_init(int argc, char **argv);
void os_target_fini(void *_target);

#endif
