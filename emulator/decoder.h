#pragma once

#include <cstdint>


struct DecodedInstruction {
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    uint8_t funct;
    uint16_t immediate;
    uint32_t target;
};

DecodedInstruction decodeInstruction(uint32_t word);