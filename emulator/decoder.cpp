#include "emulator/decoder.h"

DecodedInstruction decodeInstruction(uint32_t word) {
    DecodedInstruction inst{};
    inst.opcode = (word >> 26) & 0x3F;
    inst.rs = (word >> 21) & 0x1F;
    inst.rt = (word >> 16) & 0x1F;
    inst.rd = (word >> 11) & 0x1F;
    inst.shamt = (word >> 6) & 0x1F;
    inst.funct = word & 0x3F;
    inst.immediate = word & 0xFFFF;
    inst.target = word & 0x03FFFFFF;
    return inst;
}
