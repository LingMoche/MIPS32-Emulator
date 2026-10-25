#include "parser.h"

#include "instruction_table.h"
#include "label.h"
#include "registers.h"
#include "../error.h"

#include <stdexcept>

int32_t parseNumber(const std::string& token, int bits, bool isUnsigned,
                    int lineNumber) {
    const int maxBits = isUnsigned ? 31 : 32;
    const std::string signedness = isUnsigned ? "unsigned" : "signed";
    if (bits < 1 || bits > maxBits) {
        error(token, "Bit count must be between 1 and " + std::to_string(maxBits)
                     + " for " + signedness + " integers", lineNumber);
    }

    const std::string rangeMessage = "Integer does not fit in "
        + std::to_string(bits) + " " + signedness + " bits";
    long long value;
    std::size_t consumed = 0;
    try {
        value = std::stoll(token, &consumed, 0);
    }
    catch (const std::invalid_argument&) {
        error(token, "Invalid integer", lineNumber);
    }
    catch (const std::out_of_range&) {
        error(token, rangeMessage, lineNumber);
    }

    if (consumed != token.size()) {
        error(token, "Invalid integer", lineNumber);
    }

    const long long minValue = isUnsigned ? 0 : -(1LL << (bits - 1));
    const long long maxValue = isUnsigned ? (1LL << bits) - 1
                                         : (1LL << (bits - 1)) - 1;
    if (value < minValue || value > maxValue) {
        error(token, rangeMessage, lineNumber);
    }

    return static_cast<int32_t>(value);
}

Instruction parseInstruction(const std::vector<std::string>& tokens,
                             int lineNumber) {
    if (tokens.empty()) {
        error("", "Cannot parse empty instruction", lineNumber);
    }

    const std::string& mnemonic = tokens[0];
    auto it = instructionInfos.find(mnemonic);

    if (it == instructionInfos.end()) {
        error(mnemonic, "Invalid instruction", lineNumber);
    }

    const InstructionInfo& info = it->second;

    Instruction inst;
    inst.mnemonic = mnemonic;

    switch (info.format) {
        case OperandFormat::RD_RS_RT:
            if (tokens.size() != 4) {
                error(mnemonic, "Expect 3 operands", lineNumber);
            }

            inst.rd = parseRegister(tokens[1], lineNumber);
            inst.rs = parseRegister(tokens[2], lineNumber);
            inst.rt = parseRegister(tokens[3], lineNumber);
            break;

        case OperandFormat::RD_RT_SHAMT: {
            if (tokens.size() != 4) {
                error(mnemonic, "Expect 3 operands", lineNumber);
            }

            inst.rd = parseRegister(tokens[1], lineNumber);
            inst.rt = parseRegister(tokens[2], lineNumber);

            inst.shamt = static_cast<uint8_t>(
                parseNumber(tokens[3], 5, true, lineNumber));
            break;
        }

        case OperandFormat::RS:
            if (tokens.size() != 2) {
                error(mnemonic, "Expect 1 operand", lineNumber);
            }

            inst.rs = parseRegister(tokens[1], lineNumber);
            break;

        case OperandFormat::RT_RS_IMM: {
            if (tokens.size() != 4) {
                error(mnemonic, "Expect 3 operands", lineNumber);
            }

            inst.rt = parseRegister(tokens[1], lineNumber);
            inst.rs = parseRegister(tokens[2], lineNumber);

            // Logical immediates are zero-extended. Arithmetic immediates,
            // including addiu and sltiu, are sign-extended.
            const bool isUnsigned = mnemonic == "andi" || mnemonic == "ori" ||
                                    mnemonic == "xori";
            inst.immediate = parseNumber(tokens[3], 16, isUnsigned, lineNumber);
            break;
        }

        case OperandFormat::RT_IMM:
            if (tokens.size() != 3) {
                error(mnemonic, "Expect 2 operands", lineNumber);
            }

            inst.rt = parseRegister(tokens[1], lineNumber);
            inst.immediate = parseNumber(tokens[2], 16, true, lineNumber);
            break;

        case OperandFormat::RT_OFFSET_RS:
            if (tokens.size() != 4) {
                error(mnemonic, "Expect 3 operands", lineNumber);
            }

            inst.rt = parseRegister(tokens[1], lineNumber);
            inst.immediate = parseNumber(tokens[2], 16, false, lineNumber);
            inst.rs = parseRegister(tokens[3], lineNumber);
            break;

        case OperandFormat::RS_RT_LABEL:
            if (tokens.size() != 4) {
                error(mnemonic, "Expect 3 operands", lineNumber);
            }

            inst.rs = parseRegister(tokens[1], lineNumber);
            inst.rt = parseRegister(tokens[2], lineNumber);
            validateLabelName(tokens[3], lineNumber);
            inst.label = tokens[3];
            break;

        case OperandFormat::TARGET:
            if (tokens.size() != 2) {
                error(mnemonic, "Expect 1 operand", lineNumber);
            }

            validateLabelName(tokens[1], lineNumber);
            inst.label = tokens[1];
            break;

        default:
            error(mnemonic, "Invalid operand format", lineNumber);

    }
    return inst;
}
