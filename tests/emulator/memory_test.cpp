
#include <gtest/gtest.h>

#include "emulator/memory.h"
#include "memory_layout.h"

#include <sstream>
#include <stdexcept>

TEST(MemoryTest, PrintsFullMemoryInAddressedByteRows) {
    Memory memory(TEXT_BASE, 18, 0);
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
    Memory memory(TEXT_BASE, 0, 0);
    std::ostringstream output;
    memory.printState(output);
    EXPECT_EQ(output.str(), "Memory state: address=0x00400000, size=0 bytes\n");
    Memory nonempty(TEXT_BASE, 4);
    output.str("");
    nonempty.printState(TEXT_BASE + 4, 0, output);
    EXPECT_EQ(output.str(), "Memory state: address=0x00400004, size=0 bytes\n");
}

TEST(MemoryTest, PrintsLastAddressWithoutWrappingAndRejectsWraparound) {
    Memory memory(0xFFFFFFFEu, 2, 0);
    memory.writeByte(0xFFFFFFFFu, 0xAB);
    std::ostringstream output;
    memory.printState(0xFFFFFFFFu, 1, output);
    EXPECT_EQ(output.str(),
        "Memory state: address=0xFFFFFFFF, size=1 bytes\n"
        "0xFFFFFFFF: AB\n");
    output.str("");
    EXPECT_THROW(memory.printState(0xFFFFFFFFu, 2, output), std::runtime_error);
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

TEST(MemoryTest, MapsZeroInitializedStackSeparatelyFromText) {
    Memory memory(TEXT_BASE, 16);
    EXPECT_EQ(STACK_BASE, 0x7FF00000u);
    EXPECT_EQ(memory.readWord(STACK_BASE), 0u);
    EXPECT_EQ(memory.readWord(STACK_TOP), 0u);
    memory.writeWord(TEXT_BASE, 0x20080005u);
    memory.writeWord(STACK_TOP, 0x12345680u);
    memory.writeWord(STACK_BASE, 0xABCDEF01u);
    EXPECT_EQ(memory.readWord(TEXT_BASE), 0x20080005u);
    EXPECT_EQ(memory.readWord(STACK_BASE), 0xABCDEF01u);
    EXPECT_EQ(memory.readWord(STACK_TOP), 0x12345680u);
    EXPECT_EQ(memory.readByte(STACK_TOP), 0x80);
    EXPECT_EQ(memory.readByte(STACK_TOP + 3), 0x12);
}

TEST(MemoryTest, RejectsAccessOutsideMappedRegions) {
    Memory memory(TEXT_BASE, 16);
    for (const auto address : {TEXT_BASE - 1, TEXT_BASE + 16, STACK_BASE - 1,
                               STACK_TOP + 4, 0xFFFFFFFFu}) {
        SCOPED_TRACE(address);
        EXPECT_THROW(memory.readByte(address), std::runtime_error);
        EXPECT_THROW(memory.writeByte(address, 0xAA), std::runtime_error);
    }
}

TEST(MemoryTest, RejectsWordCrossingStackEndBeforeChangingBytes) {
    Memory memory(TEXT_BASE, 16);
    memory.writeWord(STACK_TOP, 0x12345678u);
    for (uint32_t offset = 1; offset <= 3; ++offset) {
        EXPECT_THROW(memory.readWord(STACK_TOP + offset), std::runtime_error);
        EXPECT_THROW(memory.writeWord(STACK_TOP + offset, 0), std::runtime_error);
        EXPECT_EQ(memory.readWord(STACK_TOP), 0x12345678u);
    }
    EXPECT_THROW(memory.writeWord(STACK_BASE - 1, 0), std::runtime_error);
    EXPECT_EQ(memory.readWord(STACK_BASE), 0u);
}

TEST(MemoryTest, RejectsWordAddressWraparoundBeforeChangingBytes) {
    Memory memory(0xFFFFFFFCu, 4, 0);
    memory.writeWord(0xFFFFFFFCu, 0x12345678u);
    EXPECT_THROW(memory.readWord(0xFFFFFFFDu), std::runtime_error);
    EXPECT_THROW(memory.writeWord(0xFFFFFFFDu, 0), std::runtime_error);
    EXPECT_EQ(memory.readWord(0xFFFFFFFCu), 0x12345678u);
}

TEST(MemoryTest, SupportsCustomStackSizeAndDisablingStack) {
    Memory memory(TEXT_BASE, 16, 8);
    memory.writeWord(STACK_TOP - 4, 0x12345678u);
    memory.writeWord(STACK_TOP, 0xABCDEF01u);
    EXPECT_EQ(memory.readWord(STACK_TOP - 4), 0x12345678u);
    EXPECT_EQ(memory.readWord(STACK_TOP), 0xABCDEF01u);
    EXPECT_THROW(memory.readByte(STACK_TOP - 5), std::runtime_error);
    Memory withoutStack(TEXT_BASE, 16, 0);
    EXPECT_THROW(withoutStack.readByte(STACK_TOP), std::runtime_error);
}

TEST(MemoryTest, RejectsInvalidOrOverlappingRegionsBeforeAllocating) {
    EXPECT_THROW((Memory(TEXT_BASE, 16, 3)), std::runtime_error);
    EXPECT_THROW((Memory(TEXT_BASE, 16, 0x80000004ull)), std::runtime_error);
    EXPECT_THROW((Memory(STACK_BASE - 4, 8)), std::runtime_error);
    EXPECT_THROW((Memory(STACK_TOP, 4)), std::runtime_error);
    EXPECT_THROW((Memory(0xFFFFFFFEu, 3, 0)), std::runtime_error);
    EXPECT_THROW((Memory(TEXT_BASE, static_cast<std::size_t>(-1))),
                 std::runtime_error);
    EXPECT_NO_THROW((Memory(STACK_BASE - 4, 4)));
    EXPECT_NO_THROW((Memory(STACK_TOP + 4, 4)));
}

TEST(MemoryTest, StackStorageBelongsToEachMemoryInstance) {
    Memory first(TEXT_BASE, 4);
    Memory second(TEXT_BASE, 4);
    first.writeWord(STACK_TOP, 42);
    EXPECT_EQ(second.readWord(STACK_TOP), 0u);
}

TEST(MemoryTest, PrintsAllMappedRegionsAndStackRanges) {
    Memory memory(TEXT_BASE, 4, 8);
    memory.writeWord(TEXT_BASE, 0x20080005u);
    memory.writeWord(STACK_TOP, 0x12345678u);
    std::ostringstream output;
    memory.printState(output);
    EXPECT_EQ(output.str(),
        "Memory state: address=0x00400000, size=4 bytes\n"
        "0x00400000: 05 00 08 20\n"
        "Memory state: address=0x7FFFFFF8, size=8 bytes\n"
        "0x7FFFFFF8: 00 00 00 00 78 56 34 12\n");
    output.str("");
    memory.printState(STACK_TOP + 1, 3, output);
    EXPECT_EQ(output.str(),
        "Memory state: address=0x7FFFFFFD, size=3 bytes\n"
        "0x7FFFFFFD: 56 34 12\n");
    output.str("");
    EXPECT_THROW(memory.printState(STACK_TOP, 5, output), std::runtime_error);
    EXPECT_THROW(memory.printState(TEXT_BASE, STACK_TOP - TEXT_BASE, output),
                 std::runtime_error);
    EXPECT_TRUE(output.str().empty());
    memory.printState(STACK_TOP + 4, 0, output);
    EXPECT_EQ(output.str(), "Memory state: address=0x80000000, size=0 bytes\n");
}
