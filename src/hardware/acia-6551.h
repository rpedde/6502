/*
 * Copyright (C) 2013 Ron Pedde <ron@pedde.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _16550_H_
#define _16550_H_

/* CR is don't care.. not setting baud rate */

#define CMD_PAR2   0x80
#define CMD_PAR1   0x40
#define CMD_PAR0   0x20
#define CMD_EHCO   0x10  /* 0: normal, 1: echo */
#define CMD_TXCTL1 0x08  /* 00: txirq disabled, rts high, 01: txirq enabled, rts low */
#define CMD_TXCTL0 0x04  /* 10: txirq disabled, rts low, 11: txirq disabled, rts low, send brk */
#define CMD_RXIE   0x02  /* 0: rx irq enabled from bit 0 of status reg */
#define CMD_DTR    0x01  /* 0: disable dtr (dtr high), 1 enable dtr (dtr low) */

#define SR_IRQ     0x80  /* 1 if irq has occurred */
#define SR_DSR     0x40
#define SR_DCD     0x20
#define SR_TDRE    0x10  /* transmit data reg empty */
#define SR_RDRF    0x08  /* byte waiting for input */
#define SR_OR      0x04  /* 1: overrun has occurred */
#define SR_FE      0x02  /* 1: framing error */
#define SR_PE      0x01  /* 1: parity error */


#define REG_TDR 0
#define REG_RDR 0
#define REG_PR  1
#define REG_SR  1
#define REG_CMD 2
#define REG_CTL 3

#endif /* _16550_H_ */
