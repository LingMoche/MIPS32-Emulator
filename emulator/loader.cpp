#include "loader.h"

#include <fstream>
#include <stdexcept>

void loadBinary(
    const std::string& filename,
    Memory& memory,
    uint32_t loadAddress
) {
    std::ifstream file(
        filename,
        std::ios::binary
    );

    if (!file.is_open()) {
        throw std::runtime_error(
            "Failed to open binary file: " + filename
        );
    }

    uint32_t address = loadAddress;

    char byte;

    while (file.get(byte)) {
        memory.writeByte(
            address,
            static_cast<uint8_t>(
                static_cast<unsigned char>(byte)
            )
        );

        address++;
    }
}