#include "symbol_table.h"

#include "label.h"
#include "tokenizer.h"
#include "../memory_layout.h"
#include "../error.h"


bool isLabel(const std::vector<std::string>& tokens) {
    return
        tokens.size() == 1 &&
        !tokens[0].empty() &&
        tokens[0].back() == ':';
}

std::string parseLabel(
    const std::vector<std::string>& tokens,
    int lineNumber
) {
    std::string label = tokens[0];
    label.pop_back();

    validateLabelName(label, lineNumber);

    return label;
}

SymbolTable buildSymbolTable(const std::vector<SourceLine>& lines) {
    SymbolTable symbols;

    uint32_t pc = TEXT_BASE;

    for (const auto& line : lines) {
        auto tokens = tokenize(line.text);

        if (tokens.empty()) {
            continue;
        }

        if (isLabel(tokens)) {
            std::string label = parseLabel(tokens, line.lineNumber);

            if (symbols.find(label) != symbols.end()) {
                error(label, "Duplicate label", line.lineNumber);
            }

            symbols[label] = pc;
        }
        else {
            pc += 4;
        }
    }

    return symbols;
}
