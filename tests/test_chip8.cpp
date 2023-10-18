// Tests for the interpreter and the assembler. Build and run with `make test`.
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "../src/assembler.hpp"
#include "../src/chip8.hpp"

using chip8::Machine;

static int passed = 0, failed = 0;

#define CHECK(name, cond)                                                     \
    do {                                                                      \
        if (cond) ++passed;                                                   \
        else { ++failed; std::printf("FAIL %s (line %d)\n", name, __LINE__); } \
    } while (0)

// Runs a program given as opcodes; returns the machine after `steps` cycles.
static Machine run_ops(const std::vector<uint16_t>& ops, size_t steps) {
    std::vector<uint8_t> rom;
    for (uint16_t op : ops) {
        rom.push_back(static_cast<uint8_t>(op >> 8));
        rom.push_back(static_cast<uint8_t>(op & 0xFF));
    }
    Machine m;
    m.load_rom(rom);
    for (size_t i = 0; i < steps; ++i) m.cycle();
    return m;
}

static int lit_pixels(const Machine& m) {
    int n = 0;
    for (bool p : m.display) n += p;
    return n;
}

static void test_arithmetic_and_flow() {
    Machine m = run_ops({0x6105, 0x7103, 0x6210, 0x8124}, 4);  // V1=5; V1+=3; V2=0x10; V1+=V2
    CHECK("LD/ADD immediate", m.v[1] == 0x18);
    CHECK("ADD register no carry", m.v[0xF] == 0);
    m = run_ops({0x61FF, 0x6202, 0x8124}, 3);
    CHECK("ADD carry", m.v[1] == 0x01 && m.v[0xF] == 1);
    m = run_ops({0x6105, 0x6207, 0x8125}, 3);
    CHECK("SUB borrow", m.v[1] == 0xFE && m.v[0xF] == 0);
    m = run_ops({0x6107, 0x6205, 0x8125}, 3);
    CHECK("SUB no borrow", m.v[1] == 2 && m.v[0xF] == 1);
    m = run_ops({0x6105, 0x6207, 0x8127}, 3);
    CHECK("SUBN", m.v[1] == 2 && m.v[0xF] == 1);
    m = run_ops({0x610B, 0x8106, 0x620B, 0x820E}, 4);
    CHECK("SHR", m.v[1] == 5);
    CHECK("SHL", m.v[2] == 0x16 && m.v[0xF] == 0);
    m = run_ops({0x6181, 0x810E}, 2);
    CHECK("SHL flag from bit 7 of VX", m.v[0xF] == 1 && m.v[1] == 0x02);
    m = run_ops({0x61F0, 0x620F, 0x8121, 0x6331, 0x8322, 0x6455, 0x6503, 0x8453}, 8);
    CHECK("OR", m.v[1] == 0xFF);
    CHECK("AND", m.v[3] == 0x01);
    CHECK("XOR", m.v[4] == 0x56);
    CHECK("logic ops reset VF", m.v[0xF] == 0);
    // cosmac shift quirk uses VY
    Machine q;
    q.quirks.shift_uses_vy = true;
    q.load_rom({0x61, 0x00, 0x62, 0x03, 0x81, 0x26});  // V1=0; V2=3; SHR V1,V2 -> V1 = 3>>1 = 1, VF=1
    q.cycle(); q.cycle(); q.cycle();
    CHECK("shift quirk", q.v[1] == 1 && q.v[0xF] == 1);

    // jumps, calls, skips
    m = run_ops({0x1204, 0x6001, 0x6002}, 2);  // JP 204 skips "V0=1"
    CHECK("JP", m.v[0] == 2 && m.pc == 0x206);
    m = run_ops({0x2206, 0x6001, 0x1204, 0x6002, 0x00EE}, 4);  // CALL 206; ...; V0=2; RET -> V0=1
    CHECK("CALL/RET", m.v[0] == 1 && m.sp == 0);
    // V0=5; SE skips V0=1; SNE does not skip so V0=2; V1=5; SE V0,V1 no skip so V0=3; SNE V0,V1 skips V0=4
    m = run_ops({0x6005, 0x3005, 0x6001, 0x4005, 0x6002, 0x6105, 0x5010, 0x6003, 0x9010, 0x6004}, 8);
    CHECK("SE/SNE immediate and register", m.v[0] == 3 && m.v[1] == 5 && m.pc == 0x214);
    m = run_ops({0x6002, 0xB204, 0x6001, 0x6009, 0x600A}, 3);  // JP V0, 204 -> 206
    CHECK("JP V0", m.v[0] == 9);
    m = run_ops({0xA123, 0x6004, 0xF01E}, 3);
    CHECK("LD I / ADD I", m.i == 0x127);
}

static void test_memory_timers_random() {
    Machine m = run_ops({0x60FE, 0xA300, 0xF033, 0xF265}, 4);  // BCD of 254 at 0x300, then load V0..V2
    CHECK("BCD", m.memory[0x300] == 2 && m.memory[0x301] == 5 && m.memory[0x302] == 4);
    CHECK("LD Vx, [I]", m.v[0] == 2 && m.v[1] == 5 && m.v[2] == 4);
    CHECK("I unchanged without quirk", m.i == 0x300);
    m = run_ops({0x6011, 0x6122, 0x6233, 0xA400, 0xF255}, 5);
    CHECK("LD [I], Vx", m.memory[0x400] == 0x11 && m.memory[0x401] == 0x22 && m.memory[0x402] == 0x33);
    Machine q;
    q.quirks.load_store_increments_i = true;
    q.load_rom({0xA4, 0x00, 0xF1, 0x55});
    q.cycle(); q.cycle();
    CHECK("load/store quirk advances I", q.i == 0x402);

    m = run_ops({0x600A, 0xF029}, 2);
    CHECK("font address", m.i == 0x050 + 10 * 5 && m.memory[m.i] == 0xF0);

    m = run_ops({0x603C, 0xF015, 0x6005, 0xF018, 0xF107}, 5);
    CHECK("timers set and read", m.delay_timer == 0x3C && m.sound_timer == 5 && m.v[1] == 0x3C);
    CHECK("sound on", m.sound_on());
    for (int i = 0; i < 5; ++i) m.tick_timers();
    CHECK("timers count down", m.delay_timer == 0x37 && m.sound_timer == 0 && !m.sound_on());

    Machine r;
    r.set_random([] { return static_cast<uint8_t>(0xAB); });
    r.load_rom({0xC0, 0x0F});
    r.cycle();
    CHECK("RND masks the random byte", r.v[0] == 0x0B);
}

static void test_display_and_keys() {
    // draw the font glyph for 0 at (0,0): 4x5 pixels, 14 lit
    Machine m = run_ops({0x6000, 0xF029, 0x6100, 0x6200, 0xD125}, 5);
    CHECK("sprite drawn", lit_pixels(m) == 14 && m.pixel(0, 0) && m.pixel(3, 0) && !m.pixel(1, 1));
    CHECK("no collision", m.v[0xF] == 0 && m.display_changed());
    m.clear_draw_flag();
    // draw it again on top: everything erased, collision flag set
    m.memory[0x20A] = 0xD1; m.memory[0x20B] = 0x25;
    m.cycle();
    CHECK("XOR erases and sets VF", lit_pixels(m) == 0 && m.v[0xF] == 1 && m.display_changed());
    // wrapping: draw at x=62 -> columns 62,63,0,1
    m = run_ops({0x6000, 0xF029, 0x613E, 0x621E, 0xD125}, 5);
    CHECK("sprite wraps horizontally and vertically", m.pixel(62, 30) && m.pixel(1, 30) && m.pixel(62, 2) && m.pixel(1, 2));
    // CLS
    m.memory[0x20A] = 0x00; m.memory[0x20B] = 0xE0;
    m.cycle();
    CHECK("CLS", lit_pixels(m) == 0);

    // keys: SKP / SKNP
    Machine k;
    k.load_rom({0x60, 0x05, 0xE0, 0x9E, 0x61, 0x01, 0xE0, 0xA1, 0x62, 0x01});
    k.set_key(5, true);
    for (int i = 0; i < 4; ++i) k.cycle();
    CHECK("SKP skips when key is down", k.v[1] == 0 && k.v[2] == 1);
    Machine w;
    w.load_rom({0xF3, 0x0A, 0x60, 0x01});  // LD V3, K ; V0 = 1
    w.cycle(); w.cycle(); w.cycle();
    CHECK("LD Vx, K blocks", w.v[0] == 0 && w.pc == 0x202);
    w.set_key(0xB, true);
    w.cycle(); w.cycle();
    CHECK("LD Vx, K resumes with the key", w.v[3] == 0xB && w.v[0] == 1);
}

static void test_errors() {
    bool threw = false;
    try { run_ops({0x5001}, 1); } catch (const std::runtime_error&) { threw = true; }
    CHECK("unknown opcode throws", threw);
    threw = false;
    try { run_ops({0x00EE}, 1); } catch (const std::runtime_error&) { threw = true; }
    CHECK("return on empty stack throws", threw);
    threw = false;
    try { Machine m; m.load_rom(std::vector<uint8_t>(5000, 0)); } catch (const std::runtime_error&) { threw = true; }
    CHECK("oversized rom throws", threw);
    CHECK("disassemble", chip8::disassemble(0xD125) == "DRW V1, V2, 5" && chip8::disassemble(0x00E0) == "CLS" &&
                             chip8::disassemble(0xF533) == "LD B, V5" && chip8::disassemble(0x8AB4) == "ADD VA, VB");
}

static void test_assembler() {
    const char* src =
        "; count with a loop and draw digits\n"
        "start:  LD V0, 0\n"
        "        LD I, sprite\n"
        "loop:   ADD V0, 1\n"
        "        SE V0, 10\n"
        "        JP loop\n"
        "        DRW V1, V2, 3\n"
        "        CALL sub\n"
        "        JP done\n"
        "sub:    LD F, V0\n"
        "        RET\n"
        "done:   JP done\n"
        "sprite: DB 0xF0, %10010000, 240\n";
    const auto rom = chip8::assemble(src);
    const std::vector<uint8_t> want = {0x60, 0x00, 0xA2, 0x16, 0x70, 0x01, 0x30, 0x0A, 0x12, 0x04, 0xD1, 0x23,
                                       0x22, 0x10, 0x12, 0x14, 0xF0, 0x29, 0x00, 0xEE, 0x12, 0x14, 0xF0, 0x90, 0xF0};
    CHECK("assembled bytes", rom == want);
    Machine m;
    m.load_rom(rom);
    for (int i = 0; i < 60; ++i) m.cycle();
    CHECK("assembled program runs", m.v[0] == 10 && m.i == 0x050 + 50 && m.pc == 0x214);
    CHECK("sprite from DB drawn", lit_pixels(m) == 10);  // F0 90 F0 = 4 + 2 + 4 pixels

    CHECK("mnemonics", chip8::assemble("SHR V1\nSHL V2, V3\nJP V0, 0x300\nLD [I], V4\nLD V5, [I]\nLD DT, V6\nSKNP V7\nRND V8, 0x7F\nDW 0xABCD") ==
                           std::vector<uint8_t>({0x81, 0x06, 0x82, 0x3E, 0xB3, 0x00, 0xF4, 0x55, 0xF5, 0x65, 0xF6, 0x15, 0xE7, 0xA1, 0xC8, 0x7F, 0xAB, 0xCD}));
    auto fails = [](const char* s) {
        try { chip8::assemble(s); } catch (const std::runtime_error&) { return true; }
        return false;
    };
    CHECK("unknown mnemonic", fails("FOO V1"));
    CHECK("bad register", fails("LD V1, VZ"));
    CHECK("out of range", fails("LD V1, 300"));
    CHECK("unknown label", fails("JP nowhere"));
    CHECK("duplicate label", fails("a: CLS\na: CLS"));
    CHECK("wrong operand count", fails("DRW V1, V2"));
    try { chip8::assemble("CLS\nCLS\nBAD 1"); } catch (const std::runtime_error& e) {
        CHECK("error reports the line", std::string(e.what()).find("line 3") != std::string::npos);
    }
}

int main() {
    test_arithmetic_and_flow();
    test_memory_timers_random();
    test_display_and_keys();
    test_errors();
    test_assembler();
    std::printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
