#include "emulator/cpu.h"

#include <limits>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <locale>
#include <string>

namespace {
// Interpret two's-complement bits without implementation-defined signed casts.
int64_t signedWord(uint32_t value) {
    return value <= 0x7FFFFFFFu
        ? static_cast<int64_t>(value)
        : static_cast<int64_t>(value) - 0x100000000LL;
}

uint32_t checkedResult(int64_t value) {
    if (value < std::numeric_limits<int32_t>::min() ||
        value > std::numeric_limits<int32_t>::max()) {
        throw std::overflow_error("Signed arithmetic overflow");
    }
    return static_cast<uint32_t>(value);
}

void requireWordAlignment(uint32_t address) {
    if ((address & 3u) != 0) {
        throw std::runtime_error("Unaligned word address");
    }
}

void requireHalfWordAlignment(uint32_t address) {
    if ((address & 1u) != 0) {
        throw std::runtime_error("Unaligned halfword address");
    }
}

uint32_t arithmeticShiftRight(uint32_t value, uint32_t amount) {
    uint32_t result = value >> amount;
    if ((value & 0x80000000u) && amount != 0) {
        result |= 0xFFFFFFFFu << (32 - amount);
    }
    return result;
}
}  // namespace

CPU::CPU(Memory& memory, std::istream& input, std::ostream& output)
    : memory_(memory), input_(input), output_(output) {
    reset();
}

void CPU::reset() {
    registers_.fill(0);
    pc_ = TEXT_BASE;
    hi_ = 0;
    lo_ = 0;
    halted_ = false;
    exitCode_ = 0;
    registers_[29] = STACK_TOP;
}

uint32_t CPU::getRegister(uint8_t index) const {
    if (index >= 32) {
        throw std::out_of_range("Invalid register index");
    }
    return registers_[index];
}

void CPU::setRegister(uint8_t index, uint32_t value) {
    if (index >= 32) {
        throw std::out_of_range("Invalid register index");
    }
    if (index != 0) {
        registers_[index] = value;
    }
}

uint32_t CPU::getHI() const {
    return hi_;
}

uint32_t CPU::getLO() const {
    return lo_;
}

uint32_t CPU::getPC() const {
    return pc_;
}

bool CPU::isHalted() const {
    return halted_;
}

int32_t CPU::getExitCode() const {
    return exitCode_;
}

void CPU::printState(std::ostream& output) const {
    static constexpr const char* names[] = {
        "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
        "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
        "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
    };
    std::ostringstream text;
    text << "CPU state\n" << std::hex << std::uppercase << std::setfill('0')
         << "PC = 0x" << std::setw(8) << pc_ << '\n'
         << "HI = 0x" << std::setw(8) << hi_ << '\n'
         << "LO = 0x" << std::setw(8) << lo_ << '\n';
    for (std::size_t i = 0; i < registers_.size(); ++i) {
        text << '$' << names[i] << " (r" << std::dec << i << ") = 0x"
             << std::hex << std::setw(8) << registers_[i] << '\n';
    }
    const auto state = text.str();
    output.write(state.data(), static_cast<std::streamsize>(state.size()));
}

void CPU::step() {
    if (halted_) {
        return;
    }
    requireWordAlignment(pc_);
    const auto inst = decodeInstruction(memory_.readWord(pc_));
    uint32_t nextPC = pc_ + 4;
    execute(inst, nextPC);
    pc_ = nextPC;
}

void CPU::execute(const DecodedInstruction& inst, uint32_t& nextPC) {
    const uint32_t rs = getRegister(inst.rs);
    const uint32_t rt = getRegister(inst.rt);
    const int32_t immediate = inst.immediate < 0x8000u
        ? static_cast<int32_t>(inst.immediate)
        : static_cast<int32_t>(inst.immediate) - 0x10000;
    const uint32_t extended = static_cast<uint32_t>(immediate);

    switch (inst.opcode) {
        case 0x00: {
            uint32_t result;
            switch (inst.funct) {
                case 0x00: result = rt << inst.shamt; break;  // sll / nop
                case 0x02: result = rt >> inst.shamt; break;  // srl
                case 0x03: result = arithmeticShiftRight(rt, inst.shamt); break;
                case 0x04: result = rt << (rs & 31u); break;  // sllv
                case 0x06: result = rt >> (rs & 31u); break;  // srlv
                case 0x07: result = arithmeticShiftRight(rt, rs & 31u); break;
                case 0x08:  // jr
                    requireWordAlignment(rs);
                    nextPC = rs;
                    return;
                case 0x0C:
                    executeSyscall();
                    return;
                case 0x10: result = hi_; break;  // mfhi
                case 0x12: result = lo_; break;  // mflo
                case 0x11: hi_ = rs; return;     // mthi
                case 0x13: lo_ = rs; return;     // mtlo
                case 0x18:  // mult
                case 0x19: { // multu
                    const uint64_t product = inst.funct == 0x18
                        ? static_cast<uint64_t>(signedWord(rs) * signedWord(rt))
                        : uint64_t{rs} * rt;
                    hi_ = static_cast<uint32_t>(product >> 32);
                    lo_ = static_cast<uint32_t>(product);
                    return;
                }
                case 0x1A:  // div
                case 0x1B: { // divu
                    if (rt == 0) {
                        throw std::runtime_error("Division by zero");
                    }
                    if (inst.funct == 0x1A) {
                        if (rs == 0x80000000u && rt == 0xFFFFFFFFu) {
                            throw std::overflow_error("Signed division overflow");
                        }
                        const auto dividend = signedWord(rs);
                        const auto divisor = signedWord(rt);
                        lo_ = static_cast<uint32_t>(dividend / divisor);
                        hi_ = static_cast<uint32_t>(dividend % divisor);
                    } else {
                        lo_ = rs / rt;
                        hi_ = rs % rt;
                    }
                    return;
                }
                case 0x20: result = checkedResult(signedWord(rs) + signedWord(rt)); break;
                case 0x21: result = rs + rt; break;  // addu
                case 0x22: result = checkedResult(signedWord(rs) - signedWord(rt)); break;
                case 0x23: result = rs - rt; break;  // subu
                case 0x24: result = rs & rt; break;
                case 0x25: result = rs | rt; break;
                case 0x26: result = rs ^ rt; break;
                case 0x27: result = ~(rs | rt); break;
                case 0x2A: result = signedWord(rs) < signedWord(rt); break;
                case 0x2B: result = rs < rt; break;
                default: throw std::runtime_error("Unsupported R-type instruction");
            }
            setRegister(inst.rd, result);
            return;
        }
        case 0x02:  // j
        case 0x03:  // jal (no delay slot)
            if (inst.opcode == 0x03) {
                setRegister(31, nextPC);
            }
            nextPC = (nextPC & 0xF0000000u) | (inst.target << 2);
            return;
        case 0x04:  // beq
        case 0x05:  // bne
            if ((rs == rt) == (inst.opcode == 0x04)) {
                nextPC += static_cast<uint32_t>(immediate * 4);
            }
            return;
        case 0x08: setRegister(inst.rt, checkedResult(signedWord(rs) + immediate)); return;
        case 0x09: setRegister(inst.rt, rs + extended); return;
        case 0x0A: setRegister(inst.rt, signedWord(rs) < immediate); return;
        case 0x0B: setRegister(inst.rt, rs < extended); return;
        case 0x0C: setRegister(inst.rt, rs & inst.immediate); return;
        case 0x0D: setRegister(inst.rt, rs | inst.immediate); return;
        case 0x0E: setRegister(inst.rt, rs ^ inst.immediate); return;
        case 0x0F: setRegister(inst.rt, static_cast<uint32_t>(inst.immediate) << 16); return;
        case 0x20:  // lb
        case 0x24: {  // lbu
            uint32_t value = memory_.readByte(rs + extended);
            if (inst.opcode == 0x20 && (value & 0x80u)) {
                value |= 0xFFFFFF00u;
            }
            setRegister(inst.rt, value);
            return;
        }
        case 0x21:  // lh
        case 0x25: { // lhu
            const uint32_t address = rs + extended;
            requireHalfWordAlignment(address);
            uint32_t value = memory_.readHalfWord(address);
            if (inst.opcode == 0x21 && (value & 0x8000u)) {
                value |= 0xFFFF0000u;
            }
            setRegister(inst.rt, value);
            return;
        }
        case 0x23: {  // lw
            const uint32_t address = rs + extended;
            requireWordAlignment(address);
            setRegister(inst.rt, memory_.readWord(address));
            return;
        }
        case 0x28:  // sb
            memory_.writeByte(rs + extended, static_cast<uint8_t>(rt));
            return;
        case 0x29: { // sh
            const uint32_t address = rs + extended;
            requireHalfWordAlignment(address);
            memory_.writeHalfWord(address, static_cast<uint16_t>(rt));
            return;
        }
        case 0x2B: {  // sw
            const uint32_t address = rs + extended;
            requireWordAlignment(address);
            // Validate the whole access before modifying any byte.
            memory_.readWord(address);
            memory_.writeWord(address, rt);
            return;
        }
        default: throw std::runtime_error("Unsupported instruction");
    }
}

void CPU::executeSyscall() {
    const uint32_t service = getRegister(2);  // $v0
    const uint32_t argument = getRegister(4); // $a0
    std::string text;
    switch (service) {
        case 1: // print signed integer in decimal, regardless of stream flags
            text = std::to_string(signedWord(argument));
            break;
        case 4: { // print null-terminated bytes; validate before any output
            uint64_t address = argument;
            for (;;) {
                if (address > std::numeric_limits<uint32_t>::max()) {
                    throw std::runtime_error("Syscall string wraps around address space");
                }
                const auto byte = memory_.readByte(static_cast<uint32_t>(address++));
                if (byte == 0) {
                    break;
                }
                text.push_back(static_cast<char>(byte));
            }
            break;
        }
        case 5: { // consume one decimal integer line
            std::string line;
            if (!std::getline(input_, line) || input_.bad()) {
                throw std::runtime_error("Syscall failed to read integer");
            }
            std::istringstream parser(line);
            parser.imbue(std::locale::classic());
            int64_t value;
            if (!(parser >> value) || value < std::numeric_limits<int32_t>::min() ||
                value > std::numeric_limits<int32_t>::max()) {
                throw std::runtime_error("Syscall expected a signed 32-bit decimal integer");
            }
            parser >> std::ws;
            if (!parser.eof()) {
                throw std::runtime_error("Syscall expected a signed 32-bit decimal integer");
            }
            setRegister(2, static_cast<uint32_t>(value));
            return;
        }
        case 8: { // fgets-style read into $a0, capacity (including NUL) in $a1
            const auto length = signedWord(getRegister(5));
            if (length <= 0) {
                return;
            }
            memory_.validateRange(argument, static_cast<std::size_t>(length));
            for (int64_t i = 0; i < length - 1; ++i) {
                const auto byte = input_.get();
                if (byte == std::char_traits<char>::eof()) {
                    if (input_.bad() || !input_.eof() || text.empty()) {
                        throw std::runtime_error("Syscall failed to read string");
                    }
                    break;
                }
                text.push_back(static_cast<char>(byte));
                if (byte == '\n') {
                    break;
                }
            }
            // All input and range validation succeeded before changing memory.
            for (std::size_t i = 0; i < text.size(); ++i) {
                memory_.writeByte(argument + static_cast<uint32_t>(i),
                                  static_cast<uint8_t>(text[i]));
            }
            memory_.writeByte(argument + static_cast<uint32_t>(text.size()), 0);
            return;
        }
        case 10: // terminate only the emulated program
        case 17:
            exitCode_ = service == 10 ? 0 : static_cast<int32_t>(signedWord(argument));
            halted_ = true;
            return;
        case 11:
            text.push_back(static_cast<char>(argument & 0xFFu));
            break;
        case 12: {
            const auto byte = input_.get();
            if (byte == std::char_traits<char>::eof() || input_.bad()) {
                throw std::runtime_error("Syscall failed to read character");
            }
            setRegister(2, static_cast<unsigned char>(byte));
            return;
        }
        default:
            throw std::runtime_error("Unsupported syscall service: " + std::to_string(service));
    }
    output_.write(text.data(), static_cast<std::streamsize>(text.size()));
    output_.flush();
    if (!output_) {
        throw std::runtime_error("Syscall failed to write output");
    }
}
