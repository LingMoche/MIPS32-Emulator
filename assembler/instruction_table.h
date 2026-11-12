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
    NONE,
    RD_RS_RT,
    RD_RT_SHAMT,
    RD_RT_RS,
    RD,
    RS,
    RS_RT,

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
