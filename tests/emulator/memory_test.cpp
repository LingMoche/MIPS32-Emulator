#include <gtest/gtest.h>

#include "emulator/memory.h"
#include "memory_layout.h"

#include <cstdint>
#include <vector>



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

    memory.writeWord(
        TEXT_BASE,
        0x20080005
    );

    EXPECT_EQ(memory.readByte(TEXT_BASE),     0x05);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 1), 0x00);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 2), 0x08);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 3), 0x20);
}