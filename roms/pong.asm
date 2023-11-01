; PONG for two players.
;   left paddle:  1 = up, Q = down     right paddle: 4 = up, R = down
; (the terminal keypad maps 1234/qwer/asdf/zxcv to CHIP-8 keys 1 2 3 C / 4 5 6 D / ...)
; First to 9 points wins; the game then restarts.
;
; Registers:
;   V1 left paddle y     V2 right paddle y     V3/V4 ball x/y
;   V5 ball dx (1/255)   V6 ball dy (1/255)    V7 left score   V8 right score
;   V0/V9/VA/VB scratch  VC = 2 (left paddle x)  VD = 60 (right paddle x)
;   VE = who scored this frame (0 nobody, 1 right player, 2 left player)

start:  CLS
        LD VC, 2
        LD VD, 60
        LD V1, 13
        LD V2, 13
        LD V7, 0
        LD V8, 0
        CALL draw_paddles
        CALL draw_scores
        CALL reset_ball

loop:   LD VA, 2                ; ~30 frames per second
        LD DT, VA
wait:   LD VA, DT
        SE VA, 0
        JP wait

        ; ---- left paddle input ----
        LD V0, 1
        SKNP V0
        CALL left_up
        LD V0, 4
        SKNP V0
        CALL left_down
        ; ---- right paddle input ----
        LD V0, 0xC
        SKNP V0
        CALL right_up
        LD V0, 0xD
        SKNP V0
        CALL right_down

        ; ---- move the ball ----
        LD I, ball
        DRW V3, V4, 2           ; erase
        ADD V3, V5
        ADD V4, V6
        SNE V4, 0               ; top wall
        LD V6, 1
        SNE V4, 30              ; bottom wall
        LD V6, 255
        LD VE, 0
        SNE V3, 4               ; reached the left paddle column
        CALL check_left
        SNE V3, 58              ; reached the right paddle column
        CALL check_right
        SE VE, 0
        JP scored
        LD I, ball
        DRW V3, V4, 2           ; draw at the new position
        JP loop

scored: CALL draw_scores        ; erase the old digits
        SNE VE, 1
        ADD V8, 1
        SNE VE, 2
        ADD V7, 1
        CALL draw_scores
        SNE V7, 9
        JP start
        SNE V8, 9
        JP start
        CALL reset_ball
        JP loop

; ---------------------------------------------------------------- paddles --
left_up:  SNE V1, 0
          RET
          LD I, paddle
          DRW VC, V1, 6
          ADD V1, 255
          DRW VC, V1, 6
          RET
left_down: SNE V1, 26
          RET
          LD I, paddle
          DRW VC, V1, 6
          ADD V1, 1
          DRW VC, V1, 6
          RET
right_up: SNE V2, 0
          RET
          LD I, paddle
          DRW VD, V2, 6
          ADD V2, 255
          DRW VD, V2, 6
          RET
right_down: SNE V2, 26
          RET
          LD I, paddle
          DRW VD, V2, 6
          ADD V2, 1
          DRW VD, V2, 6
          RET

draw_paddles: LD I, paddle
          DRW VC, V1, 6
          DRW VD, V2, 6
          RET

; ------------------------------------------------------------- collisions --
; the ball is in the left paddle column: bounce when it overlaps the paddle,
; otherwise the right player scores (VE = 1)
check_left: CALL hit_test_left
          SNE VF, 1
          JP bounce_right
          LD VE, 1
          RET
bounce_right: LD V5, 1
          RET
check_right: CALL hit_test_right
          SNE VF, 1
          JP bounce_left
          LD VE, 2
          RET
bounce_left: LD V5, 255
          RET

; VF = 1 when ball y + 1 >= paddle y and paddle y + 6 >= ball y
hit_test_left: LD V0, V4
          ADD V0, 1
          LD V9, V1
          SUB V0, V9              ; VF = 1 if no borrow
          SE VF, 1
          RET
          LD V0, V1
          ADD V0, 6
          LD V9, V4
          SUB V0, V9
          RET
hit_test_right: LD V0, V4
          ADD V0, 1
          LD V9, V2
          SUB V0, V9
          SE VF, 1
          RET
          LD V0, V2
          ADD V0, 6
          LD V9, V4
          SUB V0, V9
          RET

; ------------------------------------------------------------------ scores --
; digits are drawn with the built-in font at the top of the screen (XOR: calling
; twice with the same scores erases them)
draw_scores: LD VA, 20
          LD VB, 1
          LD F, V7
          DRW VA, VB, 5
          LD VA, 40
          LD F, V8
          DRW VA, VB, 5
          RET

reset_ball: LD V3, 31
          LD V4, 15
          RND V5, 1
          SNE V5, 0
          LD V5, 255
          RND V6, 1
          SNE V6, 0
          LD V6, 255
          LD I, ball
          DRW V3, V4, 2
          RET

paddle: DB %11000000, %11000000, %11000000, %11000000, %11000000, %11000000
ball:   DB %11000000, %11000000
