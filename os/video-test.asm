                ;;
                ;; Test of generic video driver
                ;;

                .org    $8000

                video_mode_reg = $DFFF
                video_color = $DFFE

                scratch1 = $10
                scratch2 = $11

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
