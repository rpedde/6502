        ;; monitor.asm
        ;; a simple 6502 binary protocol monitor
        ;;


        ;;


uart            = $bc00
regtable        = $e0
cmd_max         = 2
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

        .org    $e000

start:
        jsr     uart_init

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
        lda     regtable
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

        ;; uart_out
        ;;
        ;; send a character out the uart.  this should really
        ;; do hardware or software handshaking.
        ;;
uart_out:
        sta     uart
        rts

        ;; uart_init
        ;;
        ;; this sets up the 16550 uart.  it should ideally
        ;; set baud, stop bits, etc, and generally ensure
        ;; the 16550 is in the right state.  we'll just
        ;; skip that for now.  ;)
uart_init:
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
        bne     UIL1

        nop
        jmp     uart_in

UIL1:
        lda     uart
        rts

cmd_jumptable:
        .dw     cmd_ping - 1
        .dw     cmd_getreg - 1
        .dw     cmd_getdata - 1
