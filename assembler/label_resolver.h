#pragma once

#include "instruction.h"
#include "symbol_table.h"

#include <cstdint>

void resolveLabel(
    Instruction& inst,
    uint32_t pc,
    const SymbolTable& symbols,
    int lineNumber
);