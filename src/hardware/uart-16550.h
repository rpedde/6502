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

#define IER_ERBFI   0x01 /* enable recieved data available */
#define IER_ETBEI   0x02 /* enable transmit holding reg empty */
#define IER_ELSI    0x04 /* enable receiver line status */
#define IER_EDSSI   0x08 /* enable modem status interrupt */

#define IIR_PENDING        0x01
#define IIR_INTERRUPT_MASK 0x0E
#define IIR_FIFOS_ENABLED1 0x40
#define IIR_FIFOS_ENABLED2 0x80

#define FCR_FIFO_ENABLE     0x01
#define FCR_RCVR_FIFO_RESET 0x02
#define FCR_TXVR_FIFO_RESET 0x04
#define FCR_DMA_SELECT      0x08
#define FCR_RCVR_TRIGGER_L  0x40
#define FCR_RCVR_TRIGGER_M  0x80

#define LCR_WLS0    0x01 /* word length select bit 0 */
#define LCR_WLS1    0x02 /* word length select bit 1 */
#define LCR_STB     0x04 /* stop bits */
#define LCR_PEN     0x08 /* parity enable */
#define LCR_EPS     0x10 /* even parity select */
#define LCR_SP      0x20 /* stick parity ?!? */
#define LCR_SBRK    0x40 /* set break */
#define LCR_DLAB    0x80 /* divisor latch access bit */

#define MCR_DTR     0x01
#define MCR_RTS     0x02
#define MCR_OUT1    0x04
#define MCR_OUT2    0x08
#define MCR_LOOP    0x10

#define LSR_DR      0x01 /* data ready */
#define LSR_OE      0x02 /* overrun enable */
#define LSR_PE      0x04 /* parity error */
#define LSR_FE      0x08 /* framing error */
#define LSR_BI      0x10 /* break interrupt */
#define LSR_THRE    0x20 /* transmittter holding register */
#define LSR_TEMT    0x40 /* transmitter empty */
#define LSR_ERF     0x80 /* error in recvr fifo */

#define MSR_DCTS    0x01 /* delta clear to send */
#define MSR_DDSR    0x02 /* delta data set ready */
#define MSR_TERI    0x04 /* trailing edge ring indicator */
#define MSR_DDCD    0x08 /* delta data carrier detect */
#define MSR_CTS     0x10 /* clear to send */
#define MSR_DSR     0x20 /* data set ready */
#define MSR_RI      0x40 /* ring indicator */
#define MSR_DCD     0x80 /* data carrier detect */


#define REG_RBR 0
#define REG_THR 0
#define REG_IER 1
#define REG_IIR 2
#define REG_FCR 2
#define REG_LCR 3
#define REG_MCR 4
#define REG_LSR 5
#define REG_MSR 6
#define REG_SCR 7
#define REG_DLL 0
#define REG_DLM 1

#endif /* _16550_H_ */
