// Terminal front-end: runs a ROM (or assembles a .asm file first), draws
// the 64x32 display with Unicode half blocks, reads the keypad from the
// keyboard in raw mode, and shows a small debugger line.
//
//   chip8 run  game.ch8 [--hz 700] [--quirks cosmac]
//   chip8 run  program.asm
//   chip8 asm  program.asm out.ch8
//   chip8 dis  game.ch8
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <termios.h>
#include <unistd.h>

#include "assembler.hpp"
#include "chip8.hpp"

namespace {

std::vector<uint8_t> load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (path.size() > 4 && path.substr(path.size() - 4) == ".asm") return chip8::assemble(text);
    return std::vector<uint8_t>(text.begin(), text.end());
}

struct RawTerminal {
    termios saved{};
    bool active = false;
    RawTerminal() {
        if (!isatty(STDIN_FILENO)) return;
        tcgetattr(STDIN_FILENO, &saved);
        termios raw = saved;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        active = true;
        std::fputs("\x1b[?25l\x1b[2J", stdout);
    }
    ~RawTerminal() {
        if (active) tcsetattr(STDIN_FILENO, TCSANOW, &saved);
        std::fputs("\x1b[?25h\x1b[0m\n", stdout);
    }
};

// keyboard layout 1234 / qwer / asdf / zxcv -> CHIP-8 keypad 123C / 456D / 789E / A0BF
int key_for(char c) {
    static const char* layout = "x123qweasdzc4rfv";
    for (int i = 0; i < 16; ++i)
        if (layout[i] == c) return i;
    return -1;
}

std::string render(const chip8::Machine& m) {
    std::string out = "\x1b[H";
    for (int y = 0; y < chip8::kHeight; y += 2) {
        for (int x = 0; x < chip8::kWidth; ++x) {
            const bool top = m.pixel(x, y);
            const bool bottom = m.pixel(x, y + 1);
            if (top && bottom) out += "\xE2\x96\x88";       // full block
            else if (top) out += "\xE2\x96\x80";            // upper half
            else if (bottom) out += "\xE2\x96\x84";         // lower half
            else out += ' ';
        }
        out += "\x1b[K\n";
    }
    return out;
}

int run(const std::string& path, int hz, const std::string& quirks) {
    chip8::Machine m;
    if (quirks == "cosmac") m.quirks = {true, true, false};
    m.load_rom(load(path));

    RawTerminal terminal;
    using clock = std::chrono::steady_clock;
    const auto cycle_period = std::chrono::nanoseconds(1'000'000'000 / hz);
    const auto timer_period = std::chrono::nanoseconds(1'000'000'000 / 60);
    auto next_cycle = clock::now();
    auto next_timer = next_cycle;
    auto next_frame = next_cycle;
    std::array<int, 16> key_ttl{};  // keys stay pressed for a few frames since terminals send no key-up

    while (true) {
        const auto now = clock::now();
        if (now >= next_cycle) {
            m.cycle();
            next_cycle += cycle_period;
        }
        if (now >= next_timer) {
            m.tick_timers();
            next_timer += timer_period;
            // read keys
            char c;
            while (read(STDIN_FILENO, &c, 1) == 1) {
                if (c == 27 || c == 'p') return 0;  // Esc or p quits
                const int k = key_for(c);
                if (k >= 0) key_ttl[k] = 6;
            }
            for (int k = 0; k < 16; ++k) {
                if (key_ttl[k] > 0) --key_ttl[k];
                m.set_key(k, key_ttl[k] > 0);
            }
        }
        if (now >= next_frame) {
            next_frame += timer_period;
            std::string frame = render(m);
            const uint16_t op = static_cast<uint16_t>((m.memory[m.pc] << 8) | m.memory[m.pc + 1]);
            char status[160];
            std::snprintf(status, sizeof(status), "\x1b[7m pc=%03X  I=%03X  %-18s  cycles=%llu  %s  keys 1234/qwer/asdf/zxcv, Esc quits \x1b[0m\x1b[K",
                          m.pc, m.i, chip8::disassemble(op).c_str(), static_cast<unsigned long long>(m.cycles),
                          m.sound_on() ? "BEEP" : "    ");
            frame += status;
            std::fwrite(frame.data(), 1, frame.size(), stdout);
            std::fflush(stdout);
        }
        std::this_thread::sleep_until(std::min({next_cycle, next_timer, next_frame}));
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        if (args.size() >= 2 && args[0] == "run") {
            int hz = 700;
            std::string quirks = "modern";
            for (size_t i = 2; i + 1 < args.size(); i += 2) {
                if (args[i] == "--hz") hz = std::stoi(args[i + 1]);
                else if (args[i] == "--quirks") quirks = args[i + 1];
            }
            return run(args[1], hz, quirks);
        }
        if (args.size() == 3 && args[0] == "asm") {
            std::ifstream in(args[1]);
            if (!in) throw std::runtime_error("cannot open " + args[1]);
            std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            const auto rom = chip8::assemble(text);
            std::ofstream out(args[2], std::ios::binary);
            out.write(reinterpret_cast<const char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
            std::cout << "wrote " << rom.size() << " bytes to " << args[2] << "\n";
            return 0;
        }
        if (args.size() == 2 && args[0] == "dis") {
            const auto rom = load(args[1]);
            for (size_t i = 0; i + 1 < rom.size(); i += 2) {
                const uint16_t op = static_cast<uint16_t>((rom[i] << 8) | rom[i + 1]);
                std::printf("%03zX  %04X  %s\n", 0x200 + i, op, chip8::disassemble(op).c_str());
            }
            return 0;
        }
        std::cerr << "usage: chip8 run <rom|asm> [--hz N] [--quirks modern|cosmac]\n"
                     "       chip8 asm <source.asm> <out.ch8>\n"
                     "       chip8 dis <rom>\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
