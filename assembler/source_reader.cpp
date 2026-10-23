#include "source_reader.h"
#include "../error.h"

#include <fstream>

std::vector<SourceLine> readSourceFile(const std::string& filename) {
    std::vector<SourceLine> lines;

    // 1. Open file
    std::ifstream file(filename);

    // 2. Check whether file opened successfully
    if (!file.is_open()) {
        error(filename, "Failed to open file");
    }

    // 3. Variables for current line
    std::string text;
    int lineNumber = 1;

    // 4. Read file line by line
    while (std::getline(file, text)) {
        SourceLine line;

        line.lineNumber = lineNumber;
        line.text = text;

        lines.push_back(line);

        lineNumber++;
    }

    // 5. Return all lines
    return lines;
}