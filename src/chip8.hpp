// CHIP-8 virtual machine: memory, registers, timers, display, keypad and the
// interpreter for all 35 original opcodes. No I/O here; the terminal
// front-end and the tests drive it.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace chip8 {

constexpr int kWidth = 64;
constexpr int kHeight = 32;
constexpr uint16_t kProgramStart = 0x200;

struct Quirks {
    bool shift_uses_vy = false;     // 8XY6/8XYE: original COSMAC shifts VY into VX
    bool load_store_increments_i = false;  // FX55/FX65 advance I on the original
    bool jump_with_vx = false;      // BNNN uses VX on SUPER-CHIP
};

class Machine {
public:
    Machine();

    void reset();
    void load_rom(const std::vector<uint8_t>& rom);
    void load_rom_file(const std::string& path);

    // Executes one instruction. Throws std::runtime_error on an unknown opcode.
    void cycle();
    // Call at 60 Hz.
    void tick_timers();

    void set_key(int key, bool down) { keys.at(key) = down; }
    bool pixel(int x, int y) const { return display[y * kWidth + x]; }
    bool display_changed() const { return draw_flag; }
    void clear_draw_flag() { draw_flag = false; }
    bool sound_on() const { return sound_timer > 0; }

    // deterministic random source for tests (default: std::mt19937)
    void set_random(std::function<uint8_t()> fn) { random = std::move(fn); }

    Quirks quirks;

    // state is public so tests and debuggers can inspect it
    std::array<uint8_t, 4096> memory{};
    std::array<uint8_t, 16> v{};
    uint16_t i = 0;
    uint16_t pc = kProgramStart;
    std::array<uint16_t, 16> stack{};
    uint8_t sp = 0;
    uint8_t delay_timer = 0;
    uint8_t sound_timer = 0;
    std::array<bool, kWidth * kHeight> display{};
    std::array<bool, 16> keys{};
    uint64_t cycles = 0;

private:
    bool draw_flag = false;
    bool waiting_for_key = false;
    uint8_t waiting_register = 0;
    std::function<uint8_t()> random;

    void execute(uint16_t opcode);
};

// Disassembles one opcode into a readable mnemonic (used by the debugger view).
std::string disassemble(uint16_t opcode);

}  // namespace chip8
