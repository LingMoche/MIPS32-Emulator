#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Write one uppercase, zero-padded 8-digit hexadecimal word per line.
// Existing output files are truncated. I/O failures throw std::runtime_error.
void writeHexFile(
    const std::string& filename,
    const std::vector<uint32_t>& machineCode
);

// Write four bytes per word in little-endian order, without headers or padding.
// Existing output files are truncated. I/O failures throw std::runtime_error.
void writeBinaryFile(
    const std::string& filename,
    const std::vector<uint32_t>& machineCode
);
