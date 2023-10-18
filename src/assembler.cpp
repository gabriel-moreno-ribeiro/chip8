#include "assembler.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>

namespace chip8 {

namespace {

struct Line {
    int number;
    std::string label;
    std::string mnemonic;
    std::vector<std::string> args;
};

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return "";
    const auto b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

std::vector<Line> parse(const std::string& source) {
    std::vector<Line> lines;
    std::istringstream in(source);
    std::string raw;
    int number = 0;
    while (std::getline(in, raw)) {
        ++number;
        auto text = raw;
        const auto comment = text.find(';');
        if (comment != std::string::npos) text = text.substr(0, comment);
        text = trim(text);
        if (text.empty()) continue;
        Line line{number, "", "", {}};
        const auto colon = text.find(':');
        if (colon != std::string::npos && text.find(' ') > colon) {
            line.label = upper(trim(text.substr(0, colon)));
            text = trim(text.substr(colon + 1));
        }
        if (!text.empty()) {
            const auto space = text.find_first_of(" \t");
            line.mnemonic = upper(text.substr(0, space));
            if (space != std::string::npos) {
                std::string rest = text.substr(space + 1);
                std::string arg;
                std::istringstream args(rest);
                while (std::getline(args, arg, ',')) {
                    arg = trim(arg);
                    if (!arg.empty()) line.args.push_back(upper(arg));
                }
            }
        }
        lines.push_back(line);
    }
    return lines;
}

[[noreturn]] void fail(const Line& line, const std::string& msg) {
    throw std::runtime_error("line " + std::to_string(line.number) + ": " + msg);
}

bool is_register(const std::string& a) {
    return a.size() == 2 && a[0] == 'V' && std::isxdigit(static_cast<unsigned char>(a[1]));
}

int reg(const Line& line, const std::string& a) {
    if (!is_register(a)) fail(line, "expected a register, got " + a);
    return std::stoi(a.substr(1), nullptr, 16);
}

int number_or_label(const Line& line, const std::string& a, const std::map<std::string, int>& labels, int max) {
    int value;
    if (labels.count(a)) {
        value = labels.at(a);
    } else {
        try {
            if (a.size() > 2 && a[0] == '0' && a[1] == 'X') value = std::stoi(a.substr(2), nullptr, 16);
            else if (a.size() > 1 && a[0] == '#') value = std::stoi(a.substr(1), nullptr, 16);
            else if (a.size() > 1 && a[0] == '$') value = std::stoi(a.substr(1), nullptr, 16);
            else if (a.size() > 1 && a[0] == '%') value = std::stoi(a.substr(1), nullptr, 2);
            else value = std::stoi(a, nullptr, 10);
        } catch (const std::exception&) {
            fail(line, "bad number or unknown label " + a);
        }
    }
    if (value < 0 || value > max) fail(line, "value out of range: " + a);
    return value;
}

// Bytes this line will occupy (for the first pass).
int size_of(const Line& line) {
    if (line.mnemonic.empty()) return 0;
    if (line.mnemonic == "DB") return static_cast<int>(line.args.size());
    if (line.mnemonic == "DW") return static_cast<int>(line.args.size()) * 2;
    return 2;
}

uint16_t encode(const Line& line, const std::map<std::string, int>& labels) {
    const auto& m = line.mnemonic;
    const auto& a = line.args;
    auto need = [&](size_t count) {
        if (a.size() != count) fail(line, m + " takes " + std::to_string(count) + " operand(s)");
    };
    auto xy = [&](uint16_t base) {
        need(2);
        return static_cast<uint16_t>(base | (reg(line, a[0]) << 8) | (reg(line, a[1]) << 4));
    };
    if (m == "CLS") { need(0); return 0x00E0; }
    if (m == "RET") { need(0); return 0x00EE; }
    if (m == "SYS") { need(1); return static_cast<uint16_t>(number_or_label(line, a[0], labels, 0xFFF)); }
    if (m == "JP") {
        if (a.size() == 2 && a[0] == "V0") return static_cast<uint16_t>(0xB000 | number_or_label(line, a[1], labels, 0xFFF));
        need(1);
        return static_cast<uint16_t>(0x1000 | number_or_label(line, a[0], labels, 0xFFF));
    }
    if (m == "CALL") { need(1); return static_cast<uint16_t>(0x2000 | number_or_label(line, a[0], labels, 0xFFF)); }
    if (m == "SE") {
        need(2);
        if (is_register(a[1])) return xy(0x5000);
        return static_cast<uint16_t>(0x3000 | (reg(line, a[0]) << 8) | number_or_label(line, a[1], labels, 0xFF));
    }
    if (m == "SNE") {
        need(2);
        if (is_register(a[1])) return xy(0x9000);
        return static_cast<uint16_t>(0x4000 | (reg(line, a[0]) << 8) | number_or_label(line, a[1], labels, 0xFF));
    }
    if (m == "LD") {
        need(2);
        if (a[0] == "I") return static_cast<uint16_t>(0xA000 | number_or_label(line, a[1], labels, 0xFFF));
        if (a[0] == "DT") return static_cast<uint16_t>(0xF015 | (reg(line, a[1]) << 8));
        if (a[0] == "ST") return static_cast<uint16_t>(0xF018 | (reg(line, a[1]) << 8));
        if (a[0] == "F") return static_cast<uint16_t>(0xF029 | (reg(line, a[1]) << 8));
        if (a[0] == "B") return static_cast<uint16_t>(0xF033 | (reg(line, a[1]) << 8));
        if (a[0] == "[I]") return static_cast<uint16_t>(0xF055 | (reg(line, a[1]) << 8));
        const int x = reg(line, a[0]);
        if (a[1] == "DT") return static_cast<uint16_t>(0xF007 | (x << 8));
        if (a[1] == "K") return static_cast<uint16_t>(0xF00A | (x << 8));
        if (a[1] == "[I]") return static_cast<uint16_t>(0xF065 | (x << 8));
        if (is_register(a[1])) return xy(0x8000);
        return static_cast<uint16_t>(0x6000 | (x << 8) | number_or_label(line, a[1], labels, 0xFF));
    }
    if (m == "ADD") {
        need(2);
        if (a[0] == "I") return static_cast<uint16_t>(0xF01E | (reg(line, a[1]) << 8));
        if (is_register(a[1])) return xy(0x8004);
        return static_cast<uint16_t>(0x7000 | (reg(line, a[0]) << 8) | number_or_label(line, a[1], labels, 0xFF));
    }
    if (m == "OR") return xy(0x8001);
    if (m == "AND") return xy(0x8002);
    if (m == "XOR") return xy(0x8003);
    if (m == "SUB") return xy(0x8005);
    if (m == "SUBN") return xy(0x8007);
    if (m == "SHR") {
        if (a.size() == 1) return static_cast<uint16_t>(0x8006 | (reg(line, a[0]) << 8));
        return xy(0x8006);
    }
    if (m == "SHL") {
        if (a.size() == 1) return static_cast<uint16_t>(0x800E | (reg(line, a[0]) << 8));
        return xy(0x800E);
    }
    if (m == "RND") { need(2); return static_cast<uint16_t>(0xC000 | (reg(line, a[0]) << 8) | number_or_label(line, a[1], labels, 0xFF)); }
    if (m == "DRW") {
        need(3);
        return static_cast<uint16_t>(0xD000 | (reg(line, a[0]) << 8) | (reg(line, a[1]) << 4) | number_or_label(line, a[2], labels, 0xF));
    }
    if (m == "SKP") { need(1); return static_cast<uint16_t>(0xE09E | (reg(line, a[0]) << 8)); }
    if (m == "SKNP") { need(1); return static_cast<uint16_t>(0xE0A1 | (reg(line, a[0]) << 8)); }
    fail(line, "unknown mnemonic " + m);
}

}  // namespace

std::vector<uint8_t> assemble(const std::string& source) {
    const auto lines = parse(source);
    // first pass: label addresses
    std::map<std::string, int> labels;
    int address = 0x200;
    for (const auto& line : lines) {
        if (!line.label.empty()) {
            if (labels.count(line.label)) fail(line, "duplicate label " + line.label);
            labels[line.label] = address;
        }
        address += size_of(line);
    }
    // second pass: encode
    std::vector<uint8_t> out;
    for (const auto& line : lines) {
        if (line.mnemonic.empty()) continue;
        if (line.mnemonic == "DB") {
            for (const auto& a : line.args) out.push_back(static_cast<uint8_t>(number_or_label(line, a, labels, 0xFF)));
        } else if (line.mnemonic == "DW") {
            for (const auto& a : line.args) {
                const int w = number_or_label(line, a, labels, 0xFFFF);
                out.push_back(static_cast<uint8_t>(w >> 8));
                out.push_back(static_cast<uint8_t>(w & 0xFF));
            }
        } else {
            const uint16_t op = encode(line, labels);
            out.push_back(static_cast<uint8_t>(op >> 8));
            out.push_back(static_cast<uint8_t>(op & 0xFF));
        }
    }
    return out;
}

}  // namespace chip8
