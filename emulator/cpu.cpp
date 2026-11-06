#include "emulator/cpu.h"

#include <limits>
#include <iomanip>
#include <sstream>
#include <stdexcept>

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
}  // namespace

CPU::CPU(Memory& memory)
    : memory_(memory) {
    reset();
}

void CPU::reset() {
    registers_.fill(0);
    pc_ = TEXT_BASE;
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

uint32_t CPU::getPC() const {
    return pc_;
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
         << "PC = 0x" << std::setw(8) << pc_ << '\n';
    for (std::size_t i = 0; i < registers_.size(); ++i) {
        text << '$' << names[i] << " (r" << std::dec << i << ") = 0x"
             << std::hex << std::setw(8) << registers_[i] << '\n';
    }
    const auto state = text.str();
    output.write(state.data(), static_cast<std::streamsize>(state.size()));
}

void CPU::step() {
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
                case 0x03:  // sra: explicitly fill the sign bits
                    result = rt >> inst.shamt;
                    if ((rt & 0x80000000u) && inst.shamt != 0) {
                        result |= 0xFFFFFFFFu << (32 - inst.shamt);
                    }
                    break;
                case 0x08:  // jr
                    requireWordAlignment(rs);
                    nextPC = rs;
                    return;
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
        case 0x23: {  // lw
            const uint32_t address = rs + extended;
            requireWordAlignment(address);
            setRegister(inst.rt, memory_.readWord(address));
            return;
        }
        case 0x28:  // sb
            memory_.writeByte(rs + extended, static_cast<uint8_t>(rt));
            return;
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
