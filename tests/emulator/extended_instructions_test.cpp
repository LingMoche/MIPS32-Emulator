#include <gtest/gtest.h>

#include "assembler/assembler.h"
#include "emulator/cpu.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
constexpr uint32_t initialHI = 0xAABBCCDDu;
constexpr uint32_t initialLO = 0x11223344u;

void seedSpecialRegisters(Memory& memory, CPU& cpu) {
    memory.writeWord(TEXT_BASE, 0x02000011u);     // mthi $s0
    memory.writeWord(TEXT_BASE + 4, 0x02200013u); // mtlo $s1
    cpu.setRegister(16, initialHI);
    cpu.setRegister(17, initialLO);
    cpu.step();
    cpu.step();
    ASSERT_EQ(cpu.getHI(), initialHI);
    ASSERT_EQ(cpu.getLO(), initialLO);
}

std::array<uint32_t, 32> registers(const CPU& cpu) {
    std::array<uint32_t, 32> result{};
    for (uint8_t i = 0; i < 32; ++i) {
        result[i] = cpu.getRegister(i);
    }
    return result;
}

struct ProductQuotientCase {
    const char* name;
    uint32_t word, rs, rt, hi, lo;
};
void PrintTo(const ProductQuotientCase& value, std::ostream* output) {
    *output << value.name;
}
class MultiplyDivideTest : public testing::TestWithParam<ProductQuotientCase> {};

TEST_P(MultiplyDivideTest, ReplacesBothSpecialRegistersAndPreservesGeneralRegisters) {
    const auto& c = GetParam();
    Memory memory(TEXT_BASE, 64, 0);
    CPU cpu(memory);
    seedSpecialRegisters(memory, cpu);
    memory.writeWord(TEXT_BASE + 8, c.word);
    cpu.setRegister(8, c.rs);
    cpu.setRegister(9, c.rt);
    const auto before = registers(cpu);
    cpu.step();
    EXPECT_EQ(cpu.getHI(), c.hi);
    EXPECT_EQ(cpu.getLO(), c.lo);
    EXPECT_EQ(registers(cpu), before);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 12);
}

INSTANTIATE_TEST_SUITE_P(Boundaries, MultiplyDivideTest, testing::Values(
    ProductQuotientCase{"MultPositive", 0x01090018u, 7, 3, 0, 21},
    ProductQuotientCase{"MultNegative", 0x01090018u, 0xFFFFFFF9u, 3, 0xFFFFFFFFu, 0xFFFFFFEBu},
    ProductQuotientCase{"MultBothNegative", 0x01090018u, 0xFFFFFFF9u, 0xFFFFFFFDu, 0, 21},
    ProductQuotientCase{"MultMinSquared", 0x01090018u, 0x80000000u, 0x80000000u, 0x40000000u, 0},
    ProductQuotientCase{"MultMaxSquared", 0x01090018u, 0x7FFFFFFFu, 0x7FFFFFFFu, 0x3FFFFFFFu, 1},
    ProductQuotientCase{"MultMinByOne", 0x01090018u, 0x80000000u, 1, 0xFFFFFFFFu, 0x80000000u},
    ProductQuotientCase{"MultMinByMinusOne", 0x01090018u, 0x80000000u, 0xFFFFFFFFu, 0, 0x80000000u},
    ProductQuotientCase{"MultZero", 0x01090018u, 0xFFFFFFFFu, 0, 0, 0},
    ProductQuotientCase{"MultuMaxSquared", 0x01090019u, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFEu, 1},
    ProductQuotientCase{"MultuHighBit", 0x01090019u, 0x80000000u, 2, 1, 0},
    ProductQuotientCase{"MultuZero", 0x01090019u, 0, 0xFFFFFFFFu, 0, 0},
    ProductQuotientCase{"DivPositive", 0x0109001Au, 7, 3, 1, 2},
    ProductQuotientCase{"DivNegativeDividend", 0x0109001Au, 0xFFFFFFF9u, 3, 0xFFFFFFFFu, 0xFFFFFFFEu},
    ProductQuotientCase{"DivNegativeDivisor", 0x0109001Au, 7, 0xFFFFFFFDu, 1, 0xFFFFFFFEu},
    ProductQuotientCase{"DivBothNegative", 0x0109001Au, 0xFFFFFFF9u, 0xFFFFFFFDu, 0xFFFFFFFFu, 2},
    ProductQuotientCase{"DivMinByOne", 0x0109001Au, 0x80000000u, 1, 0, 0x80000000u},
    ProductQuotientCase{"DivMinByThree", 0x0109001Au, 0x80000000u, 3, 0xFFFFFFFEu, 0xD5555556u},
    ProductQuotientCase{"DivSmallNegative", 0x0109001Au, 0xFFFFFFFFu, 3, 0xFFFFFFFFu, 0},
    ProductQuotientCase{"DivEqualMin", 0x0109001Au, 0x80000000u, 0x80000000u, 0, 1},
    ProductQuotientCase{"DivZeroDividend", 0x0109001Au, 0, 7, 0, 0},
    ProductQuotientCase{"DivuMaxByTwo", 0x0109001Bu, 0xFFFFFFFFu, 2, 1, 0x7FFFFFFFu},
    ProductQuotientCase{"DivuMaxByOne", 0x0109001Bu, 0xFFFFFFFFu, 1, 0, 0xFFFFFFFFu},
    ProductQuotientCase{"DivuHighBitDivisor", 0x0109001Bu, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu, 1},
    ProductQuotientCase{"DivuEqualMax", 0x0109001Bu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0, 1},
    ProductQuotientCase{"DivuZeroDividend", 0x0109001Bu, 0, 7, 0, 0}
), [](const testing::TestParamInfo<ProductQuotientCase>& info) { return info.param.name; });

TEST(ExtendedCPUTest, DivisionErrorsPreserveAllState) {
    for (const auto word : {0x0109001Au, 0x0109001Bu}) {
        Memory memory(TEXT_BASE, 64, 0);
        CPU cpu(memory);
        seedSpecialRegisters(memory, cpu);
        memory.writeWord(TEXT_BASE + 8, word);
        cpu.setRegister(8, 123);
        cpu.setRegister(9, 0);
        const auto before = registers(cpu);
        EXPECT_THROW(cpu.step(), std::runtime_error);
        EXPECT_EQ(cpu.getHI(), initialHI);
        EXPECT_EQ(cpu.getLO(), initialLO);
        EXPECT_EQ(registers(cpu), before);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE + 8);
    }
    Memory memory(TEXT_BASE, 64, 0);
    CPU cpu(memory);
    seedSpecialRegisters(memory, cpu);
    memory.writeWord(TEXT_BASE + 8, 0x0109001Au);
    cpu.setRegister(8, 0x80000000u);
    cpu.setRegister(9, 0xFFFFFFFFu);
    const auto before = registers(cpu);
    EXPECT_THROW(cpu.step(), std::overflow_error);
    EXPECT_EQ(cpu.getHI(), initialHI);
    EXPECT_EQ(cpu.getLO(), initialLO);
    EXPECT_EQ(registers(cpu), before);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 8);
}

TEST(ExtendedCPUTest, TransfersPreserveOtherSpecialRegisterAndRespectZeroRegister) {
    const auto words = assemble({
        {1, "mthi $t0"}, {2, "mtlo $t1"}, {3, "mfhi $t2"}, {4, "mflo $t3"},
        {5, "mfhi $zero"}, {6, "mflo $zero"}, {7, "mthi $zero"}, {8, "mtlo $zero"}
    });
    Memory memory(TEXT_BASE, words.size() * 4, 0);
    for (std::size_t i = 0; i < words.size(); ++i) {
        memory.writeWord(TEXT_BASE + static_cast<uint32_t>(i * 4), words[i]);
    }
    CPU cpu(memory);
    EXPECT_EQ(cpu.getHI(), 0u);
    EXPECT_EQ(cpu.getLO(), 0u);
    cpu.setRegister(8, initialHI);
    cpu.setRegister(9, initialLO);
    cpu.step();
    EXPECT_EQ(cpu.getHI(), initialHI);
    EXPECT_EQ(cpu.getLO(), 0u);
    cpu.step();
    EXPECT_EQ(cpu.getHI(), initialHI);
    EXPECT_EQ(cpu.getLO(), initialLO);
    for (int i = 0; i < 4; ++i) { cpu.step(); }
    EXPECT_EQ(cpu.getRegister(10), initialHI);
    EXPECT_EQ(cpu.getRegister(11), initialLO);
    EXPECT_EQ(cpu.getRegister(0), 0u);
    EXPECT_EQ(cpu.getHI(), initialHI);
    EXPECT_EQ(cpu.getLO(), initialLO);
    cpu.step();
    EXPECT_EQ(cpu.getHI(), 0u);
    EXPECT_EQ(cpu.getLO(), initialLO);
    cpu.step();
    EXPECT_EQ(cpu.getLO(), 0u);
}

TEST(ExtendedCPUTest, ResetAndStatePrintingIncludeSpecialRegisters) {
    Memory memory(TEXT_BASE, 64, 0);
    CPU cpu(memory);
    seedSpecialRegisters(memory, cpu);
    std::ostringstream output;
    output << std::dec;
    output.fill('*');
    output.width(12);
    const auto flags = output.flags();
    cpu.printState(output);
    EXPECT_NE(output.str().find("HI = 0xAABBCCDD\n"), std::string::npos);
    EXPECT_NE(output.str().find("LO = 0x11223344\n"), std::string::npos);
    EXPECT_EQ(output.flags(), flags);
    EXPECT_EQ(output.fill(), '*');
    EXPECT_EQ(output.width(), 12);
    EXPECT_EQ(cpu.getHI(), initialHI);
    EXPECT_EQ(cpu.getLO(), initialLO);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 8);
    cpu.reset();
    EXPECT_EQ(cpu.getHI(), 0u);
    EXPECT_EQ(cpu.getLO(), 0u);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE);
    EXPECT_EQ(memory.readWord(TEXT_BASE), 0x02000011u);
}

struct VariableShiftCase { const char* name; uint32_t word, amount, value, expected; };
void PrintTo(const VariableShiftCase& value, std::ostream* output) {
    *output << value.name;
}
class VariableShiftTest : public testing::TestWithParam<VariableShiftCase> {};
TEST_P(VariableShiftTest, UsesLowFiveBitsAndPreservesSourcesAndSpecialRegisters) {
    const auto& c = GetParam();
    Memory memory(TEXT_BASE, 64, 0);
    CPU cpu(memory);
    seedSpecialRegisters(memory, cpu);
    memory.writeWord(TEXT_BASE + 8, c.word);
    cpu.setRegister(8, c.amount);
    cpu.setRegister(9, c.value);
    cpu.step();
    EXPECT_EQ(cpu.getRegister(10), c.expected);
    EXPECT_EQ(cpu.getRegister(8), c.amount);
    EXPECT_EQ(cpu.getRegister(9), c.value);
    EXPECT_EQ(cpu.getHI(), initialHI);
    EXPECT_EQ(cpu.getLO(), initialLO);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 12);
}
INSTANTIATE_TEST_SUITE_P(Boundaries, VariableShiftTest, testing::Values(
    VariableShiftCase{"Sllv", 0x01095004u, 4, 0x12345678u, 0x23456780u},
    VariableShiftCase{"SllvZero", 0x01095004u, 0, 0x80000001u, 0x80000001u},
    VariableShiftCase{"Sllv31", 0x01095004u, 31, 1, 0x80000000u},
    VariableShiftCase{"Sllv32", 0x01095004u, 32, 0x80000001u, 0x80000001u},
    VariableShiftCase{"Sllv33", 0x01095004u, 33, 1, 2},
    VariableShiftCase{"Srlv", 0x01095006u, 4, 0x87654321u, 0x08765432u},
    VariableShiftCase{"SrlvZero", 0x01095006u, 0, 0x80000001u, 0x80000001u},
    VariableShiftCase{"Srlv31", 0x01095006u, 31, 0x80000000u, 1},
    VariableShiftCase{"Srlv32", 0x01095006u, 32, 0x80000001u, 0x80000001u},
    VariableShiftCase{"SrlvAllBits", 0x01095006u, 0xFFFFFFFFu, 0x80000000u, 1},
    VariableShiftCase{"SravNegative", 0x01095007u, 4, 0x87654321u, 0xF8765432u},
    VariableShiftCase{"SravPositive", 0x01095007u, 4, 0x76543210u, 0x07654321u},
    VariableShiftCase{"SravZero", 0x01095007u, 0, 0x80000001u, 0x80000001u},
    VariableShiftCase{"Srav31", 0x01095007u, 31, 0x80000000u, 0xFFFFFFFFu},
    VariableShiftCase{"Srav32", 0x01095007u, 32, 0x80000001u, 0x80000001u},
    VariableShiftCase{"SravAllBits", 0x01095007u, 0xFFFFFFFFu, 0x80000000u, 0xFFFFFFFFu}
), [](const testing::TestParamInfo<VariableShiftCase>& info) { return info.param.name; });

TEST(ExtendedCPUTest, VariableShiftDestinationCanAliasEitherSourceOrZero) {
    for (const auto function : {4u, 6u, 7u}) {
        for (const auto destination : {0u, 8u, 9u}) {
            Memory memory(TEXT_BASE, 4, 0);
            // s??v $destination, $t1, $t0
            memory.writeWord(TEXT_BASE, 0x01090000u | (destination << 11) | function);
            CPU cpu(memory);
            cpu.setRegister(8, 1);
            cpu.setRegister(9, 0x80000001u);
            cpu.step();
            const auto expected = function == 4 ? 2u : function == 6 ? 0x40000000u : 0xC0000000u;
            EXPECT_EQ(cpu.getRegister(destination), destination == 0 ? 0u : expected);
            if (destination != 8) { EXPECT_EQ(cpu.getRegister(8), 1u); }
            if (destination != 9) { EXPECT_EQ(cpu.getRegister(9), 0x80000001u); }
        }
    }
}

TEST(ExtendedCPUTest, HalfwordLoadsSignOrZeroExtendAndAllowTwoByteAlignment) {
    for (const uint16_t half : {0u, 0x7FFFu, 0x8000u, 0xFFFFu}) {
        for (const auto word : {0x8509FFFEu, 0x9509FFFEu}) { // lh/lhu $t1,-2($t0)
            Memory memory(TEXT_BASE, 64, 0);
            memory.writeWord(TEXT_BASE, word);
            memory.writeHalfWord(TEXT_BASE + 34, half);
            CPU cpu(memory);
            cpu.setRegister(8, TEXT_BASE + 36);
            cpu.step();
            const uint32_t expected = word == 0x8509FFFEu && (half & 0x8000u)
                ? 0xFFFF0000u | half : half;
            EXPECT_EQ(cpu.getRegister(9), expected);
            EXPECT_EQ(cpu.getRegister(8), TEXT_BASE + 36);
            EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
        }
    }
}

TEST(ExtendedCPUTest, HalfwordStoreWritesLowSixteenBitsInLittleEndianAndStackLoadsWork) {
    const auto words = assemble({
        {1, "sh $t0, 2($sp)"}, {2, "lh $t1, 2($sp)"}, {3, "lhu $t2, 2($sp)"}
    });
    Memory memory(TEXT_BASE, words.size() * 4);
    for (std::size_t i = 0; i < words.size(); ++i) {
        memory.writeWord(TEXT_BASE + static_cast<uint32_t>(i * 4), words[i]);
    }
    memory.writeWord(STACK_TOP, 0xAABBCCDDu);
    CPU cpu(memory);
    cpu.setRegister(8, 0x12348001u);
    cpu.step();
    EXPECT_EQ(memory.readWord(STACK_TOP), 0x8001CCDDu);
    EXPECT_EQ(memory.readByte(STACK_TOP + 2), 0x01);
    EXPECT_EQ(memory.readByte(STACK_TOP + 3), 0x80);
    cpu.step();
    EXPECT_EQ(cpu.getRegister(9), 0xFFFF8001u);
    cpu.step();
    EXPECT_EQ(cpu.getRegister(10), 0x8001u);
}

TEST(ExtendedCPUTest, HalfwordFaultsPreserveRegistersSpecialRegistersAndAllMemory) {
    for (const auto word : {0x85090000u, 0x95090000u, 0xA5090000u}) {
        for (const auto address : {TEXT_BASE + 33, TEXT_BASE + 64, STACK_TOP + 3,
                                   STACK_TOP + 4, 0xFFFFFFFFu}) {
            Memory memory(TEXT_BASE, 65, 16); // aligned final byte has no partner
            CPU cpu(memory);
            seedSpecialRegisters(memory, cpu);
            memory.writeWord(TEXT_BASE + 8, word);
            memory.writeByte(TEXT_BASE + 64, 0xAA);
            cpu.setRegister(8, address);
            cpu.setRegister(9, 0x12345678u);
            const auto before = registers(cpu);
            std::ostringstream bytesBefore;
            memory.printState(bytesBefore);
            EXPECT_THROW(cpu.step(), std::runtime_error);
            EXPECT_EQ(registers(cpu), before);
            EXPECT_EQ(cpu.getHI(), initialHI);
            EXPECT_EQ(cpu.getLO(), initialLO);
            EXPECT_EQ(cpu.getPC(), TEXT_BASE + 8);
            std::ostringstream bytesAfter;
            memory.printState(bytesAfter);
            EXPECT_EQ(bytesAfter.str(), bytesBefore.str());
        }
    }
}

TEST(ExtendedCPUTest, HalfwordLoadDestinationCanAliasBaseAndZeroStillChecksAddress) {
    Memory memory(TEXT_BASE, 64, 0);
    memory.writeWord(TEXT_BASE, 0x95080000u);     // lhu $t0,0($t0)
    memory.writeHalfWord(TEXT_BASE + 34, 0x8001u);
    CPU cpu(memory);
    cpu.setRegister(8, TEXT_BASE + 34);
    cpu.step();
    EXPECT_EQ(cpu.getRegister(8), 0x8001u);
    for (const auto word : {0x85000000u, 0x95000000u}) { // lh/lhu $zero,0($t0)
        cpu.reset();
        memory.writeWord(TEXT_BASE, word);
        cpu.setRegister(8, TEXT_BASE + 34);
        cpu.step();
        EXPECT_EQ(cpu.getRegister(0), 0u);
        cpu.reset();
        cpu.setRegister(8, TEXT_BASE + 33);
        EXPECT_THROW(cpu.step(), std::runtime_error);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE);
    }
}

TEST(ExtendedInstructionIntegrationTest, ComputesAndStoresProductQuotientAndRemainder) {
    const auto words = assemble({
        {1, "addiu $t0, $zero, -7"}, {2, "addiu $t1, $zero, 3"},
        {3, "mult $t0, $t1"}, {4, "mfhi $s0"}, {5, "mflo $s1"},
        {6, "div $t0, $t1"}, {7, "mflo $s2"}, {8, "mfhi $s3"},
        {9, "sh $s1, 0($sp)"}, {10, "lh $s4, 0($sp)"},
        {11, "lhu $s5, 0($sp)"}, {12, "sllv $s6, $t1, $t1"},
        {13, "srlv $s7, $s6, $t1"}, {14, "srav $t2, $t0, $t1"},
        {15, "mthi $s6"}, {16, "mtlo $s7"},
        {17, "addiu $v0, $zero, 10"}, {18, "syscall"}
    });
    Memory memory(TEXT_BASE, words.size() * 4);
    for (std::size_t i = 0; i < words.size(); ++i) {
        memory.writeWord(TEXT_BASE + static_cast<uint32_t>(i * 4), words[i]);
    }
    CPU cpu(memory);
    for (int steps = 0; !cpu.isHalted() && steps < 30; ++steps) { cpu.step(); }
    EXPECT_TRUE(cpu.isHalted());
    EXPECT_EQ(cpu.getRegister(16), 0xFFFFFFFFu);
    EXPECT_EQ(cpu.getRegister(17), 0xFFFFFFEBu);
    EXPECT_EQ(cpu.getRegister(18), 0xFFFFFFFEu);
    EXPECT_EQ(cpu.getRegister(19), 0xFFFFFFFFu);
    EXPECT_EQ(cpu.getRegister(20), 0xFFFFFFEBu);
    EXPECT_EQ(cpu.getRegister(21), 0xFFEBu);
    EXPECT_EQ(cpu.getRegister(22), 24u);
    EXPECT_EQ(cpu.getRegister(23), 3u);
    EXPECT_EQ(cpu.getRegister(10), 0xFFFFFFFFu);
    EXPECT_EQ(cpu.getHI(), 24u);
    EXPECT_EQ(cpu.getLO(), 3u);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + words.size() * 4);
}
} // namespace
