#include <gtest/gtest.h>

#include "assembler/symbol_table.h"

TEST(SymbolTableTest, RejectsDuplicateLabels) {
    std::vector<SourceLine> lines = {
        {1, "loop:"},
        {2, "addi $t0, $zero, 1"},
        {3, "loop:"}
    };

    EXPECT_THROW(
        buildSymbolTable(lines),
        std::runtime_error
    );
}

TEST(SymbolTableTest, BuildsLabelAddresses) {
    std::vector<SourceLine> lines = {
        {1, "main:"},
        {2, "addi $t0, $zero, 5"},
        {3, ""},
        {4, "loop:"},
        {5, "addi $t0, $t0, -1"},
        {6, "bne $t0, $zero, loop"}
    };

    SymbolTable symbols = buildSymbolTable(lines);

    EXPECT_EQ(symbols.at("main"), 0x00400000u);
    EXPECT_EQ(symbols.at("loop"), 0x00400004u);
}