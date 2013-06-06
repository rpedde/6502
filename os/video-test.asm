        ;;
        ;; Test of generic video driver
        ;;

        ;; This fits in an 8k rom, from $E000 to $FFFF

                .org    $E000

                video_mode_reg = $DFFF
                video_color = $DFFE

                scratch1 = $10
                scratch2 = $11

reset_routine:
start:
                lda #$00
                sta video_mode_reg
                sta scratch1

                lda #$d0
                sta scratch2

                ldy $00
                ldx $00
L20:
                lda #"a"

L23:
                sta (scratch1),y
                iny
                cpy #80
                bne L23

                ldy #$00
                clc
                lda #80
                adc scratch1
                sta scratch1
                lda #00
                adc scratch2
                sta scratch2

                inx
                cpx #24
                bne L20

                ldx $00

loop:
                txa
                sta video_color
                inx
                jmp loop

irq_routine:
nmi_routine:
                rti

                .org $fffa

v_nmi:          dw nmi_routine
v_reset:        dw reset_routine
v_irq:          dw irq_routine
