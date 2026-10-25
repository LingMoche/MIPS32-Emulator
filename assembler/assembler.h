#pragma once

#include "source_reader.h"

#include <cstdint>
#include <vector>

std::vector<uint32_t> assemble(
    const std::vector<SourceLine>& lines
);

std::vector<uint32_t> assembleFile(
    const std::string& filename
);