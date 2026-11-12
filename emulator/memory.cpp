#include "memory.h"

#include <algorithm>
#include <stdexcept>
#include <iomanip>
#include <limits>
#include <sstream>

Memory::Memory(uint32_t baseAddress, std::size_t size, std::size_t stackSize) {
    constexpr uint64_t addressSpaceSize = uint64_t{1} << 32;
    constexpr uint64_t stackEnd = uint64_t{STACK_TOP} + 4;
    if (size > addressSpaceSize - baseAddress) {
        throw std::runtime_error("Memory region exceeds 32-bit address space");
    }
    if (stackSize > stackEnd || stackSize % 4 != 0) {
        throw std::runtime_error("Invalid stack size");
    }
    const auto stackBase = static_cast<uint32_t>(stackEnd - stackSize);
    if (size != 0 && stackSize != 0 && baseAddress < stackEnd &&
        uint64_t{baseAddress} + size > stackBase) {
        throw std::runtime_error("Memory region overlaps stack");
    }

    regions_.push_back({baseAddress, std::vector<uint8_t>(size, 0)});
    if (stackSize != 0) {
        regions_.push_back({stackBase, std::vector<uint8_t>(stackSize, 0)});
        std::sort(regions_.begin(), regions_.end(),
                  [](const Region& left, const Region& right) {
                      return left.baseAddress < right.baseAddress;
                  });
    }
}

void Memory::printState(std::ostream& output) const {
    for (const auto& region : regions_) {
        printState(region.baseAddress, region.data.size(), output);
    }
}

void Memory::printState(uint32_t address, std::size_t size,
                        std::ostream& output) const {
    const auto& region = regions_[findRegion(address, size)];
    const auto start = static_cast<std::size_t>(address - region.baseAddress);

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
             << static_cast<unsigned>(region.data[start + offset]);
        if (offset % 16 == 15 || offset == size - 1) {
            text << '\n';
        }
    }
    const auto state = text.str();
    output.write(state.data(), static_cast<std::streamsize>(state.size()));
}

std::size_t Memory::findRegion(uint32_t address, std::size_t size) const {
    if (size != 0 && size - 1 > std::numeric_limits<uint32_t>::max() - address) {
        throw std::runtime_error("Memory range wraps around address space");
    }
    for (std::size_t i = 0; i < regions_.size(); ++i) {
        const auto& region = regions_[i];
        if (address >= region.baseAddress) {
            const auto offset = static_cast<std::size_t>(address - region.baseAddress);
            if (offset <= region.data.size() && size <= region.data.size() - offset) {
                return i;
            }
        }
    }
    throw std::runtime_error("Memory range out of bounds");
}

void Memory::validateRange(uint32_t address, std::size_t size) const {
    findRegion(address, size);
}

uint8_t Memory::readByte(uint32_t address) const {
    const auto& region = regions_[findRegion(address, 1)];
    return region.data[address - region.baseAddress];
}

void Memory::writeByte(
    uint32_t address,
    uint8_t value
) {
    auto& region = regions_[findRegion(address, 1)];
    region.data[address - region.baseAddress] = value;
}

uint16_t Memory::readHalfWord(uint32_t address) const {
    const auto& region = regions_[findRegion(address, 2)];
    const auto index = static_cast<std::size_t>(address - region.baseAddress);
    return static_cast<uint16_t>(uint32_t{region.data[index]} |
                                 (uint32_t{region.data[index + 1]} << 8));
}

void Memory::writeHalfWord(uint32_t address, uint16_t value) {
    auto& region = regions_[findRegion(address, 2)];
    const auto index = static_cast<std::size_t>(address - region.baseAddress);
    // Validate both bytes before modifying either byte.
    region.data[index] = static_cast<uint8_t>(value);
    region.data[index + 1] = static_cast<uint8_t>(value >> 8);
}

uint32_t Memory::readWord(
    uint32_t address
) const {
    const auto& region = regions_[findRegion(address, 4)];
    const auto index = static_cast<std::size_t>(address - region.baseAddress);
    uint32_t b0 = region.data[index];
    uint32_t b1 = region.data[index + 1];
    uint32_t b2 = region.data[index + 2];
    uint32_t b3 = region.data[index + 3];

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
    auto& region = regions_[findRegion(address, 4)];
    const auto index = static_cast<std::size_t>(address - region.baseAddress);
    // Validate the entire word before changing any byte.
    for (std::size_t offset = 0; offset < 4; ++offset) {
        region.data[index + offset] = static_cast<uint8_t>(value >> (offset * 8));
    }
}
