# chip8

A CHIP-8 emulator written from scratch in C++17, with a terminal display,
a disassembler and an assembler, so you can write programs for it in
readable mnemonics and watch them run.

```sh
make
./chip8 run roms/bouncer.asm          # assembles on the fly and runs it
./chip8 run game.ch8 --hz 700 --quirks cosmac
./chip8 asm roms/keypad.asm keypad.ch8
./chip8 dis keypad.ch8
```

The 64x32 display is drawn with Unicode half-block characters (two pixels
per terminal row). The keypad maps to `1234 / qwer / asdf / zxcv`; Esc quits.
A status line shows the program counter, the current instruction
disassembled, the cycle count and whether the buzzer is on.

## What is implemented

- All 35 original opcodes: control flow (`JP`, `CALL`, `RET`, skips),
  arithmetic and logic with the `VF` carry/borrow/shift flags, memory
  (`LD I`, `ADD I`, BCD, register dump and load), timers, random numbers,
  keypad checks including the blocking `LD Vx, K`, and `DRW` with XOR
  drawing, wrap-around and collision detection.
- The built-in 4x5 font at `0x050`.
- Quirk switches for the shift and load/store behaviour of the original
  COSMAC VIP interpreter and the `JP V0` variant of SUPER-CHIP, because
  different ROMs assume different behaviour.
- 60 Hz timers decoupled from the CPU clock, which is adjustable.
- An assembler with labels, `DB`/`DW`, decimal, hex (`0x`, `#`, `$`) and
  binary (`%`) literals, and line-numbered error messages.
- A disassembler.

## How it works

`Machine::cycle()` fetches the two-byte big-endian opcode at `PC`, advances
`PC`, and dispatches on the top nibble; the remaining nibbles select the
registers and immediates. State (memory, `V0`-`VF`, `I`, stack, timers,
display, keys) is plain public data so the tests and the debugger line can
look at it. Nothing in the core touches the terminal: `main.cpp` puts the
tty in raw mode, polls keys once per frame (a pressed key is held for a few
frames because terminals never report key-up), and renders the display.

## Tests

```sh
make test      # built with AddressSanitizer and UBSan
```

Every opcode is exercised with tiny programs written as opcodes, including
the flag semantics, sprite wrapping and collision, timers, the key wait,
both quirk modes and the error cases, plus the assembler round trip for
every mnemonic and a program that is assembled, run and checked on the
display.

## License

MIT
