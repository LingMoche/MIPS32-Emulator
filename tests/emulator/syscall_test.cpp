#include <gtest/gtest.h>

#include "assembler/assembler.h"
#include "emulator/cpu.h"

#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
class SyscallTest : public testing::Test {
protected:
    Memory memory{TEXT_BASE, 128, 16};
    std::istringstream input;
    std::ostringstream output;
    CPU cpu{memory, input, output};
    static constexpr uint32_t buffer = TEXT_BASE + 32;

    void SetUp() override {
        memory.writeWord(TEXT_BASE, 0x0000000Cu);
    }

    void call(uint32_t service, uint32_t argument = 0) {
        cpu.setRegister(2, service);
        cpu.setRegister(4, argument);
        cpu.step();
    }

    void expectFailurePreservesState() {
        std::array<uint32_t, 32> registers{};
        for (int i = 0; i < 32; ++i) {
            registers[i] = cpu.getRegister(i);
        }
        std::ostringstream before;
        memory.printState(before);
        EXPECT_THROW(cpu.step(), std::runtime_error);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE);
        EXPECT_FALSE(cpu.isHalted());
        EXPECT_EQ(cpu.getExitCode(), 0);
        for (int i = 0; i < 32; ++i) {
            EXPECT_EQ(cpu.getRegister(i), registers[i]);
        }
        std::ostringstream after;
        memory.printState(after);
        EXPECT_EQ(after.str(), before.str());
    }
};

TEST_F(SyscallTest, PrintsSignedDecimalIndependentlyOfOutputFormatting) {
    for (const auto value : {0u, 2147483647u, 0x80000000u, 0xFFFFFFFFu}) {
        cpu.reset();
        call(1, value);
    }
    EXPECT_EQ(output.str(), "02147483647-2147483648-1");
    output.str("");
    output << std::hex << std::showbase;
    output.fill('*');
    output.width(12);
    const auto flags = output.flags();
    cpu.reset();
    call(1, 42);
    EXPECT_EQ(output.str(), "42");
    EXPECT_EQ(output.flags(), flags);
    EXPECT_EQ(output.fill(), '*');
    EXPECT_EQ(output.width(), 12);
    EXPECT_EQ(cpu.getRegister(2), 1u);
    EXPECT_EQ(cpu.getRegister(4), 42u);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
}

TEST_F(SyscallTest, PrintsNullTerminatedStringsIncludingEmptyAndStackStrings) {
    memory.writeWord(buffer, 0x000A6948u); // "Hi\n\0"
    call(4, buffer);
    cpu.reset();
    call(4, buffer + 3);
    memory.writeWord(STACK_TOP, 0x00004B4Fu); // "OK\0"
    cpu.reset();
    call(4, STACK_TOP);
    EXPECT_EQ(output.str(), "Hi\nOK");
}

TEST_F(SyscallTest, InvalidOrUnterminatedStringProducesNoPartialOutput) {
    for (const auto address : {TEXT_BASE + 128, TEXT_BASE + 126}) {
        memory.writeByte(TEXT_BASE + 126, 'A');
        memory.writeByte(TEXT_BASE + 127, 'B');
        cpu.setRegister(2, 4);
        cpu.setRegister(4, address);
        expectFailurePreservesState();
        EXPECT_TRUE(output.str().empty());
    }
}

TEST_F(SyscallTest, PrintsOnlyLowByteIncludingNull) {
    call(11, 0x12340041u);
    cpu.reset();
    call(11, 0);
    cpu.reset();
    call(11, 0xFF);
    EXPECT_EQ(output.str(), std::string("A\0\xFF", 3));
}

TEST_F(SyscallTest, ReadsSignedDecimalLimitsAndConsumesOneLine) {
    input.str(" -2147483648 \r\n+2147483647\n0\n-1");
    for (const auto expected : {0x80000000u, 2147483647u, 0u, 0xFFFFFFFFu}) {
        cpu.reset();
        call(5);
        EXPECT_EQ(cpu.getRegister(2), expected);
        EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
    }
}

TEST_F(SyscallTest, InvalidIntegerOrEofPreservesState) {
    for (const auto* line : {"", "\n", "abc\n", "12xyz\n", "1 2\n", "0x10\n",
                             "2147483648\n", "-2147483649\n",
                             "999999999999999999999999\n"}) {
        SCOPED_TRACE(line);
        input.clear();
        input.str(line);
        cpu.setRegister(2, 5);
        expectFailurePreservesState();
    }
}

TEST_F(SyscallTest, ReadsCharactersWithoutSkippingWhitespaceAndZeroExtends) {
    input.str(std::string(" \n\xFF", 3));
    for (const auto expected : {32u, 10u, 255u}) {
        cpu.reset();
        call(12);
        EXPECT_EQ(cpu.getRegister(2), expected);
    }
    cpu.reset();
    cpu.setRegister(2, 12);
    expectFailurePreservesState();
}

TEST_F(SyscallTest, ReadsStringWithNewlineAndTerminatorWithoutTouchingOtherBytes) {
    input.str("Hi\nnext");
    for (int i = 0; i < 8; ++i) {
        memory.writeByte(buffer + i, 0xAA);
    }
    cpu.setRegister(5, 8);
    call(8, buffer);
    EXPECT_EQ(memory.readWord(buffer), 0x000A6948u);
    EXPECT_EQ(memory.readWord(buffer + 4), 0xAAAAAAAAu);
    EXPECT_EQ(input.peek(), 'n');
    EXPECT_EQ(cpu.getRegister(2), 8u);
    EXPECT_EQ(cpu.getRegister(4), buffer);
    EXPECT_EQ(cpu.getRegister(5), 8u);
}

TEST_F(SyscallTest, StringCapacityLeavesExcessInputForNextCall) {
    input.str("ABCDE\n");
    cpu.setRegister(5, 4);
    call(8, buffer);
    EXPECT_EQ(memory.readWord(buffer), 0x00434241u);
    EXPECT_EQ(input.peek(), 'D');
    cpu.reset();
    cpu.setRegister(5, 4);
    call(8, buffer);
    EXPECT_EQ(memory.readWord(buffer), 0x000A4544u);
}

TEST_F(SyscallTest, StringCapacityZeroNegativeAndOneDoNotConsumeInput) {
    input.str("abc\n");
    for (const auto length : {0u, 0xFFFFFFFFu, 0x80000000u}) {
        cpu.reset();
        cpu.setRegister(5, length);
        call(8, 0xFFFFFFFFu); // address is ignored for nonpositive capacities
        EXPECT_EQ(input.peek(), 'a');
    }
    cpu.reset();
    cpu.setRegister(5, 1);
    memory.writeByte(buffer, 0xAA);
    call(8, buffer);
    EXPECT_EQ(memory.readByte(buffer), 0);
    EXPECT_EQ(input.peek(), 'a');
}

TEST_F(SyscallTest, InvalidStringBufferIsRejectedBeforeInputOrMemoryChanges) {
    input.str("Hi\n");
    for (const auto address : {TEXT_BASE + 126, 0xFFFFFFFFu, STACK_TOP + 3}) {
        cpu.setRegister(2, 8);
        cpu.setRegister(4, address);
        cpu.setRegister(5, 4);
        expectFailurePreservesState();
        EXPECT_EQ(input.peek(), 'H');
    }
}

TEST_F(SyscallTest, StringEofAfterSomeBytesSucceedsButEmptyEofPreservesMemory) {
    input.str("Hi");
    cpu.setRegister(5, 8);
    call(8, buffer);
    EXPECT_EQ(memory.readByte(buffer), 'H');
    EXPECT_EQ(memory.readByte(buffer + 1), 'i');
    EXPECT_EQ(memory.readByte(buffer + 2), 0);
    cpu.reset();
    cpu.setRegister(2, 8);
    cpu.setRegister(4, buffer);
    cpu.setRegister(5, 8);
    expectFailurePreservesState();
}

TEST_F(SyscallTest, ExitsWithSignedCodeAndDoesNotExecuteAgainUntilReset) {
    for (const auto service : {10u, 17u}) {
        for (const auto code : {42u, 0xFFFFFFFFu, 0x80000000u}) {
            cpu.reset();
            EXPECT_FALSE(cpu.isHalted());
            EXPECT_EQ(cpu.getExitCode(), 0);
            call(service, code);
            EXPECT_TRUE(cpu.isHalted());
            const int64_t expected = code <= 0x7FFFFFFFu ? int64_t{code}
                                                       : int64_t{code} - 0x100000000LL;
            EXPECT_EQ(cpu.getExitCode(), service == 10 ? 0 : expected);
            EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
            memory.writeWord(TEXT_BASE + 4, 0xFFFFFFFFu);
            EXPECT_NO_THROW(cpu.step());
            EXPECT_EQ(cpu.getPC(), TEXT_BASE + 4);
        }
    }
    cpu.reset();
    call(11, 'R');
    EXPECT_EQ(output.str(), "R");
}

TEST_F(SyscallTest, UnknownServiceAndStreamFailuresPreserveEmulatedState) {
    cpu.setRegister(2, 999);
    expectFailurePreservesState();
    cpu.setRegister(2, 1);
    output.setstate(std::ios::badbit);
    expectFailurePreservesState();
    input.setstate(std::ios::badbit);
    for (const auto service : {5u, 8u, 12u}) {
        cpu.setRegister(2, service);
        cpu.setRegister(4, buffer);
        cpu.setRegister(5, 8);
        expectFailurePreservesState();
    }
}

TEST(SyscallIntegrationTest, AssembledProgramReadsPrintsAndExits) {
    const auto words = assemble({
        {1, "addiu $v0, $zero, 5"}, {2, "syscall"},
        {3, "addu $a0, $v0, $zero"}, {4, "addiu $v0, $zero, 1"},
        {5, "syscall"}, {6, "addiu $a0, $zero, 10"},
        {7, "addiu $v0, $zero, 11"}, {8, "syscall"},
        {9, "addiu $v0, $zero, 10"}, {10, "syscall"}
    });
    Memory memory(TEXT_BASE, words.size() * 4);
    for (std::size_t i = 0; i < words.size(); ++i) {
        memory.writeWord(TEXT_BASE + static_cast<uint32_t>(i * 4), words[i]);
    }
    std::istringstream input("-42\n");
    std::ostringstream output;
    CPU cpu(memory, input, output);
    int steps = 0;
    while (!cpu.isHalted() && steps < 20) {
        cpu.step();
        ++steps;
    }
    EXPECT_TRUE(cpu.isHalted());
    EXPECT_EQ(steps, 10);
    EXPECT_EQ(cpu.getPC(), TEXT_BASE + words.size() * 4);
    EXPECT_EQ(cpu.getExitCode(), 0);
    EXPECT_EQ(cpu.getRegister(0), 0u);
    EXPECT_EQ(output.str(), "-42\n");
}
} // namespace
