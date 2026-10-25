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