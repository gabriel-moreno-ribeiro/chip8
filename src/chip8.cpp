#include "chip8.hpp"

#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace chip8 {

namespace {

// 4x5 pixel glyphs for 0-F, stored at 0x050 as every interpreter does.
constexpr std::array<uint8_t, 80> kFont = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
};
constexpr uint16_t kFontStart = 0x050;

}  // namespace

Machine::Machine() {
    std::mt19937 engine{std::random_device{}()};
    random = [engine]() mutable { return static_cast<uint8_t>(engine() & 0xFF); };
    reset();
}

void Machine::reset() {
    memory.fill(0);
    v.fill(0);
    stack.fill(0);
    display.fill(false);
    keys.fill(false);
    i = 0;
    pc = kProgramStart;
    sp = 0;
    delay_timer = sound_timer = 0;
    cycles = 0;
    draw_flag = true;
    waiting_for_key = false;
    std::copy(kFont.begin(), kFont.end(), memory.begin() + kFontStart);
}

void Machine::load_rom(const std::vector<uint8_t>& rom) {
    reset();
    if (rom.size() > memory.size() - kProgramStart) throw std::runtime_error("rom too large");
    std::copy(rom.begin(), rom.end(), memory.begin() + kProgramStart);
}

void Machine::load_rom_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    load_rom(bytes);
}

void Machine::tick_timers() {
    if (delay_timer > 0) --delay_timer;
    if (sound_timer > 0) --sound_timer;
}

void Machine::cycle() {
    if (waiting_for_key) {
        // FX0A: block until a key is pressed, then store it
        for (int k = 0; k < 16; ++k) {
            if (keys[k]) {
                v[waiting_register] = static_cast<uint8_t>(k);
                waiting_for_key = false;
                break;
            }
        }
        return;
    }
    const uint16_t opcode = static_cast<uint16_t>((memory[pc] << 8) | memory[pc + 1]);
    pc += 2;
    ++cycles;
    execute(opcode);
}

void Machine::execute(uint16_t op) {
    const uint8_t x = (op >> 8) & 0xF;
    const uint8_t y = (op >> 4) & 0xF;
    const uint8_t n = op & 0xF;
    const uint8_t nn = op & 0xFF;
    const uint16_t nnn = op & 0xFFF;
    uint8_t& vx = v[x];
    const uint8_t vy = v[y];

    auto unknown = [op] {
        std::ostringstream s;
        s << "unknown opcode 0x" << std::hex << std::setw(4) << std::setfill('0') << op;
        throw std::runtime_error(s.str());
    };

    switch (op >> 12) {
    case 0x0:
        if (op == 0x00E0) {
            display.fill(false);
            draw_flag = true;
        } else if (op == 0x00EE) {
            if (sp == 0) throw std::runtime_error("return with empty stack");
            pc = stack[--sp];
        } else {
            unknown();  // 0NNN (machine code routine) is not supported
        }
        break;
    case 0x1: pc = nnn; break;
    case 0x2:
        if (sp >= stack.size()) throw std::runtime_error("stack overflow");
        stack[sp++] = pc;
        pc = nnn;
        break;
    case 0x3: if (vx == nn) pc += 2; break;
    case 0x4: if (vx != nn) pc += 2; break;
    case 0x5: if (n != 0) unknown(); if (vx == vy) pc += 2; break;
    case 0x6: vx = nn; break;
    case 0x7: vx = static_cast<uint8_t>(vx + nn); break;
    case 0x8:
        switch (n) {
        case 0x0: vx = vy; break;
        case 0x1: vx |= vy; v[0xF] = 0; break;
        case 0x2: vx &= vy; v[0xF] = 0; break;
        case 0x3: vx ^= vy; v[0xF] = 0; break;
        case 0x4: {
            const uint16_t sum = vx + vy;
            vx = static_cast<uint8_t>(sum);
            v[0xF] = sum > 0xFF ? 1 : 0;
            break;
        }
        case 0x5: {
            const uint8_t flag = vx >= vy ? 1 : 0;
            vx = static_cast<uint8_t>(vx - vy);
            v[0xF] = flag;
            break;
        }
        case 0x6: {
            const uint8_t src = quirks.shift_uses_vy ? vy : vx;
            const uint8_t flag = src & 1;
            vx = src >> 1;
            v[0xF] = flag;
            break;
        }
        case 0x7: {
            const uint8_t flag = vy >= vx ? 1 : 0;
            vx = static_cast<uint8_t>(vy - vx);
            v[0xF] = flag;
            break;
        }
        case 0xE: {
            const uint8_t src = quirks.shift_uses_vy ? vy : vx;
            const uint8_t flag = (src >> 7) & 1;
            vx = static_cast<uint8_t>(src << 1);
            v[0xF] = flag;
            break;
        }
        default: unknown();
        }
        break;
    case 0x9: if (n != 0) unknown(); if (vx != vy) pc += 2; break;
    case 0xA: i = nnn; break;
    case 0xB: pc = static_cast<uint16_t>(nnn + (quirks.jump_with_vx ? vx : v[0])); break;
    case 0xC: vx = random() & nn; break;
    case 0xD: {
        // draw an 8xN sprite from memory[I] at (VX, VY), XOR onto the screen,
        // VF = 1 when any lit pixel is turned off. Sprites wrap around the edges.
        const int px = vx % kWidth;
        const int py = vy % kHeight;
        v[0xF] = 0;
        for (int row = 0; row < n; ++row) {
            const uint8_t bits = memory[(i + row) & 0xFFF];
            const int yy = (py + row) % kHeight;
            for (int col = 0; col < 8; ++col) {
                if (!(bits & (0x80 >> col))) continue;
                const int xx = (px + col) % kWidth;
                bool& p = display[yy * kWidth + xx];
                if (p) v[0xF] = 1;
                p = !p;
            }
        }
        draw_flag = true;
        break;
    }
    case 0xE:
        if (nn == 0x9E) { if (keys[vx & 0xF]) pc += 2; }
        else if (nn == 0xA1) { if (!keys[vx & 0xF]) pc += 2; }
        else unknown();
        break;
    case 0xF:
        switch (nn) {
        case 0x07: vx = delay_timer; break;
        case 0x0A: waiting_for_key = true; waiting_register = x; break;
        case 0x15: delay_timer = vx; break;
        case 0x18: sound_timer = vx; break;
        case 0x1E: i = static_cast<uint16_t>((i + vx) & 0xFFF); break;
        case 0x29: i = static_cast<uint16_t>(kFontStart + (vx & 0xF) * 5); break;
        case 0x33:
            memory[i & 0xFFF] = vx / 100;
            memory[(i + 1) & 0xFFF] = (vx / 10) % 10;
            memory[(i + 2) & 0xFFF] = vx % 10;
            break;
        case 0x55:
            for (int r = 0; r <= x; ++r) memory[(i + r) & 0xFFF] = v[r];
            if (quirks.load_store_increments_i) i = static_cast<uint16_t>(i + x + 1);
            break;
        case 0x65:
            for (int r = 0; r <= x; ++r) v[r] = memory[(i + r) & 0xFFF];
            if (quirks.load_store_increments_i) i = static_cast<uint16_t>(i + x + 1);
            break;
        default: unknown();
        }
        break;
    default: unknown();
    }
}

std::string disassemble(uint16_t op) {
    std::ostringstream s;
    s << std::hex << std::uppercase;
    const int x = (op >> 8) & 0xF, y = (op >> 4) & 0xF, n = op & 0xF, nn = op & 0xFF, nnn = op & 0xFFF;
    switch (op >> 12) {
    case 0x0:
        if (op == 0x00E0) return "CLS";
        if (op == 0x00EE) return "RET";
        s << "SYS " << nnn; break;
    case 0x1: s << "JP " << nnn; break;
    case 0x2: s << "CALL " << nnn; break;
    case 0x3: s << "SE V" << x << ", " << nn; break;
    case 0x4: s << "SNE V" << x << ", " << nn; break;
    case 0x5: s << "SE V" << x << ", V" << y; break;
    case 0x6: s << "LD V" << x << ", " << nn; break;
    case 0x7: s << "ADD V" << x << ", " << nn; break;
    case 0x8: {
        static const char* names[] = {"LD", "OR", "AND", "XOR", "ADD", "SUB", "SHR", "SUBN", "?", "?", "?", "?", "?", "?", "SHL", "?"};
        s << names[n] << " V" << x << ", V" << y;
        break;
    }
    case 0x9: s << "SNE V" << x << ", V" << y; break;
    case 0xA: s << "LD I, " << nnn; break;
    case 0xB: s << "JP V0, " << nnn; break;
    case 0xC: s << "RND V" << x << ", " << nn; break;
    case 0xD: s << "DRW V" << x << ", V" << y << ", " << n; break;
    case 0xE: s << (nn == 0x9E ? "SKP V" : "SKNP V") << x; break;
    case 0xF:
        switch (nn) {
        case 0x07: s << "LD V" << x << ", DT"; break;
        case 0x0A: s << "LD V" << x << ", K"; break;
        case 0x15: s << "LD DT, V" << x; break;
        case 0x18: s << "LD ST, V" << x; break;
        case 0x1E: s << "ADD I, V" << x; break;
        case 0x29: s << "LD F, V" << x; break;
        case 0x33: s << "LD B, V" << x; break;
        case 0x55: s << "LD [I], V" << x; break;
        case 0x65: s << "LD V" << x << ", [I]"; break;
        default: s << "??? " << op;
        }
        break;
    }
    return s.str();
}

}  // namespace chip8
