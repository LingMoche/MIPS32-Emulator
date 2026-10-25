#include <gtest/gtest.h>

#include "assembler/label_resolver.h"

TEST(LabelResolverTest, ResolvesBackwardBranch) {
    SymbolTable symbols = {
        {"loop", 0x00400004}
    };

    Instruction inst;
    inst.mnemonic = "bne";
    inst.label = "loop";

    uint32_t pc = 0x00400008;

    resolveLabel(inst, pc, symbols, 1);

    EXPECT_EQ(inst.immediate, -2);
}

TEST(LabelResolverTest, ResolvesForwardBranch) {
    SymbolTable symbols = {
        {"done", 0x00400010}
    };

    Instruction inst;
    inst.mnemonic = "beq";
    inst.label = "done";

    uint32_t pc = 0x00400004;

    resolveLabel(inst, pc, symbols, 1);

    EXPECT_EQ(inst.immediate, 2);
}

TEST(LabelResolverTest, RejectsUndefinedLabel) {
    SymbolTable symbols;

    Instruction inst;
    inst.mnemonic = "bne";
    inst.label = "missing";

    EXPECT_THROW(
        resolveLabel(
            inst,
            0x00400000,
            symbols,
            1
        ),
        std::runtime_error
    );
}

TEST(LabelResolverTest, ResolvesBranchToNextInstruction) {
    SymbolTable symbols = {
        {"next", 0x00400008}
    };

    Instruction inst;
    inst.mnemonic = "beq";
    inst.label = "next";

    uint32_t pc = 0x00400004;

    resolveLabel(inst, pc, symbols, 1);

    EXPECT_EQ(inst.immediate, 0);
}

TEST(LabelResolverTest, ResolvesJumpTarget) {
    SymbolTable symbols = {
        {"loop", 0x00400008}
    };

    Instruction inst;
    inst.mnemonic = "j";
    inst.label = "loop";

    resolveLabel(
        inst,
        0x00400000,
        symbols,
        1
    );

    EXPECT_EQ(inst.target, 0x00100002u);
}

TEST(LabelResolverTest, PreservesInstructionFieldsWhenThereIsNoLabel) {
    for (const auto* mnemonic : {"beq", "bne", "j", "jal", "addi"}) {
        SCOPED_TRACE(mnemonic);
        Instruction inst;
        inst.mnemonic = mnemonic;
        inst.rs = 1;
        inst.rt = 2;
        inst.rd = 3;
        inst.shamt = 4;
        inst.immediate = -123;
        inst.target = 0x00123456;

        resolveLabel(inst, 0x00400000, {}, 17);

        EXPECT_EQ(inst.mnemonic, mnemonic);
        EXPECT_TRUE(inst.label.empty());
        EXPECT_EQ(inst.rs, 1);
        EXPECT_EQ(inst.rt, 2);
        EXPECT_EQ(inst.rd, 3);
        EXPECT_EQ(inst.shamt, 4);
        EXPECT_EQ(inst.immediate, -123);
        EXPECT_EQ(inst.target, 0x00123456u);
    }
}

TEST(LabelResolverTest, ResolvesBothSignedBranchLimits) {
    for (const auto* mnemonic : {"beq", "bne"}) {
        SCOPED_TRACE(mnemonic);
        for (const int32_t offset : {-32768, 32767}) {
            SCOPED_TRACE(offset);
            Instruction inst;
            inst.mnemonic = mnemonic;
            inst.label = "boundary";
            const uint32_t target = static_cast<uint32_t>(
                int64_t{0x00420004} + int64_t{offset} * 4);

            resolveLabel(inst, 0x00420000, {{"boundary", target}}, 23);

            EXPECT_EQ(inst.immediate, offset);
            EXPECT_EQ(inst.label, "boundary");
        }
    }
}

TEST(LabelResolverTest, RejectsBranchesOneWordBeyondEitherSignedLimit) {
    for (const auto* mnemonic : {"beq", "bne"}) {
        SCOPED_TRACE(mnemonic);
        for (const int32_t offset : {-32769, 32768}) {
            SCOPED_TRACE(offset);
            Instruction inst;
            inst.mnemonic = mnemonic;
            inst.label = "too_far";
            inst.immediate = 123;
            const uint32_t target = static_cast<uint32_t>(
                int64_t{0x00420004} + int64_t{offset} * 4);

            EXPECT_THROW(resolveLabel(inst, 0x00420000,
                                      {{"too_far", target}}, 24),
                         std::runtime_error);
            EXPECT_EQ(inst.immediate, 123);
        }
    }
}

TEST(LabelResolverTest, ResolvesJumpRegionEndpoints) {
    for (const auto* mnemonic : {"j", "jal"}) {
        SCOPED_TRACE(mnemonic);
        Instruction inst;
        inst.mnemonic = mnemonic;
        inst.label = "destination";

        resolveLabel(inst, 0x10000100,
                     {{"destination", 0x10000000}}, 25);
        EXPECT_EQ(inst.target, 0u);

        resolveLabel(inst, 0x10000100,
                     {{"destination", 0x1FFFFFFC}}, 25);
        EXPECT_EQ(inst.target, 0x03FFFFFFu);
    }
}

TEST(LabelResolverTest, RejectsEveryUnalignedJumpTarget) {
    for (const auto* mnemonic : {"j", "jal"}) {
        SCOPED_TRACE(mnemonic);
        for (const uint32_t remainder : {1u, 2u, 3u}) {
            SCOPED_TRACE(remainder);
            Instruction inst;
            inst.mnemonic = mnemonic;
            inst.label = "unaligned";
            inst.target = 123;

            EXPECT_THROW(resolveLabel(inst, 0x00400000,
                                      {{"unaligned", 0x00400100 + remainder}}, 26),
                         std::runtime_error);
            EXPECT_EQ(inst.target, 123u);
        }
    }
}

TEST(LabelResolverTest, RejectsJumpToAnother256MbRegion) {
    for (const auto* mnemonic : {"j", "jal"}) {
        SCOPED_TRACE(mnemonic);
        Instruction inst;
        inst.mnemonic = mnemonic;
        inst.label = "far_away";
        inst.target = 123;

        EXPECT_THROW(resolveLabel(inst, 0x00400000,
                                  {{"far_away", 0x10000000}}, 27),
                     std::runtime_error);
        EXPECT_EQ(inst.target, 123u);
    }
}

TEST(LabelResolverTest, UsesPcPlusFourForJumpRegionAtBoundary) {
    for (const auto* mnemonic : {"j", "jal"}) {
        SCOPED_TRACE(mnemonic);
        Instruction inst;
        inst.mnemonic = mnemonic;
        inst.label = "destination";

        resolveLabel(inst, 0x0FFFFFFC,
                     {{"destination", 0x10000000}}, 28);
        EXPECT_EQ(inst.target, 0u);

        EXPECT_THROW(resolveLabel(inst, 0x0FFFFFFC,
                                  {{"destination", 0x0FFFFFFC}}, 28),
                     std::runtime_error);
    }
}

TEST(LabelResolverTest, UsesWrappedPcPlusFourForJumpRegion) {
    for (const auto* mnemonic : {"j", "jal"}) {
        SCOPED_TRACE(mnemonic);
        Instruction inst;
        inst.mnemonic = mnemonic;
        inst.label = "destination";

        resolveLabel(inst, 0xFFFFFFFC,
                     {{"destination", 0x00000000}}, 29);
        EXPECT_EQ(inst.target, 0u);

        EXPECT_THROW(resolveLabel(inst, 0xFFFFFFFC,
                                  {{"destination", 0xFFFFFFFC}}, 29),
                     std::runtime_error);
    }
}

TEST(LabelResolverTest, UndefinedLabelErrorIncludesSourceLineAndName) {
    Instruction inst;
    inst.mnemonic = "jal";
    inst.label = "missing_target";

    try {
        resolveLabel(inst, 0x00400000, {}, 41);
        FAIL() << "An undefined label must be rejected";
    }
    catch (const std::runtime_error& exception) {
        EXPECT_EQ(std::string(exception.what()),
                  "Line 41: Undefined label: missing_target");
    }
}
