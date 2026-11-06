#include <gtest/gtest.h>

#include "emulator/cpu.h"
#include "emulator/memory.h"
#include "assembler/assembler.h"
#include "memory_layout.h"

#include <cstdint>
#include <ostream>
#include <stdexcept>

TEST(CPUTest, InitializesRegisters) {
    Memory memory(TEXT_BASE, 1024);
    CPU cpu(memory);

    EXPECT_EQ(cpu.getRegister(8), 0u);
    EXPECT_EQ(cpu.getRegister(29), STACK_TOP);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE);
}

TEST(CPUTest, ZeroRegisterIsImmutable) {
    Memory memory(TEXT_BASE, 1024);
    CPU cpu(memory);

    cpu.setRegister(0, 123);

    EXPECT_EQ(cpu.getRegister(0), 0u);
}

TEST(CPUTest, ExecutesAddiu) {
    Memory memory(TEXT_BASE, 1024);

    // addiu $t0, $zero, 5
    memory.writeWord(TEXT_BASE, 0x24080005u);

    CPU cpu(memory);

    cpu.step();

    EXPECT_EQ(cpu.getRegister(8), 5u);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
}

TEST(CPUTest, AddiuSignExtendsImmediateAndUsesSourceRegister) {
    Memory memory(TEXT_BASE, 1024);

    // addiu $t1, $t0, -1
    memory.writeWord(TEXT_BASE, 0x2509FFFFu);

    CPU cpu(memory);
    cpu.setRegister(8, 5u);
    cpu.step();

    EXPECT_EQ(cpu.getRegister(9), 4u);
    EXPECT_EQ(cpu.getRegister(8), 5u);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
}

TEST(CPUTest, AddiuWrapsWithoutOverflowException) {
    Memory memory(TEXT_BASE, 1024);

    // addiu $t1, $t0, 1
    memory.writeWord(TEXT_BASE, 0x25090001u);

    CPU cpu(memory);
    cpu.setRegister(8, 0xFFFFFFFFu);
    cpu.step();

    EXPECT_EQ(cpu.getRegister(9), 0u);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
}

namespace {
struct ALUCase {
    const char* name;
    uint32_t word;
    uint32_t rs;
    uint32_t rt;
    uint32_t expected;
};
void PrintTo(const ALUCase& value, std::ostream* out) {
    *out << value.name;
}
class CPUALUTest : public testing::TestWithParam<ALUCase> {};

TEST_P(CPUALUTest, ExecutesAndAdvancesPC) {
    const auto& c = GetParam();
    Memory memory(TEXT_BASE, 64);
    memory.writeWord(TEXT_BASE, c.word);
    CPU cpu(memory);
    cpu.setRegister(8, c.rs);
    cpu.setRegister(9, c.rt);
    cpu.step();
    EXPECT_EQ(cpu.getRegister(10), c.expected);
    EXPECT_EQ(cpu.getRegister(8), c.rs);
    EXPECT_EQ(cpu.getRegister(9), c.rt);
    EXPECT_EQ(cpu.getRegister(0), 0u);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
}

// Fixed instruction words, independent of the assembler and decoder.
INSTANTIATE_TEST_SUITE_P(Instructions, CPUALUTest, testing::Values(
    ALUCase{"Add", 0x01095020u, 7, 5, 12},
    ALUCase{"AddNegative", 0x01095020u, 0xFFFFFFFEu, 1, 0xFFFFFFFFu},
    ALUCase{"AddSignedBoundary", 0x01095020u, 0x80000000u, 0, 0x80000000u},
    ALUCase{"AdduWrap", 0x01095021u, 0xFFFFFFFFu, 1, 0},
    ALUCase{"AdduSignedOverflow", 0x01095021u, 0x7FFFFFFFu, 1, 0x80000000u},
    ALUCase{"Sub", 0x01095022u, 5, 7, 0xFFFFFFFEu},
    ALUCase{"SubNegative", 0x01095022u, 0xFFFFFFFEu, 0xFFFFFFFFu, 0xFFFFFFFFu},
    ALUCase{"SubuWrap", 0x01095023u, 0, 1, 0xFFFFFFFFu},
    ALUCase{"And", 0x01095024u, 0xA5A5FFFFu, 0xFFFF1234u, 0xA5A51234u},
    ALUCase{"Or", 0x01095025u, 0xA5000000u, 0x005A1234u, 0xA55A1234u},
    ALUCase{"Xor", 0x01095026u, 0xFFFF0000u, 0xFF00FF00u, 0x00FFFF00u},
    ALUCase{"Nor", 0x01095027u, 0xFFFF0000u, 0x0000FF00u, 0x000000FFu},
    ALUCase{"SltSigned", 0x0109502Au, 0xFFFFFFFFu, 1, 1},
    ALUCase{"SltFalse", 0x0109502Au, 1, 0xFFFFFFFFu, 0},
    ALUCase{"SltEqual", 0x0109502Au, 5, 5, 0},
    ALUCase{"SltuUnsigned", 0x0109502Bu, 0xFFFFFFFFu, 1, 0},
    ALUCase{"SltuTrue", 0x0109502Bu, 1, 0xFFFFFFFFu, 1},
    ALUCase{"Sll", 0x00095100u, 0, 0x12345678u, 0x23456780u},
    ALUCase{"Sll31", 0x000957C0u, 0, 1, 0x80000000u},
    ALUCase{"SllZero", 0x00095000u, 0, 0xFFFFFFFFu, 0xFFFFFFFFu},
    ALUCase{"Srl", 0x00095102u, 0, 0x87654321u, 0x08765432u},
    ALUCase{"Srl31", 0x000957C2u, 0, 0x80000000u, 1},
    ALUCase{"SraNegative", 0x00095103u, 0, 0x87654321u, 0xF8765432u},
    ALUCase{"SraPositive", 0x00095103u, 0, 0x76543210u, 0x07654321u},
    ALUCase{"Sra31", 0x000957C3u, 0, 0x80000000u, 0xFFFFFFFFu},
    ALUCase{"SraZero", 0x00095003u, 0, 0x80000000u, 0x80000000u},
    ALUCase{"Addi", 0x210A0005u, 7, 0, 12},
    ALUCase{"AddiMinImmediate", 0x210A8000u, 0, 0, 0xFFFF8000u},
    ALUCase{"AddiuMaxImmediate", 0x250A7FFFu, 1, 0, 0x8000u},
    ALUCase{"AddiuMinImmediate", 0x250A8000u, 0, 0, 0xFFFF8000u},
    ALUCase{"SltiSigned", 0x290A0001u, 0xFFFFFFFFu, 0, 1},
    ALUCase{"SltiNegativeImmediate", 0x290AFFFFu, 0, 0, 0},
    ALUCase{"SltiEqual", 0x290A8000u, 0xFFFF8000u, 0, 0},
    ALUCase{"SltiuSignExtension", 0x2D0AFFFFu, 0x10000u, 0, 1},
    ALUCase{"SltiuEqual", 0x2D0AFFFFu, 0xFFFFFFFFu, 0, 0},
    ALUCase{"AndiZeroExtension", 0x310A8001u, 0xFFFFFFFFu, 0, 0x8001u},
    ALUCase{"OriZeroExtension", 0x350A8001u, 0x12340000u, 0, 0x12348001u},
    ALUCase{"XoriZeroExtension", 0x390AFFFFu, 0xFFFF1234u, 0, 0xFFFFEDCBu},
    ALUCase{"Lui", 0x3C0A8001u, 0, 0, 0x80010000u}
), [](const testing::TestParamInfo<ALUCase>& info) { return info.param.name; });

struct OverflowCase { uint32_t word, rs, rt; };
void PrintTo(const OverflowCase& value, std::ostream* out) {
    *out << std::hex << value.word << '_' << value.rs << '_' << value.rt << std::dec;
}
class CPUOverflowTest : public testing::TestWithParam<OverflowCase> {};
TEST_P(CPUOverflowTest, PreservesDestinationAndPC) {
    const auto c = GetParam();
    Memory memory(TEXT_BASE, 64);
    memory.writeWord(TEXT_BASE, c.word);
    CPU cpu(memory);
    cpu.setRegister(8, c.rs);
    cpu.setRegister(9, c.rt);
    cpu.setRegister(10, 123);
    EXPECT_THROW(cpu.step(), std::overflow_error);
    EXPECT_EQ(cpu.getRegister(10), 123u);
    EXPECT_EQ(cpu.getRegister(0), 0u);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE);
}
INSTANTIATE_TEST_SUITE_P(SignedArithmetic, CPUOverflowTest, testing::Values(
    OverflowCase{0x01095020u, 0x7FFFFFFFu, 1},
    OverflowCase{0x01095020u, 0x80000000u, 0xFFFFFFFFu},
    OverflowCase{0x01095022u, 0x80000000u, 1},
    OverflowCase{0x01095022u, 0x7FFFFFFFu, 0xFFFFFFFFu},
    OverflowCase{0x210A0001u, 0x7FFFFFFFu, 0},
    OverflowCase{0x210AFFFFu, 0x80000000u, 0},
    OverflowCase{0x01090020u, 0x7FFFFFFFu, 1}, // add $zero still traps
    OverflowCase{0x21000001u, 0x7FFFFFFFu, 0}
));

struct BranchCase { const char* name; uint32_t word, rs, rt, target; };
void PrintTo(const BranchCase& value, std::ostream* out) {
    *out << value.name;
}
class CPUBranchTest : public testing::TestWithParam<BranchCase> {};
TEST_P(CPUBranchTest, SelectsNextPC) {
    const auto c = GetParam();
    Memory memory(TEXT_BASE, 64);
    memory.writeWord(TEXT_BASE, c.word);
    CPU cpu(memory);
    cpu.setRegister(8, c.rs);
    cpu.setRegister(9, c.rt);
    cpu.step();
    EXPECT_EQ(cpu.getPC(), c.target);
    EXPECT_EQ(cpu.getRegister(8), c.rs);
    EXPECT_EQ(cpu.getRegister(9), c.rt);
}
INSTANTIATE_TEST_SUITE_P(Branches, CPUBranchTest, testing::Values(
    BranchCase{"BeqTaken", 0x11090002u, 3, 3, TEXT_BASE + 12},
    BranchCase{"BeqNotTaken", 0x11090002u, 3, 4, TEXT_BASE + 4},
    BranchCase{"BneTaken", 0x15090002u, 3, 4, TEXT_BASE + 12},
    BranchCase{"BneNotTaken", 0x15090002u, 3, 3, TEXT_BASE + 4},
    BranchCase{"Backward", 0x1109FFFFu, 3, 3, TEXT_BASE},
    BranchCase{"ZeroOffset", 0x11090000u, 3, 3, TEXT_BASE + 4},
    BranchCase{"MinOffset", 0x11098000u, 3, 3, TEXT_BASE + 4 - 131072},
    BranchCase{"MaxOffset", 0x15097FFFu, 3, 4, TEXT_BASE + 4 + 131068}
), [](const testing::TestParamInfo<BranchCase>& info) { return info.param.name; });
}  // namespace

TEST(CPUTest, ExecutesJalAndJrWithoutDelaySlots) {
    Memory memory(TEXT_BASE, 64);
    memory.writeWord(TEXT_BASE, 0x0C100004u); // jal TEXT_BASE + 16
    memory.writeWord(TEXT_BASE + 4, 0x24080007u); // return: addiu $t0, $zero, 7
    memory.writeWord(TEXT_BASE + 16, 0x03E00008u); // jr $ra
    CPU cpu(memory);
    cpu.step();
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 16);
    EXPECT_EQ(cpu.getRegister(31), TEXT_BASE + 4);
    EXPECT_EQ(cpu.getRegister(8), 0u);
    cpu.step();
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
    cpu.step();
    EXPECT_EQ(cpu.getRegister(8), 7u);
}

TEST(CPUTest, JumpSkipsFollowingInstructionAndPreservesLinkRegister) {
    Memory memory(TEXT_BASE, 64);
    memory.writeWord(TEXT_BASE, 0x08100004u); // j TEXT_BASE + 16
    memory.writeWord(TEXT_BASE + 4, 0x24080007u);
    CPU cpu(memory);
    cpu.setRegister(31, 123);
    cpu.step();
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 16);
    EXPECT_EQ(cpu.getRegister(8), 0u);
    EXPECT_EQ(cpu.getRegister(31), 123u);
}

TEST(CPUTest, LoadsWordsAndSignedOrUnsignedBytes) {
    for (const auto word : {0x8D09FFFCu, 0x8109FFFCu, 0x9109FFFCu}) {
        SCOPED_TRACE(word);
        Memory memory(TEXT_BASE, 128);
        memory.writeWord(TEXT_BASE, word); // load $t1, -4($t0)
        memory.writeWord(TEXT_BASE + 64, 0x12345680u);
        CPU cpu(memory);
        cpu.setRegister(8, TEXT_BASE + 68);
        cpu.step();
        const uint32_t expected = word == 0x8D09FFFCu ? 0x12345680u
            : word == 0x8109FFFCu ? 0xFFFFFF80u : 0x80u;
        EXPECT_EQ(cpu.getRegister(9), expected);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
    }
}

TEST(CPUTest, StoresLittleEndianWordAndLowByte) {
    for (const auto word : {0xAD09FFFCu, 0xA109FFFDu}) {
        SCOPED_TRACE(word);
        Memory memory(TEXT_BASE, 128);
        memory.writeWord(TEXT_BASE, word);
        memory.writeWord(TEXT_BASE + 64, 0xAAAAAAAAu);
        CPU cpu(memory);
        cpu.setRegister(8, TEXT_BASE + 68);
        cpu.setRegister(9, 0x12345678u);
        cpu.step();
        EXPECT_EQ(memory.readWord(TEXT_BASE + 64),
            word == 0xAD09FFFCu ? 0x12345678u : 0xAAAA78AAu);
        EXPECT_EQ(memory.readByte(TEXT_BASE + 64), word == 0xAD09FFFCu ? 0x78 : 0xAA);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
    }
}

TEST(CPUTest, RejectsUnalignedWordsAndRegisterJump) {
    for (const auto word : {0x8D090001u, 0xAD090001u, 0x01000008u}) {
        SCOPED_TRACE(word);
        Memory memory(TEXT_BASE, 128);
        memory.writeWord(TEXT_BASE, word);
        CPU cpu(memory);
        cpu.setRegister(8, TEXT_BASE + 65);
        cpu.setRegister(9, 123);
        EXPECT_THROW(cpu.step(), std::runtime_error);
        EXPECT_EQ(cpu.getRegister(9), 123u);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE);
        EXPECT_EQ(memory.readWord(TEXT_BASE + 64), 0u);
    }
}

TEST(CPUTest, MemoryFaultsPreserveStateIncludingPartialWordStores) {
    for (const auto word : {0x8D090000u, 0xAD090000u, 0x81090004u,
                            0x91090004u, 0xA1090004u, 0x8D000000u}) {
        SCOPED_TRACE(word);
        Memory memory(TEXT_BASE, 66);
        memory.writeWord(TEXT_BASE, word);
        memory.writeByte(TEXT_BASE + 64, 0xAA);
        memory.writeByte(TEXT_BASE + 65, 0xBB);
        CPU cpu(memory);
        cpu.setRegister(8, TEXT_BASE + 64);
        cpu.setRegister(9, 0x12345678u);
        EXPECT_THROW(cpu.step(), std::runtime_error);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE);
        EXPECT_EQ(cpu.getRegister(9), 0x12345678u);
        EXPECT_EQ(memory.readByte(TEXT_BASE + 64), 0xAA);
        EXPECT_EQ(memory.readByte(TEXT_BASE + 65), 0xBB);
    }
}

TEST(CPUTest, RejectsUnknownOpcodeAndFunction) {
    for (const auto word : {0xFC000000u, 0x0000003Fu}) {
        Memory memory(TEXT_BASE, 64);
        memory.writeWord(TEXT_BASE, word);
        CPU cpu(memory);
        EXPECT_THROW(cpu.step(), std::runtime_error);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE);
    }
}

TEST(CPUTest, InstructionWritesCannotChangeZeroRegister) {
    for (const auto word : {0x01090021u, 0x25000001u, 0x3C00FFFFu,
                            0x8D000000u, 0x81000000u, 0x91000000u, 0u}) {
        Memory memory(TEXT_BASE, 128);
        memory.writeWord(TEXT_BASE, word);
        memory.writeWord(TEXT_BASE + 64, 0xFFFFFFFFu);
        CPU cpu(memory);
        cpu.setRegister(8, TEXT_BASE + 64);
        cpu.setRegister(9, 7);
        cpu.step();
        EXPECT_EQ(cpu.getRegister(0), 0u);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
    }
}

TEST(CPUTest, AssembledLoopExecutesUntilBranchFallsThrough) {
    const auto words = assemble({
        {1, "addiu $t0, $zero, 3"},
        {2, "loop:"},
        {3, "addiu $t0, $t0, -1"},
        {4, "bne $t0, $zero, loop"},
        {5, "sw $t0, 0($t1)"}
    });
    Memory memory(TEXT_BASE, 128);
    for (std::size_t i = 0; i < words.size(); ++i) {
        memory.writeWord(TEXT_BASE + static_cast<uint32_t>(i * 4), words[i]);
    }
    CPU cpu(memory);
    cpu.setRegister(9, TEXT_BASE + 64);
    memory.writeWord(TEXT_BASE + 64, 123);
    for (int i = 0; i < 8; ++i) { cpu.step(); }
    EXPECT_EQ(cpu.getRegister(8), 0u);
    EXPECT_EQ(memory.readWord(TEXT_BASE + 64), 0u);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 16);
}

TEST(CPUTest, LoadsPositiveAndBoundaryBytesAtUnalignedAddresses) {
    for (const auto byte : {0u, 0x7Fu, 0x80u, 0xFFu}) {
        for (const auto word : {0x81090001u, 0x91090001u}) {
            Memory memory(TEXT_BASE, 66);
            memory.writeWord(TEXT_BASE, word);
            memory.writeByte(TEXT_BASE + 65, static_cast<uint8_t>(byte));
            CPU cpu(memory);
            cpu.setRegister(8, TEXT_BASE + 64);
            cpu.step();
            const uint32_t expected = word == 0x81090001u && byte >= 0x80u
                ? byte | 0xFFFFFF00u : byte;
            EXPECT_EQ(cpu.getRegister(9), expected);
        }
    }
}

TEST(CPUTest, DestinationCanAliasSource) {
    for (const auto word : {0x01094021u, 0x25080001u, 0x8D080000u}) {
        Memory memory(TEXT_BASE, 128);
        memory.writeWord(TEXT_BASE, word);
        memory.writeWord(TEXT_BASE + 64, 42);
        CPU cpu(memory);
        cpu.setRegister(8, TEXT_BASE + 64);
        cpu.setRegister(9, 1);
        cpu.step();
        EXPECT_EQ(cpu.getRegister(8), word == 0x8D080000u ? 42u : TEXT_BASE + 65);
    }
}

TEST(CPUTest, FetchOutsideMemoryDoesNotAdvancePC) {
    Memory memory(TEXT_BASE, 4);
    memory.writeWord(TEXT_BASE, 0u); // nop
    CPU cpu(memory);
    cpu.step();
    EXPECT_THROW(cpu.step(), std::runtime_error);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
}

TEST(CPUTest, ResetRestoresAllRegistersAndPC) {
    Memory memory(TEXT_BASE, 64);
    memory.writeWord(TEXT_BASE, 0u);
    CPU cpu(memory);
    for (uint8_t i = 1; i < 32; ++i) { cpu.setRegister(i, 123); }
    cpu.step();
    cpu.reset();
    for (uint8_t i = 0; i < 32; ++i) {
        EXPECT_EQ(cpu.getRegister(i), i == 29 ? STACK_TOP : 0u);
    }
    EXPECT_EQ(cpu.getPC(), TEXT_BASE);
    EXPECT_THROW(cpu.getRegister(32), std::out_of_range);
    EXPECT_THROW(cpu.setRegister(32, 1), std::out_of_range);
}
