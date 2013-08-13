        ;; monitor.asm
        ;; a simple 6502 binary protocol monitor
        ;;


        ;;


uart = $bc00
cmd_max = 2

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
        ;;   uint8_t - S
        ;;   uint8_t - D
        ;;   uint16_t - PC

        ;; uart_out
        ;;
        ;; send a character out the uart.  this should really
        ;; do hardware or software handshaking.
        ;;
cmd_getreg:
        jmp     mainloop

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
        beq     uart_in

        lda     uart
        rts

cmd_jumptable:
        .dw     cmd_ping - 1
        .dw     cmd_getreg - 1
