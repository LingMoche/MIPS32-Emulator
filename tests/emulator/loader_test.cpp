#include <gtest/gtest.h>

#include "assembler/assembler.h"
#include "assembler/output_writer.h"
#include "emulator/loader.h"
#include "emulator/cpu.h"
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

TEST(LoaderIntegrationTest, LoadedProgramPreservesStackFramesAcrossNestedCalls) {
    const auto words = assemble({
        {1, "addiu $t0, $zero, 42"},
        {2, "jal outer"},
        {3, "j done"},
        {4, "outer:"},
        {5, "addiu $sp, $sp, -8"},
        {6, "sw $ra, 4($sp)"},
        {7, "sw $t0, 0($sp)"},
        {8, "jal inner"},
        {9, "lw $t0, 0($sp)"},
        {10, "lw $ra, 4($sp)"},
        {11, "addiu $sp, $sp, 8"},
        {12, "jr $ra"},
        {13, "inner:"},
        {14, "addiu $t0, $zero, 99"},
        {15, "jr $ra"},
        {16, "done:"},
        {17, "addiu $t1, $t0, 0"}
    });
    const auto path = std::filesystem::path(::testing::TempDir()) /
                      "loader_nested_stack.bin";
    writeBinaryFile(path.string(), words);
    Memory memory(TEXT_BASE, words.size() * 4);
    loadBinary(path.string(), memory, TEXT_BASE);
    std::filesystem::remove(path);
    CPU cpu(memory);
    for (int i = 0; i < 6; ++i) { cpu.step(); }
    EXPECT_EQ(cpu.getRegister(29), STACK_TOP - 8);
    EXPECT_EQ(memory.readWord(STACK_TOP - 8), 42u);
    EXPECT_EQ(memory.readWord(STACK_TOP - 4), TEXT_BASE + 8);
    cpu.step();
    EXPECT_EQ(cpu.getRegister(8), 99u);
    for (int i = 0; i < 7; ++i) { cpu.step(); }
    EXPECT_EQ(cpu.getRegister(8), 42u);
    EXPECT_EQ(cpu.getRegister(9), 42u);
    EXPECT_EQ(cpu.getRegister(29), STACK_TOP);
    EXPECT_EQ(cpu.getRegister(31), TEXT_BASE + 8);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + words.size() * 4);
    cpu.reset();
    EXPECT_EQ(cpu.getRegister(29), STACK_TOP);
    EXPECT_EQ(memory.readWord(STACK_TOP - 8), 42u);
}
