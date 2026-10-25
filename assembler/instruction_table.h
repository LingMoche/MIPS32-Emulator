#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

enum class InstructionType {
    R,
    I,
    J
};

enum class OperandFormat {
    RD_RS_RT,
    RD_RT_SHAMT,
    RS,

    RT_RS_IMM,
    RT_IMM,
    RT_OFFSET_RS,
    RS_RT_LABEL,

    TARGET
};

struct InstructionInfo {
    InstructionType type;
    OperandFormat format;
    uint8_t opcode;
    uint8_t funct;
};

extern const std::unordered_map<std::string, InstructionInfo> instructionInfos;