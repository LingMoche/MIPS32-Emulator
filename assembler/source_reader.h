#pragma once

#include <string>
#include <vector>

struct SourceLine {
    int lineNumber;
    std::string text;
};

std::vector<SourceLine> readSourceFile(const std::string& filename);