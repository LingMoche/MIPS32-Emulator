#include "memory.h"

#include <stdexcept>

Memory::Memory(uint32_t baseAddress, std::size_t size)
    : baseAddress_(baseAddress),
      data_(size, 0) {
}

std::size_t Memory::toIndex(uint32_t address) const {
    if (address < baseAddress_) {
        throw std::runtime_error("Memory address below base");
    }

    std::size_t index =
        static_cast<std::size_t>(address - baseAddress_);

    if (index >= data_.size()) {
        throw std::runtime_error("Memory address out of range");
    }

    return index;
}

uint8_t Memory::readByte(uint32_t address) const {
    return data_[toIndex(address)];
}

void Memory::writeByte(
    uint32_t address,
    uint8_t value
) {
    data_[toIndex(address)] = value;
}

uint32_t Memory::readWord(
    uint32_t address
) const {
    uint32_t b0 = readByte(address);
    uint32_t b1 = readByte(address + 1);
    uint32_t b2 = readByte(address + 2);
    uint32_t b3 = readByte(address + 3);

    return
        b0 |
        (b1 << 8) |
        (b2 << 16) |
        (b3 << 24);
}

void Memory::writeWord(
    uint32_t address,
    uint32_t value
) {
    writeByte(
        address,
        static_cast<uint8_t>(value & 0xFF)
    );

    writeByte(
        address + 1,
        static_cast<uint8_t>((value >> 8) & 0xFF)
    );

    writeByte(
        address + 2,
        static_cast<uint8_t>((value >> 16) & 0xFF)
    );

    writeByte(
        address + 3,
        static_cast<uint8_t>((value >> 24) & 0xFF)
    );
}