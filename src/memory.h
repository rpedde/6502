/*
 *
 */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "hardware.h"

/* The actual reading and writing of RAM/ROM should be done by
   a driver, as this is hardware, not cpu dependent */

#define E_MEM_SUCCESS 0
#define E_MEM_ALLOC   1
#define E_MEM_FOPEN   2

extern int memory_init(void);
extern void memory_deinit(void);

extern uint8_t memory_read(uint16_t addr);
extern void memory_write(uint16_t addr, uint8_t data);
extern int memory_load(const char *module, hw_config_t *config);

#endif /* _MEMORY_H_ */
