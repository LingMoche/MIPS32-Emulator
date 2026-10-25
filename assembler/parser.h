#pragma once

#include "instruction.h"

#include <cstdint>
#include <string>
#include <vector>

// Accepts decimal, hexadecimal (0x), and octal (leading 0) integers.
// Signed widths are 1-32 bits; unsigned widths are 1-31 bits to fit int32_t.
// Invalid input is reported through error().
int32_t parseNumber(const std::string& token, int bits, bool isUnsigned,
                    int lineNumber = 0);

// Parses one instruction after tokenize() has removed commas and parentheses.
// Branches and jumps retain symbolic labels for later resolution.
Instruction parseInstruction(const std::vector<std::string>& tokens,
                             int lineNumber = 0);
