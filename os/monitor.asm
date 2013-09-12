        ;; monitor.asm
        ;; a simple 6502 binary protocol monitor
        ;;


        ;; pad this out into a whole 8k rom
        .org    $e000
        .db     $ea


uart            = $bc00
regtable        = $e0
cmd_max         = 5
scratchmem      = $d0

        ;; regtable holds the breakpoint table as well as
        ;; the register data
        ;;
        ;; regtable + 0 : A
        ;; regtable + 1 : X
        ;; regtable + 2 : Y
        ;; regtable + 3 : SP
        ;; regtable + 4 : P
        ;; regtable + 5 : IP
        ;; regtable + 7,8,9 : IP save data

        .org    $f800

reset_vector:
start:
        jsr     uart_init
        lda     #$ff
        sta     regtable + 3
        php
        pla
        sta     regtable + 4
        lda     #$00
        sta     regtable + 5
        lda     #$e0
        sta     regtable + 6

mainloop:
        jsr     uart_in

        ;; command byte in A
        cmp     #cmd_max
        bcs     errmain

        ;; shift left (mul 2) and pull call stack
        asl
        tax
        lda     cmd_jumptable+1,x
        pha
        lda     cmd_jumptable,x
        pha

        ;; send preliminary success
        lda     #$01
        jsr     uart_out

        rts

errmain:
        ;; send preliminary failure - does not meet prereq
        lda     #$00
        jsr     uart_out
        jmp     mainloop

        ;; Commands:
        ;;
        ;; Command:
        ;;  0 - Ping (cmd_ping)
        ;;
        ;; Extra parameters:
        ;;  None
        ;;
        ;; Response:
        ;;  uint8_t - result code (always 1)
        ;;
cmd_ping:
        lda     #$01
        jsr     uart_out

        jmp     mainloop

        ;; Command:
        ;;  1 - Get Registers (cmd_getreg)
        ;;
        ;; Extra parameters:
        ;;   None
        ;;
        ;; Response:
        ;;   uint8_t - A
        ;;   uint8_t - X
        ;;   uint8_t - Y
        ;;   uint8_t - SP
        ;;   uint8_t - P
        ;;   uint16_t - IP
cmd_getreg:
        lda     #regtable
        sta     scratchmem

        lda     #$00
        sta     scratchmem + 1

        lda     #$07
        sta     scratchmem + 2

        jmp     util_sendblock

        ;; Command:
        ;;  2 - Get Data (cmd_getdata)
        ;;
        ;; Extra parameters:
        ;;   uint16_t - addr (little endian)
        ;;   uint8_t - len
        ;;
        ;; Response:
        ;;   uint8_t * len
        ;;
        ;; Destroys:
        ;;   A, Y
cmd_getdata:
        jsr     uart_in         ; set up parameters
        sta     scratchmem

        jsr     uart_in
        sta     scratchmem + 1

        jsr     uart_in
        beq     c2l2            ; don't emit anything on zero length

        sta     scratchmem + 2

util_sendblock:
        ldy     #$00

c2l1:
        lda     (scratchmem), y
        jsr     uart_out
        iny

        cpy     scratchmem + 2
        bne     c2l1

c2l2:
        jmp     mainloop


        ;; Command:
        ;;  3 - Set Registers (cmd_setreg)
        ;;
        ;; Extra parameters:
        ;;   uint8_t - A
        ;;   uint8_t - X
        ;;   uint8_t - Y
        ;;   uint8_t - SP
        ;;   uint8_t - P
        ;;   uint16_t - IP
        ;;
        ;; Response:
        ;;   uin8_t - result (0x01)
        ;;
cmd_setreg:
        lda     #regtable
        sta     scratchmem

        lda     #$00
        sta     scratchmem + 1

        lda     #$07
        sta     scratchmem + 2

        jmp     util_getblock

        ;; Command:
        ;;  4 - Set Data (cmd_setdata)
        ;;
        ;; Extra parameters:
        ;;   uint16_t - addr (little endian)
        ;;   uint8_t - len
        ;;   uint8_t * len
        ;;
        ;; Response:
        ;;   uint8_t - result (0x01)
        ;;
        ;; Destroys:
        ;;   A, Y
cmd_setdata:
        jsr     uart_in         ; set up parameters
        sta     scratchmem

        jsr     uart_in
        sta     scratchmem + 1

        jsr     uart_in
        beq     c4out           ; success on zero len

        sta     scratchmem + 2

util_getblock:
        ldy     #$00

c4l1:
        jsr     uart_in
        sta     (scratchmem), y
        iny

        cpy     scratchmem + 2
        bne     c4l1

c4out:
        lda     #$01
        jsr     uart_out
        jmp     mainloop

        ;; uart_out
        ;;
        ;; send a character out the uart.  this should really
        ;; do hardware or software handshaking.
        ;;
uart_out:
        pha
        lda     uart + 5
        and     #$20            ; Transmit reg empty
        bne     uol1

        nop
        jmp     uart_out

uol1:
        pla
        sta     uart
        rts

        ;; uart_init
        ;;
        ;; this sets up the 16550 uart.  it should ideally
        ;; set baud, stop bits, etc, and generally ensure
        ;; the 16550 is in the right state.  we'll just
        ;; skip that for now.  ;)
uart_init:
        ;; set lcr to 8N1
        ;; 7 6 5 4 3 2 1 0
        ;;             1 1 - 8 data bits
        ;;           0     - 1 stop bit
        ;;         0       - no parity
        ;;       0         - even parity select (don't care)
        ;;     0           - sticky parity bit (don't care)
        ;;   0             - break (clear)
        ;; 1               - dlab
        lda     #$83
        sta     uart + 3

        ;; divisor MSB
        lda     #$00
        sta     uart + 1

        ;; divisor LSB
        ;;   #3  - 38400 @ 1.8432MHz clock
        ;;   #5  - 38400 @ 3.072MHz clock
        ;;   #30 - 38400 @ 18.432MHz clock
        lda     #$03
        sta     uart

        ;; reset the divisor latch
        sta     uart + 3
        rts

        ;; uart_waiting
        ;;
        ;; this does a blocking wait on a byte in from
        ;; the uart.
        ;;
        ;; Destroys: a, d
uart_in:
        lda     uart + 5
        and     #$01
        bne     uil1

        nop
        jmp     uart_in

uil1:
        lda     uart
        rts

        ;; we'll need to flesh these out once we
        ;; have single stepping that is brk driven

nmi_vector:
irq_vector:
        rti


cmd_jumptable:
        .dw     cmd_ping - 1
        .dw     cmd_getreg - 1
        .dw     cmd_getdata - 1
        .dw     cmd_setreg - 1
        .dw     cmd_setdata - 1

        ;;
        ;; reset/nmi jump table
        ;;

        .org $fffa
v_nmi:
        dw      nmi_vector
v_reset:
        dw      reset_vector
v_irq:
        dw      irq_vector
