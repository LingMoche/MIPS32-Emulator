#include <gtest/gtest.h>

#include "assembler/assembler.h"
#include "assembler/output_writer.h"
#include "emulator/loader.h"
#include "emulator/memory.h"
#include "memory_layout.h"

#include <cstdint>
#include <filesystem>
#include <vector>

TEST(LoaderIntegrationTest, LoadsAssembledProgram) {
    // Step 1: Prepare assembly source
    std::vector<SourceLine> lines = {
        {1, "main:"},
        {2, "addi $t0, $zero, 5"},
        {3, "addi $t0, $t0, -1"},
        {4, "bne $t0, $zero, main"}
    };

    // Step 2: Assemble
    auto machineCode = assemble(lines);

    ASSERT_EQ(machineCode.size(), 3u);

    EXPECT_EQ(machineCode[0], 0x20080005u);
    EXPECT_EQ(machineCode[1], 0x2108FFFFu);
    EXPECT_EQ(machineCode[2], 0x1500FFFDu);

    // Step 3: Write binary file
    std::filesystem::path path =
        std::filesystem::path(::testing::TempDir()) /
        "loader_integration.bin";

    writeBinaryFile(path.string(), machineCode);

    // Step 4: Create memory and load binary
    Memory memory(TEXT_BASE, 1024);

    loadBinary(path.string(), memory, TEXT_BASE);

    // Step 5: Verify loaded instructions
    EXPECT_EQ(memory.readWord(TEXT_BASE),
              0x20080005u);

    EXPECT_EQ(memory.readWord(TEXT_BASE + 4),
              0x2108FFFFu);

    EXPECT_EQ(memory.readWord(TEXT_BASE + 8),
              0x1500FFFDu);

    // Step 6: Remove temporary file
    std::filesystem::remove(path);
}

TEST(LoaderIntegrationTest, PreservesLittleEndianBytes) {
    std::filesystem::path path =
        std::filesystem::path(::testing::TempDir()) /
        "loader_endian.bin";

    writeBinaryFile(path.string(), {0x20080005u});

    Memory memory(TEXT_BASE, 64);

    loadBinary(path.string(), memory, TEXT_BASE);

    EXPECT_EQ(memory.readByte(TEXT_BASE),     0x05);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 1), 0x00);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 2), 0x08);
    EXPECT_EQ(memory.readByte(TEXT_BASE + 3), 0x20);

    EXPECT_EQ(memory.readWord(TEXT_BASE), 0x20080005u);

    std::filesystem::remove(path);
}