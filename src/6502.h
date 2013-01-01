/*
 *
 */

#ifndef _6502_H_
#define _6502_H_

extern void cpu_init(void);
extern void cpu_deinit(void);
extern uint8_t cpu_execute(void);

typedef struct cpu_t_struct {
    uint8_t p;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint16_t ip;
    uint8_t sp;
    uint8_t irq;
} cpu_t;

extern cpu_t cpu_state;

#define FLAG_N  0x80
#define FLAG_V  0x40
#define FLAG_UNUSED 0x20
#define FLAG_B  0x10
#define FLAG_D  0x08
#define FLAG_I  0x04
#define FLAG_Z  0x02
#define FLAG_C  0x01

#define FLAG_IRQ 0x01
#define FLAG_NMI 0x02

#endif /* _6502_H_ */
