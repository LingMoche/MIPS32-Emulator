#include "memory.h"

#include <stdexcept>
#include <iomanip>
#include <limits>
#include <sstream>

Memory::Memory(uint32_t baseAddress, std::size_t size)
    : baseAddress_(baseAddress),
      data_(size, 0) {
}

void Memory::printState(std::ostream& output) const {
    printState(baseAddress_, data_.size(), output);
}

void Memory::printState(uint32_t address, std::size_t size,
                        std::ostream& output) const {
    if (address < baseAddress_) {
        throw std::runtime_error("Memory address below base");
    }
    const auto start = static_cast<std::size_t>(address - baseAddress_);
    if (start > data_.size() || size > data_.size() - start ||
        (size != 0 && size - 1 > std::numeric_limits<uint32_t>::max() - address)) {
        throw std::runtime_error("Memory range out of bounds");
    }

    std::ostringstream text;
    text << "Memory state: address=0x" << std::hex << std::uppercase
         << std::setfill('0') << std::setw(8) << address
         << ", size=" << std::dec << size << " bytes\n";
    for (std::size_t offset = 0; offset < size; ++offset) {
        if (offset % 16 == 0) {
            text << "0x" << std::hex << std::setw(8)
                 << (address + static_cast<uint32_t>(offset)) << ':';
        }
        text << ' ' << std::hex << std::setw(2)
             << static_cast<unsigned>(data_[start + offset]);
        if (offset % 16 == 15 || offset == size - 1) {
            text << '\n';
        }
    }
    const auto state = text.str();
    output.write(state.data(), static_cast<std::streamsize>(state.size()));
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
