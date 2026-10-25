#include <gtest/gtest.h>

#include "assembler/encoder.h"

#include <string>

TEST(EncoderTest, EncodesAdd) {
    Instruction inst;
    inst.mnemonic = "add";
    inst.rd = 8;
    inst.rs = 9;
    inst.rt = 10;

    EXPECT_EQ(
        encodeInstruction(inst), 
        0x012A4020
    );
}

TEST(EncoderTest, EncodesSub) {
    Instruction inst;
    inst.mnemonic = "sub";
    inst.rd = 8;
    inst.rs = 9;
    inst.rt = 10;

    EXPECT_EQ(
        encodeInstruction(inst),
        0x012A4022
    );
}

TEST(EncoderTest, EncodesSll) {
    Instruction inst;

    inst.mnemonic = "sll";
    inst.rd = 8;
    inst.rt = 9;
    inst.shamt = 4;

    EXPECT_EQ(
        encodeInstruction(inst),
        0x00094100u
    );
}

TEST(EncoderTest, EncodesAddi) {
    Instruction inst;

    inst.mnemonic = "addi";
    inst.rs = 9;
    inst.rt = 8;
    inst.immediate = 5;

    EXPECT_EQ(
        encodeInstruction(inst),
        0x21280005u
    );
}

TEST(EncoderTest, EncodesNegativeImmediate) {
    Instruction inst;

    inst.mnemonic = "addi";
    inst.rs = 9;
    inst.rt = 8;
    inst.immediate = -1;

    EXPECT_EQ(
        encodeInstruction(inst),
        0x2128FFFFu
    );
}

TEST(EncoderTest, EncodesLw) {
    Instruction inst;

    inst.mnemonic = "lw";
    inst.rs = 29;
    inst.rt = 8;
    inst.immediate = 12;

    EXPECT_EQ(
        encodeInstruction(inst),
        0x8FA8000Cu
    );
}

TEST(EncoderTest, EncodesOri) {
    Instruction inst;

    inst.mnemonic = "ori";
    inst.rs = 9;
    inst.rt = 8;
    inst.immediate = 0xFFFF;

    EXPECT_EQ(
        encodeInstruction(inst),
        0x3528FFFFu
    );
}

TEST(EncoderTest, EncodesJump) {
    Instruction inst;
    inst.mnemonic = "j";
    inst.target = 0x00100002;

    EXPECT_EQ(
        encodeInstruction(inst, 1),
        0x08100002u
    );
}

namespace {

struct EncodingCase {
    const char* mnemonic;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    int32_t immediate;
    uint32_t target;
    uint32_t expected;
};

void PrintTo(const EncodingCase& testCase, std::ostream* output) {
    *output << testCase.mnemonic;
}

class SupportedInstructionEncodingTest
    : public ::testing::TestWithParam<EncodingCase> {};

TEST_P(SupportedInstructionEncodingTest, MatchesKnownMachineWord) {
    const auto& testCase = GetParam();
    Instruction inst;
    inst.mnemonic = testCase.mnemonic;
    inst.rs = testCase.rs;
    inst.rt = testCase.rt;
    inst.rd = testCase.rd;
    inst.shamt = testCase.shamt;
    inst.immediate = testCase.immediate;
    inst.target = testCase.target;

    EXPECT_EQ(encodeInstruction(inst), testCase.expected);
}

// Expected words are fixed MIPS encodings, independent of instructionInfos.
INSTANTIATE_TEST_SUITE_P(
    EverySupportedMnemonic,
    SupportedInstructionEncodingTest,
    ::testing::Values(
        EncodingCase{"add",   9, 10, 8, 0,      0,          0, 0x012A4020u},
        EncodingCase{"addu",  9, 10, 8, 0,      0,          0, 0x012A4021u},
        EncodingCase{"sub",   9, 10, 8, 0,      0,          0, 0x012A4022u},
        EncodingCase{"subu",  9, 10, 8, 0,      0,          0, 0x012A4023u},
        EncodingCase{"and",   9, 10, 8, 0,      0,          0, 0x012A4024u},
        EncodingCase{"or",    9, 10, 8, 0,      0,          0, 0x012A4025u},
        EncodingCase{"xor",   9, 10, 8, 0,      0,          0, 0x012A4026u},
        EncodingCase{"nor",   9, 10, 8, 0,      0,          0, 0x012A4027u},
        EncodingCase{"slt",   9, 10, 8, 0,      0,          0, 0x012A402Au},
        EncodingCase{"sltu",  9, 10, 8, 0,      0,          0, 0x012A402Bu},
        EncodingCase{"sll",   0,  9, 8, 4,      0,          0, 0x00094100u},
        EncodingCase{"srl",   0,  9, 8, 4,      0,          0, 0x00094102u},
        EncodingCase{"sra",   0,  9, 8, 4,      0,          0, 0x00094103u},
        EncodingCase{"jr",   31,  0, 0, 0,      0,          0, 0x03E00008u},
        EncodingCase{"addi",  9,  8, 0, 0, 0x1234,          0, 0x21281234u},
        EncodingCase{"addiu", 9,  8, 0, 0, 0x1234,          0, 0x25281234u},
        EncodingCase{"andi",  9,  8, 0, 0, 0x1234,          0, 0x31281234u},
        EncodingCase{"ori",   9,  8, 0, 0, 0x1234,          0, 0x35281234u},
        EncodingCase{"xori",  9,  8, 0, 0, 0x1234,          0, 0x39281234u},
        EncodingCase{"slti",  9,  8, 0, 0, 0x1234,          0, 0x29281234u},
        EncodingCase{"sltiu", 9,  8, 0, 0, 0x1234,          0, 0x2D281234u},
        EncodingCase{"lui",   0,  8, 0, 0, 0xABCD,          0, 0x3C08ABCDu},
        EncodingCase{"lw",   29,  8, 0, 0,    -16,          0, 0x8FA8FFF0u},
        EncodingCase{"sw",   29,  8, 0, 0,    -16,          0, 0xAFA8FFF0u},
        EncodingCase{"lb",   29,  8, 0, 0,    -16,          0, 0x83A8FFF0u},
        EncodingCase{"lbu",  29,  8, 0, 0,    -16,          0, 0x93A8FFF0u},
        EncodingCase{"sb",   29,  8, 0, 0,    -16,          0, 0xA3A8FFF0u},
        EncodingCase{"beq",   9,  8, 0, 0,     -2,          0, 0x1128FFFEu},
        EncodingCase{"bne",   9,  8, 0, 0,     -2,          0, 0x1528FFFEu},
        EncodingCase{"j",     0,  0, 0, 0,      0, 0x00100002, 0x08100002u},
        EncodingCase{"jal",   0,  0, 0, 0,      0, 0x00100002, 0x0C100002u}
    ),
    [](const ::testing::TestParamInfo<EncodingCase>& info) {
        return std::string(info.param.mnemonic);
    }
);

}  // namespace

TEST(EncoderTest, KeepsRegisterFieldsSeparateAtTheirLimits) {
    Instruction inst;
    inst.mnemonic = "add";
    EXPECT_EQ(encodeInstruction(inst), 0x00000020u);

    inst.rs = 31;
    EXPECT_EQ(encodeInstruction(inst), 0x03E00020u);

    inst.rs = 0;
    inst.rt = 31;
    EXPECT_EQ(encodeInstruction(inst), 0x001F0020u);

    inst.rt = 0;
    inst.rd = 31;
    EXPECT_EQ(encodeInstruction(inst), 0x0000F820u);

    inst.rs = 31;
    inst.rt = 31;
    EXPECT_EQ(encodeInstruction(inst), 0x03FFF820u);
}

TEST(EncoderTest, EncodesShiftAmountLimits) {
    Instruction inst;
    inst.mnemonic = "sll";
    EXPECT_EQ(encodeInstruction(inst), 0x00000000u);

    inst.rd = 31;
    inst.rt = 31;
    inst.shamt = 31;
    EXPECT_EQ(encodeInstruction(inst), 0x001FFFC0u);
}

TEST(EncoderTest, EncodesSignedImmediateLimitsWithoutCorruptingRegisters) {
    Instruction inst;
    inst.mnemonic = "addi";
    EXPECT_EQ(encodeInstruction(inst), 0x20000000u);

    inst.rs = 31;
    inst.rt = 31;
    inst.immediate = -32768;
    EXPECT_EQ(encodeInstruction(inst), 0x23FF8000u);

    inst.immediate = 32767;
    EXPECT_EQ(encodeInstruction(inst), 0x23FF7FFFu);
}

TEST(EncoderTest, EncodesUnsignedImmediateLimits) {
    Instruction inst;
    inst.mnemonic = "andi";
    EXPECT_EQ(encodeInstruction(inst), 0x30000000u);

    inst.rs = 31;
    inst.rt = 31;
    inst.immediate = 65535;
    EXPECT_EQ(encodeInstruction(inst), 0x33FFFFFFu);

    inst.mnemonic = "lui";
    inst.rs = 0;
    EXPECT_EQ(encodeInstruction(inst), 0x3C1FFFFFu);
}

TEST(EncoderTest, EncodesJumpTargetLimits) {
    Instruction inst;
    for (const auto* mnemonic : {"j", "jal"}) {
        SCOPED_TRACE(mnemonic);
        inst.mnemonic = mnemonic;
        inst.target = 0;
        EXPECT_EQ(encodeInstruction(inst),
                  inst.mnemonic == "j" ? 0x08000000u : 0x0C000000u);

        inst.target = 0x03FFFFFF;
        EXPECT_EQ(encodeInstruction(inst),
                  inst.mnemonic == "j" ? 0x0BFFFFFFu : 0x0FFFFFFFu);
    }
}
