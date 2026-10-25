#include "encoder.h"

#include "instruction_table.h"
#include "../error.h"
#include "symbol_table.h"

uint32_t encodeInstruction(const Instruction& inst, int lineNumber) {
    const InstructionInfo& info =
        instructionInfos.at(inst.mnemonic);

    switch (info.type) {
        case InstructionType::R:
            return
                (static_cast<uint32_t>(info.opcode) << 26) |
                (static_cast<uint32_t>(inst.rs) << 21) |
                (static_cast<uint32_t>(inst.rt) << 16) |
                (static_cast<uint32_t>(inst.rd) << 11) |
                (static_cast<uint32_t>(inst.shamt) << 6) |
                static_cast<uint32_t>(info.funct);

        case InstructionType::I:
            return
            (static_cast<uint32_t>(info.opcode) << 26) |
            (static_cast<uint32_t>(inst.rs) << 21) |
            (static_cast<uint32_t>(inst.rt) << 16) |
            (static_cast<uint32_t>(inst.immediate) & 0xFFFF);
            
        case InstructionType::J:
            return
                (static_cast<uint32_t>(info.opcode) << 26) |
                (inst.target & 0x03FFFFFF);
        default:
            error(inst.mnemonic, "Encoder not implemented for instruction",
                  lineNumber);
    }
}