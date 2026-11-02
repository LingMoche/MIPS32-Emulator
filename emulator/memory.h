#pragma once

#include <cstdint>
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

private:
    uint32_t baseAddress_;
    std::vector<uint8_t> data_;

    std::size_t toIndex(uint32_t address) const;
};