#include "instruction_table.h"

const std::unordered_map<std::string, InstructionInfo> instructionInfos = {
    {"add",   {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x20}},
    {"addu",  {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x21}},
    {"sub",   {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x22}},
    {"subu",  {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x23}},
    {"and",   {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x24}},
    {"or",    {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x25}},
    {"xor",   {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x26}},
    {"nor",   {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x27}},
    {"slt",   {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x2A}},
    {"sltu",  {InstructionType::R, OperandFormat::RD_RS_RT,    0x00, 0x2B}},

    {"sll",   {InstructionType::R, OperandFormat::RD_RT_SHAMT, 0x00, 0x00}},
    {"srl",   {InstructionType::R, OperandFormat::RD_RT_SHAMT, 0x00, 0x02}},
    {"sra",   {InstructionType::R, OperandFormat::RD_RT_SHAMT, 0x00, 0x03}},

    {"jr",    {InstructionType::R, OperandFormat::RS,          0x00, 0x08}},

    {"addi",  {InstructionType::I, OperandFormat::RT_RS_IMM,   0x08, 0x00}},
    {"addiu", {InstructionType::I, OperandFormat::RT_RS_IMM,   0x09, 0x00}},
    {"andi",  {InstructionType::I, OperandFormat::RT_RS_IMM,   0x0C, 0x00}},
    {"ori",   {InstructionType::I, OperandFormat::RT_RS_IMM,   0x0D, 0x00}},
    {"xori",  {InstructionType::I, OperandFormat::RT_RS_IMM,   0x0E, 0x00}},
    {"slti",  {InstructionType::I, OperandFormat::RT_RS_IMM,   0x0A, 0x00}},
    {"sltiu", {InstructionType::I, OperandFormat::RT_RS_IMM,   0x0B, 0x00}},

    {"lui",   {InstructionType::I, OperandFormat::RT_IMM,      0x0F, 0x00}},

    {"lw",    {InstructionType::I, OperandFormat::RT_OFFSET_RS, 0x23, 0x00}},
    {"sw",    {InstructionType::I, OperandFormat::RT_OFFSET_RS, 0x2B, 0x00}},
    {"lb",    {InstructionType::I, OperandFormat::RT_OFFSET_RS, 0x20, 0x00}},
    {"lbu",   {InstructionType::I, OperandFormat::RT_OFFSET_RS, 0x24, 0x00}},
    {"sb",    {InstructionType::I, OperandFormat::RT_OFFSET_RS, 0x28, 0x00}},

    {"beq",   {InstructionType::I, OperandFormat::RS_RT_LABEL,  0x04, 0x00}},
    {"bne",   {InstructionType::I, OperandFormat::RS_RT_LABEL,  0x05, 0x00}},

    {"j",     {InstructionType::J, OperandFormat::TARGET,       0x02, 0x00}},
    {"jal",   {InstructionType::J, OperandFormat::TARGET,       0x03, 0x00}},
};