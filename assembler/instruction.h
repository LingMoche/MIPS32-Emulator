#pragma once

#include <cstdint>
#include <string>

struct Instruction {
    std::string mnemonic;

    uint8_t rs = 0;
    uint8_t rt = 0;
    uint8_t rd = 0;

    uint8_t shamt = 0;
    int32_t immediate = 0;

    std::string label;
};
