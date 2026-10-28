#pragma once

#include "memory.h"

#include <cstdint>
#include <string>

void loadBinary(
    const std::string& filename,
    Memory& memory,
    uint32_t loadAddress
);