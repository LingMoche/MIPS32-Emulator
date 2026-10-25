#include <gtest/gtest.h>

#include "assembler/encoder.h"

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