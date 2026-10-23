#include <iostream>
#include "assembler/source_reader.h"
#include "error.h"

int main() {
    std::string filename = "test.asm";

    std::vector<SourceLine> lines = readSourceFile(filename);

    for (const auto& line : lines) {
        std::cout << line.lineNumber << ": " << line.text << std::endl;
    }

    return 0;
}