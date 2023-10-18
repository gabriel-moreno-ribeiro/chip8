; Shows the hex digit of whatever key is pressed (keys 1234/qwer/asdf/zxcv).
start:  CLS
        LD V1, 28
        LD V2, 12
loop:   LD V0, K              ; wait for a key
        CLS
        LD F, V0              ; I = font glyph for V0
        DRW V1, V2, 5
        JP loop
