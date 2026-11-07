#pragma once

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <vector>

#include "memory_layout.h"

class Memory {
public:
    // Map the requested region and a separate, zero-initialized stack ending
    // at STACK_TOP + 4 (exclusive). Set stackSize to zero to omit the stack.
    // Regions must not overlap or extend beyond the 32-bit address space;
    // a nonempty stack size must be a multiple of four.
    explicit Memory(
        uint32_t baseAddress,
        std::size_t size,
        std::size_t stackSize = STACK_SIZE
    );

    uint8_t readByte(uint32_t address) const;
    void writeByte(uint32_t address, uint8_t value);

    uint32_t readWord(uint32_t address) const;
    void writeWord(uint32_t address, uint32_t value);

    // Print all mapped regions, or a byte range within one mapped region.
    // Invalid ranges throw before producing any output; an empty range is valid
    // at any address from the base through the end of the allocation.
    void printState(std::ostream& output = std::cout) const;
    void printState(uint32_t address, std::size_t size,
                    std::ostream& output = std::cout) const;

private:
    struct Region {
        uint32_t baseAddress;
        std::vector<uint8_t> data;
    };
    std::vector<Region> regions_;

    std::size_t findRegion(uint32_t address, std::size_t size) const;
};
