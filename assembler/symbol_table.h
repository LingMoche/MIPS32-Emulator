#pragma once

#include "source_reader.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

using SymbolTable =
    std::unordered_map<std::string, uint32_t>;

bool isLabel(const std::vector<std::string>& tokens);

std::string parseLabel(
    const std::vector<std::string>& tokens,
    int lineNumber = 0
);

SymbolTable buildSymbolTable(const std::vector<SourceLine>& lines);