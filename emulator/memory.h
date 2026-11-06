#pragma once

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <vector>

class Memory {
public:
    explicit Memory(
        uint32_t baseAddress,
        std::size_t size
    );

    uint8_t readByte(uint32_t address) const;
    void writeByte(uint32_t address, uint8_t value);

    uint32_t readWord(uint32_t address) const;
    void writeWord(uint32_t address, uint32_t value);

    // Print all memory, or a byte range, as addressed hexadecimal rows.
    // Invalid ranges throw before producing any output; an empty range is valid
    // at any address from the base through the end of the allocation.
    void printState(std::ostream& output = std::cout) const;
    void printState(uint32_t address, std::size_t size,
                    std::ostream& output = std::cout) const;

private:
    uint32_t baseAddress_;
    std::vector<uint8_t> data_;

    std::size_t toIndex(uint32_t address) const;
};
