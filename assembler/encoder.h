#pragma once

#include "instruction.h"

#include <cstdint>

uint32_t encodeInstruction(const Instruction& inst, int lineNumber = 0);