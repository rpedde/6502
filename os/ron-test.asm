        ;; simple test of 16550 uart
        ;;
        ;;


uart_base = $1000

        .org $e000

reset_routine
        ldx #$00

        lda output,x

outloop sta uart_base
        inx
        lda output,x
        bne outloop
        jmp *

irq_routine
nmi_routine
        rti






output  .db     "Testing 16550 UART"
        .db     $00

        .org $fffa

v_nmi   dw nmi_routine
v_reset dw reset_routine
v_irq   dw irq_routine
