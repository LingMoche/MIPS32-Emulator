#include <gtest/gtest.h>

#include "assembler/symbol_table.h"
#include "assembler/assembler.h"
#include "assembler/parser.h"

#include <stdexcept>
#include <string>

namespace {

template <typename Action>
void expectLabelError(Action action, const std::string& label, int lineNumber) {
    const std::string expected = "Line " + std::to_string(lineNumber) +
        ": Invalid label" + (label.empty() ? "" : ": " + label);
    try {
        action();
        FAIL() << "Expected std::runtime_error: " << expected;
    }
    catch (const std::runtime_error& exception) {
        EXPECT_EQ(std::string(exception.what()), expected);
    }
}

} // namespace

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

TEST(SymbolTableTest, AcceptsSameNamesInDefinitionsAndAllLabelReferences) {
    for (const std::string label : {"target", "Loop42", "_loop", ".L1",
                                    "$entry", "a.b$c_9", "_", ".", "$"}) {
        SCOPED_TRACE(label);
        const SymbolTable symbols = buildSymbolTable({{171, label + ":"}});
        ASSERT_EQ(symbols.size(), 1u);
        EXPECT_EQ(symbols.at(label), 0x00400000u);

        for (const std::string mnemonic : {"beq", "bne", "j", "jal"}) {
            SCOPED_TRACE(mnemonic);
            const bool isBranch = mnemonic == "beq" || mnemonic == "bne";
            const std::vector<std::string> tokens = isBranch
                ? std::vector<std::string>{mnemonic, "$t0", "$t1", label}
                : std::vector<std::string>{mnemonic, label};
            EXPECT_EQ(parseInstruction(tokens, 181).label, label);
        }
    }
}

TEST(SymbolTableTest, RejectsSameInvalidNamesInDefinitionsAndAllLabelReferences) {
    for (const std::string label : {"", "123", "1loop", "foo-bar", "foo:bar",
                                    "foo:", "loop+4", "a/b", "a*b"}) {
        SCOPED_TRACE(label);
        expectLabelError(
            [&] { buildSymbolTable({{211, label + ":"}}); }, label, 211);

        for (const std::string mnemonic : {"beq", "bne", "j", "jal"}) {
            SCOPED_TRACE(mnemonic);
            const bool isBranch = mnemonic == "beq" || mnemonic == "bne";
            const std::vector<std::string> tokens = isBranch
                ? std::vector<std::string>{mnemonic, "$t0", "$t1", label}
                : std::vector<std::string>{mnemonic, label};
            expectLabelError([&] { parseInstruction(tokens, 223); }, label, 223);
        }
    }
}

TEST(SymbolTableTest, AssemblyRejectsInvalidUnusedLabelDefinition) {
    const std::vector<SourceLine> lines = {
        {17, "bad-name:"},
        {18, "addi $t0, $zero, 1"}
    };

    expectLabelError([&] { assemble(lines); }, "bad-name", 17);
}

TEST(SymbolTableTest, RecognizesOnlySingleColonTerminatedTokens) {
    EXPECT_FALSE(isLabel({}));
    EXPECT_FALSE(isLabel({""}));
    EXPECT_FALSE(isLabel({"loop"}));
    EXPECT_FALSE(isLabel({"loop:", "add"}));
    EXPECT_TRUE(isLabel({"loop:"}));
    EXPECT_TRUE(isLabel({".L1:"}));
}

TEST(SymbolTableTest, BuildsEmptyTableForEmptyAndCommentOnlySources) {
    EXPECT_TRUE(buildSymbolTable({}).empty());
    EXPECT_TRUE(buildSymbolTable({
        {1, ""}, {3, " \t\r"}, {7, "# comment"}, {11, " \t# comment"}
    }).empty());
}

TEST(SymbolTableTest, ConsecutiveAndTrailingLabelsUseInstructionAddresses) {
    const SymbolTable symbols = buildSymbolTable({
        {10, "first:"},
        {20, " \t# comment between labels"},
        {30, "alias: # same address"},
        {40, "addi $t0, $zero, 1"},
        {80, ""},
        {90, "last:"},
        {100, "end_alias:"}
    });

    ASSERT_EQ(symbols.size(), 4u);
    EXPECT_EQ(symbols.at("first"), 0x00400000u);
    EXPECT_EQ(symbols.at("alias"), 0x00400000u);
    EXPECT_EQ(symbols.at("last"), 0x00400004u);
    EXPECT_EQ(symbols.at("end_alias"), 0x00400004u);
}

TEST(SymbolTableTest, KeepsCaseSensitiveLabelNamesDistinct) {
    const SymbolTable symbols = buildSymbolTable({
        {1, "Loop:"}, {2, "add $t0, $t1, $t2"}, {3, "loop:"}
    });

    ASSERT_EQ(symbols.size(), 2u);
    EXPECT_EQ(symbols.at("Loop"), 0x00400000u);
    EXPECT_EQ(symbols.at("loop"), 0x00400004u);
}

TEST(SymbolTableTest, DuplicateLabelErrorIdentifiesTheSecondDefinition) {
    try {
        buildSymbolTable({{17, "loop:"}, {213, "loop: # duplicate"}});
        FAIL() << "Expected duplicate label rejection";
    }
    catch (const std::runtime_error& exception) {
        EXPECT_EQ(std::string(exception.what()),
                  "Line 213: Duplicate label: loop");
    }
}
