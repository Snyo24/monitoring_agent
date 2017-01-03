/**
 * @file storage.h
 * @author Snyo 
 * @brief Send packets
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "routine.h"
#include "packet.h"

int storage_init(routine_t *storage);
int storage_fini(routine_t *storage);

packet_t *get_packet(enum packet_type type);

#endif
