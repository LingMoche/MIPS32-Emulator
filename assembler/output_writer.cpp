#include "output_writer.h"

#include "../error.h"

#include <fstream>
#include <iomanip>
#include <locale>

namespace {

void finishOutput(std::ofstream& output, const std::string& filename) {
    // Closing flushes buffered bytes and can fail even if every write succeeded.
    output.close();
    if (!output) {
        error(filename, "Failed to write output file");
    }
}

} // namespace

void writeHexFile(
    const std::string& filename,
    const std::vector<uint32_t>& machineCode
) {
    // Binary mode keeps the line separator a single LF on every platform.
    std::ofstream output(filename, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error(filename, "Failed to open output file");
    }

    output.imbue(std::locale::classic());
    output << std::hex << std::uppercase << std::setfill('0');
    for (uint32_t code : machineCode) {
        output << std::setw(8) << code << '\n';
        if (!output) {
            error(filename, "Failed to write output file");
        }
    }

    finishOutput(output, filename);
}

void writeBinaryFile(
    const std::string& filename,
    const std::vector<uint32_t>& machineCode
) {
    std::ofstream output(filename, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        error(filename, "Failed to open output file");
    }

    for (uint32_t code : machineCode) {
        const unsigned char bytes[] = {
            static_cast<unsigned char>(code & 0xFFu),
            static_cast<unsigned char>((code >> 8) & 0xFFu),
            static_cast<unsigned char>((code >> 16) & 0xFFu),
            static_cast<unsigned char>((code >> 24) & 0xFFu),
        };
        output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
        if (!output) {
            error(filename, "Failed to write output file");
        }
    }

    finishOutput(output, filename);
}
