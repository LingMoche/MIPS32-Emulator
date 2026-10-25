#include "assembler.h"

#include "encoder.h"
#include "label_resolver.h"
#include "parser.h"
#include "symbol_table.h"
#include "tokenizer.h"

#include "../memory_layout.h"

#include <cstdint>
#include <vector>
#include <string>

std::vector<uint32_t> assemble(
    const std::vector<SourceLine>& lines
) {
    SymbolTable symbols = buildSymbolTable(lines);

    std::vector<uint32_t> machineCode;

    uint32_t pc = TEXT_BASE;

    for (const auto& line : lines) {
        auto tokens = tokenize(line.text);

        // empty line / comment
        if (tokens.empty()) {
            continue;
        }

        // label
        if (isLabel(tokens)) {
            continue;
        }

        Instruction inst =
            parseInstruction(tokens, line.lineNumber);

        resolveLabel(
            inst,
            pc,
            symbols,
            line.lineNumber
        );

        machineCode.push_back(
            encodeInstruction(inst, line.lineNumber)
        );

        pc += 4;
    }

    return machineCode;
}

std::vector<uint32_t> assembleFile(
    const std::string& filename
) {
    return assemble(readSourceFile(filename));
}