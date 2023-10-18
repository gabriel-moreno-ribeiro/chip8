CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -pedantic -O2
CORE = src/chip8.cpp src/assembler.cpp

.PHONY: all test roms clean

all: chip8

chip8: $(CORE) src/main.cpp src/chip8.hpp src/assembler.hpp
	$(CXX) $(CXXFLAGS) -o chip8 $(CORE) src/main.cpp

test_chip8: $(CORE) tests/test_chip8.cpp src/chip8.hpp src/assembler.hpp
	$(CXX) $(CXXFLAGS) -g -fsanitize=address,undefined -o test_chip8 $(CORE) tests/test_chip8.cpp

test: test_chip8
	./test_chip8

roms: chip8
	./chip8 asm roms/bouncer.asm roms/bouncer.ch8
	./chip8 asm roms/keypad.asm roms/keypad.ch8

clean:
	rm -f chip8 test_chip8 roms/*.ch8
