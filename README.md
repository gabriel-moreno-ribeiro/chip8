# chip8

> 🇺🇸 [English version below](#english)

Um emulador de CHIP-8 em C++17 com display no terminal, disassembler e assembler, então dá pra escrever programa em mnemônicos legíveis e ver rodando. E, porque emulador sem jogo é triste, dois jogos escritos em assembly de CHIP-8: **Pong** pra dois jogadores e **Breakout**.

```sh
make
make pong                          # 1/Q movem a raquete da esquerda, 4/R a da direita
make breakout                      # Q/E movem a raquete, 3 bolas, 32 tijolos
./chip8 run roms/bouncer.asm       # monta na hora e roda
./chip8 run game.ch8 --hz 700 --quirks cosmac
./chip8 asm roms/pong.asm pong.ch8
./chip8 dis pong.ch8
```

O display de 64x32 é desenhado com caracteres de meio bloco (dois pixels por linha do terminal). O teclado hexadecimal mapeia pra `1234 / qwer / asdf / zxcv`; Esc sai. Uma linha de status mostra o PC, a instrução atual desmontada, os ciclos e se o buzzer está ligado.

## Os jogos (`roms/`)

Escrever Pong em CHIP-8 é um exercício de humildade: 16 registradores de 8 bits, sem multiplicação, sem comparação direta (você faz `SUB` e olha o flag de borrow), e o único jeito de saber que a bola bateu em algo é o bit de colisão do `DRW`. O Breakout usa exatamente isso: desenha a bola, se `VF` ligou, desfaz o desenho, descobre se foi raquete, contador ou tijolo pela coordenada, apaga o tijolo redesenhando ele (XOR), inverte a direção. Cada jogo tem uns 150 linhas comentadas, e tem um `pong.asm` pra ler antes do `breakout.asm`.

Os dois rodam nos testes de forma headless: a máquina executa milhares de ciclos com os timers batendo, teclas são "apertadas" pelo código, e a suíte confere que raquetes se movem, que tijolos somem e que o contador na tela bate com o registrador.

## O emulador

- Os 35 opcodes originais: fluxo (`JP`, `CALL`, `RET`, skips), aritmética e lógica com os flags de carry/borrow/shift em `VF`, memória (`LD I`, `ADD I`, BCD, dump e load de registradores), timers, aleatório, teclado incluindo o `LD Vx, K` que bloqueia, e `DRW` com XOR, wrap e colisão.
- Fonte 4x5 embutida em `0x050`.
- Quirks pro shift e load/store do COSMAC VIP original e o `JP V0` do SUPER-CHIP, porque ROMs diferentes assumem comportamentos diferentes.
- Timers a 60 Hz desacoplados do clock da CPU (ajustável).
- Assembler com labels, `DB`/`DW`, literais decimais, hex (`0x`, `#`, `$`) e binários (`%`), com erro apontando a linha.

`Machine::cycle()` busca o opcode de dois bytes em `PC`, avança e despacha pelo nibble alto. O estado é dado público pra os testes e a linha de debug olharem. Nada no núcleo toca o terminal.

Testes: `make test` (ASan e UBSan): todo opcode, flags, wrap e colisão de sprite, timers, espera de tecla, os dois quirks, os erros, o assembler de ida e volta em cada mnemônico, e os jogos.

---

## English

A CHIP-8 emulator in C++17 with a terminal display, a disassembler and an assembler, so you can write programs in readable mnemonics and watch them run. And, because an emulator without games is sad, two games written in CHIP-8 assembly: two-player **Pong** and **Breakout**.

```sh
make
make pong                          # 1/Q move the left paddle, 4/R the right one
make breakout                      # Q/E move the paddle, 3 balls, 32 bricks
./chip8 run roms/bouncer.asm       # assembles on the fly and runs
./chip8 run game.ch8 --hz 700 --quirks cosmac
./chip8 asm roms/pong.asm pong.ch8
./chip8 dis pong.ch8
```

The 64x32 display is drawn with half-block characters (two pixels per terminal line). The hex keypad maps to `1234 / qwer / asdf / zxcv`; Esc quits. A status line shows the PC, the current instruction disassembled, the cycles and whether the buzzer is on.

## The games (`roms/`)

Writing Pong in CHIP-8 is an exercise in humility: 16 8-bit registers, no multiplication, no direct comparison (you do a `SUB` and look at the borrow flag), and the only way to know the ball hit something is the `DRW` collision bit. Breakout uses exactly that: draw the ball, if `VF` went on, undo the drawing, figure out whether it was the paddle, the counter or a brick from the coordinate, erase the brick by redrawing it (XOR), flip the direction. Each game is about 150 commented lines, and there's a `pong.asm` to read before `breakout.asm`.

Both run headless in the tests: the machine executes thousands of cycles with the timers ticking, keys are "pressed" by code, and the suite checks that paddles move, that bricks disappear and that the counter on screen matches the register.

## The emulator

- The 35 original opcodes: flow (`JP`, `CALL`, `RET`, skips), arithmetic and logic with the carry/borrow/shift flags in `VF`, memory (`LD I`, `ADD I`, BCD, register dump and load), timers, random, keyboard including the blocking `LD Vx, K`, and `DRW` with XOR, wrap and collision.
- Built-in 4x5 font at `0x050`.
- Quirks for the shift and load/store of the original COSMAC VIP and the SUPER-CHIP `JP V0`, because different ROMs assume different behaviours.
- Timers at 60 Hz decoupled from the CPU clock (adjustable).
- Assembler with labels, `DB`/`DW`, decimal, hex (`0x`, `#`, `$`) and binary (`%`) literals, with errors pointing at the line.

`Machine::cycle()` fetches the two-byte opcode at `PC`, advances and dispatches on the high nibble. The state is public data for the tests and the debug line to look at. Nothing in the core touches the terminal.

Tests: `make test` (ASan and UBSan): every opcode, flags, sprite wrap and collision, timers, key wait, both quirks, the errors, the assembler round trip on every mnemonic, and the games.

MIT.
