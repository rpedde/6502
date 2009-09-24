/*
 *
 */

#ifndef _MEMORY_H_
#define _MEMORY_H_

/* The actual reading and writing of RAM/ROM should be done by
   a driver, as this is hardware, not cpu dependent */

#define E_MEM_SUCCESS 0
#define E_MEM_ALLOC   1


extern int memory_init(void);
extern void memory_deinit(void);

extern uint8_t memory_read(uint16_t addr);
extern void memory_write(uint16_t addr, uint8_t data);
extern int memory_load(uint16_t addr, char *file);

#endif /* _MEMORY_H_ */
