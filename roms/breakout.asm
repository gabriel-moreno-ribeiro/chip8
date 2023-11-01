; BREAKOUT. Move the paddle with Q (left) and E (right); clear all 32 bricks.
; You have 3 balls; the number of balls left is shown in the top-left corner.
; When the last ball is lost the paddle waits for any key and the game restarts.
;
; Registers:
;   V1 paddle x          V3/V4 ball x/y        V5 ball dx (1/255)   V6 ball dy (1/255)
;   V7 bricks left       V8 balls left         V0/V9/VA/VB scratch  VC = 30 (paddle y)
;   VD/VE brick coordinates while drawing or erasing

start:  LD V8, 3
        LD VC, 30
level:  CLS
        CALL draw_balls
        CALL draw_bricks
        LD V7, 32
        LD V1, 28
        LD I, paddle
        DRW V1, VC, 1
        CALL reset_ball

loop:   LD VA, 2
        LD DT, VA
wait:   LD VA, DT
        SE VA, 0
        JP wait

        ; ---- paddle ----
        LD V0, 4                ; Q
        SKNP V0
        CALL paddle_left
        LD V0, 6                ; E
        SKNP V0
        CALL paddle_right

        ; ---- ball ----
        LD I, ball
        DRW V3, V4, 1           ; erase
        ADD V3, V5
        ADD V4, V6
        SNE V3, 0               ; left wall
        LD V5, 1
        SNE V3, 63              ; right wall
        LD V5, 255
        SNE V4, 0               ; ceiling
        LD V6, 1
        SNE V4, 31              ; below the paddle: ball lost
        JP lost_ball
        LD I, ball
        DRW V3, V4, 1           ; draw; VF tells us whether we hit something
        SE VF, 1
        JP loop
        ; something was hit: undo the draw and find out what
        DRW V3, V4, 1
        LD V0, V4
        LD V9, 28
        SUB V0, V9              ; VF = 1 when ball y >= 28: the paddle
        SNE VF, 1
        JP paddle_hit
        LD V0, V4
        LD V9, 6
        SUB V0, V9              ; VF = 0 when ball y < 6: the balls counter, not a brick
        SNE VF, 0
        JP counter_hit
        JP brick_hit

paddle_hit: LD V6, 255          ; bounce up
        LD V0, V3
        LD V9, V1
        SUB V0, V9              ; V0 = ball x - paddle x
        LD V5, 255              ; left half of the paddle sends the ball left...
        LD V9, 4
        SUB V0, V9              ; VF = 1 when ball x - paddle x >= 4
        SNE VF, 1
        LD V5, 1                ; ...and the right half sends it right
        LD I, ball
        DRW V3, V4, 1
        JP loop

counter_hit: LD V6, 1           ; just bounce down off the digit
        LD I, ball
        DRW V3, V4, 1
        JP loop

brick_hit: CALL erase_brick
        LD V0, V6               ; flip the vertical direction
        LD V6, 255
        SNE V0, 255
        LD V6, 1
        LD I, ball
        DRW V3, V4, 1
        ADD V7, 255
        SNE V7, 0
        JP level                ; all bricks gone: next level
        JP loop

; the brick at the ball's position: x rounded down to a multiple of 8,
; y mapped to the row it belongs to (rows start at 6, 9, 12 and 15)
erase_brick: LD VD, V3
        LD V0, 0xF8
        AND VD, V0
        LD VE, V4
        SNE VE, 7
        LD VE, 6
        SNE VE, 10
        LD VE, 9
        SNE VE, 13
        LD VE, 12
        SNE VE, 16
        LD VE, 15
        LD I, brick
        DRW VD, VE, 2
        RET

paddle_left: SNE V1, 0
        RET
        LD I, paddle
        DRW V1, VC, 1
        ADD V1, 254
        DRW V1, VC, 1
        RET
paddle_right: SNE V1, 56
        RET
        LD I, paddle
        DRW V1, VC, 1
        ADD V1, 2
        DRW V1, VC, 1
        RET

lost_ball: CALL draw_balls       ; erase the counter
        ADD V8, 255
        CALL draw_balls          ; draw the new count
        SNE V8, 0
        JP game_over
        CALL reset_ball
        JP loop

game_over: LD V0, K              ; wait for any key, then start over
        JP start

reset_ball: LD V3, 31
        LD V4, 20
        RND V5, 1
        SNE V5, 0
        LD V5, 255
        LD V6, 255
        LD I, ball
        DRW V3, V4, 1
        RET

draw_balls: LD VA, 0
        LD VB, 0
        LD F, V8
        DRW VA, VB, 5
        RET

draw_bricks: LD I, brick
        LD VE, 6
rows:   LD VD, 0
cols:   DRW VD, VE, 2
        ADD VD, 8
        SE VD, 64
        JP cols
        ADD VE, 3
        SE VE, 18
        JP rows
        RET

paddle: DB %11111111
ball:   DB %10000000
brick:  DB %11111110, %11111110
