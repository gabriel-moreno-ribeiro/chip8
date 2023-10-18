; A ball that bounces around the screen, drawn with XOR sprites.
; Registers: V1,V2 position  V3,V4 direction (1 or 255)  V5 delay
start:  LD I, ball
        LD V1, 10
        LD V2, 5
        LD V3, 1
        LD V4, 1
        DRW V1, V2, 4
loop:   LD V5, 1
        LD DT, V5
wait:   LD V5, DT
        SE V5, 0
        JP wait
        DRW V1, V2, 4          ; erase (XOR)
        ADD V1, V3
        ADD V2, V4
        SNE V1, 0
        LD V3, 1
        SNE V1, 60
        LD V3, 255
        SNE V2, 0
        LD V4, 1
        SNE V2, 28
        LD V4, 255
        DRW V1, V2, 4          ; draw at the new position
        JP loop
ball:   DB %01100000, %11110000, %11110000, %01100000
