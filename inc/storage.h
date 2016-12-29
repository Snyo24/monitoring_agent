/**
 * @file storage.h
 * @author Snyo 
 * @brief Send packets
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "runnable.h"
#include "packet.h"

int storage_init(runnable_t *storage);
int storage_fini(runnable_t *storage);

#endif
