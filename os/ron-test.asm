        ;; simple test of 16550 uart
        ;;
        ;;


uart_base = $1000

temp_reg = $0f

                .org $e000

reset_routine:  ldy #o_start & $00ff
                lda #o_start > 8
                jsr ser_outstring

prompt:         lda #'>'
                sta uart_base
                lda #' '
                sta uart_base

poll_uart:      lda uart_base + 5
                and #$01
                beq poll_uart

                lda uart_base
                sta uart_base

                cmp #$0d
                bne poll_uart

                lda #$0a
                sta uart_base
                jmp prompt

                ;; output a string, assumes A:Y points to zero terminated
                ;; string.  destroys A and Y
ser_outstring:  sty temp_reg
                sta temp_reg + 1

                ldy #$00

ser_outstring1: lda (temp_reg),y
                beq ser_outstring2

                sta uart_base
                iny
                bne ser_outstring1

ser_outstring2: rts


irq_routine:
nmi_routine:
                rti



o_start:        .db     "Howdy from 6502", $0a, $0d, $00

                .org $fffa

v_nmi:          dw nmi_routine
v_reset:        dw reset_routine
v_irq:          dw irq_routine
