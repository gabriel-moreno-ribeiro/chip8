// A small CHIP-8 assembler (the classic Cowgod mnemonics) with labels, so
// programs can be written readably instead of as hex.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chip8 {

// Assembles source text into ROM bytes. Throws std::runtime_error with the
// line number on syntax errors.
std::vector<uint8_t> assemble(const std::string& source);

}  // namespace chip8
