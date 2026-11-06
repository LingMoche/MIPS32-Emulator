
#include <gtest/gtest.h>

#include "emulator/memory.h"
#include "memory_layout.h"

#include <sstream>
#include <stdexcept>

TEST(MemoryTest, PrintsFullMemoryInAddressedByteRows) {
    Memory memory(TEXT_BASE, 18);
    memory.writeWord(TEXT_BASE, 0x20080005u);
    memory.writeByte(TEXT_BASE + 17, 0xFF);
    std::ostringstream output;
    output.fill('*');
    output.width(12);
    const auto flags = output.flags();
    memory.printState(output);
    EXPECT_EQ(output.str(),
        "Memory state: address=0x00400000, size=18 bytes\n"
        "0x00400000: 05 00 08 20 00 00 00 00 00 00 00 00 00 00 00 00\n"
        "0x00400010: 00 FF\n");
    EXPECT_EQ(memory.readWord(TEXT_BASE), 0x20080005u);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 17), 0xFF);
    EXPECT_EQ(output.flags(), flags);
    EXPECT_EQ(output.fill(), '*');
    EXPECT_EQ(output.width(), 12);
}

TEST(MemoryTest, PrintsUnalignedRangeThroughLastByte) {
    Memory memory(TEXT_BASE, 4);
    memory.writeWord(TEXT_BASE, 0x20080005u);
    std::ostringstream output;
    memory.printState(TEXT_BASE + 1, 3, output);
    EXPECT_EQ(output.str(),
        "Memory state: address=0x00400001, size=3 bytes\n"
        "0x00400001: 00 08 20\n");
}

TEST(MemoryTest, RejectsInvalidPrintRangesBeforeWritingOutput) {
    Memory memory(TEXT_BASE, 4);
    std::ostringstream output;
    EXPECT_THROW(memory.printState(TEXT_BASE - 1, 1, output), std::runtime_error);
    EXPECT_THROW(memory.printState(TEXT_BASE + 3, 2, output), std::runtime_error);
    EXPECT_THROW(memory.printState(TEXT_BASE + 5, 0, output), std::runtime_error);
    EXPECT_THROW(memory.printState(TEXT_BASE, static_cast<std::size_t>(-1), output),
                 std::runtime_error);
    EXPECT_TRUE(output.str().empty());
}

TEST(MemoryTest, PrintsEmptyMemoryAndEmptyRange) {
    Memory memory(TEXT_BASE, 0);
    std::ostringstream output;
    memory.printState(output);
    EXPECT_EQ(output.str(), "Memory state: address=0x00400000, size=0 bytes\n");
    Memory nonempty(TEXT_BASE, 4);
    output.str("");
    nonempty.printState(TEXT_BASE + 4, 0, output);
    EXPECT_EQ(output.str(), "Memory state: address=0x00400004, size=0 bytes\n");
}

TEST(MemoryTest, PrintsLastAddressWithoutWrappingAndRejectsWraparound) {
    Memory memory(0xFFFFFFFEu, 3);
    memory.writeByte(0xFFFFFFFFu, 0xAB);
    std::ostringstream output;
    memory.printState(0xFFFFFFFFu, 1, output);
    EXPECT_EQ(output.str(),
        "Memory state: address=0xFFFFFFFF, size=1 bytes\n"
        "0xFFFFFFFF: AB\n");
    output.str("");
    EXPECT_THROW(memory.printState(output), std::runtime_error);
    EXPECT_TRUE(output.str().empty());
}

TEST(MemoryTest, WritesAndReadsByte) {
    Memory memory(TEXT_BASE, 1024);

    memory.writeByte(TEXT_BASE + 10, 0xAB);

    EXPECT_EQ(
        memory.readByte(TEXT_BASE + 10),
        0xAB
    );
}

TEST(MemoryTest, ReadsLittleEndianWord) {
    Memory memory(TEXT_BASE, 1024);

    memory.writeByte(TEXT_BASE,     0x05);
    memory.writeByte(TEXT_BASE + 1, 0x00);
    memory.writeByte(TEXT_BASE + 2, 0x08);
    memory.writeByte(TEXT_BASE + 3, 0x20);

    EXPECT_EQ(
        memory.readWord(TEXT_BASE),
        0x20080005u
    );
}

TEST(MemoryTest, WritesLittleEndianWord) {
    Memory memory(TEXT_BASE, 1024);

    memory.writeWord(TEXT_BASE, 0x20080005u);

    EXPECT_EQ(memory.readByte(TEXT_BASE),     0x05);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 1), 0x00);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 2), 0x08);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 3), 0x20);
}
