#include <gtest/gtest.h>

#include "assembler/tokenizer.h"

TEST(TokenizerTest, TokenizesLines) {
    std::string line = "add $t0, $t1, $t2";
    auto tokens = tokenize(line);
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0], "add");
    EXPECT_EQ(tokens[1], "$t0");
    EXPECT_EQ(tokens[2], "$t1");
    EXPECT_EQ(tokens[3], "$t2");
}

TEST(TokenizerTest, TokenizesLinesWithComments) {
    std::string line = "add $t0, $t1, $t2 # This is a comment";
    auto tokens = tokenize(line);
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0], "add");
    EXPECT_EQ(tokens[1], "$t0");
    EXPECT_EQ(tokens[2], "$t1");
    EXPECT_EQ(tokens[3], "$t2");
}

TEST(TokenizerTest, TokenizesLinesWithParentheses) {
    std::string line = "lw   $t0,  12($sp)";
    auto tokens = tokenize(line);
    ASSERT_EQ(tokens.size(), 4);
    EXPECT_EQ(tokens[0], "lw");
    EXPECT_EQ(tokens[1], "$t0");
    EXPECT_EQ(tokens[2], "12");
    EXPECT_EQ(tokens[3], "$sp");
}

TEST(TokenizerTest, TokenizesEmptyLines) {
    EXPECT_TRUE(tokenize("").empty());
}

TEST(TokenizerTest, TokenizesLinesWithOnlyComments) {
    EXPECT_TRUE(tokenize("   # only comment").empty());
}

TEST(TokenizerTest, TokenizesLinesWithOnlyWhitespace) {
    EXPECT_TRUE(tokenize("   ").empty());
}

TEST(TokenizerTest, StopsAtFirstCommentMarkerWithoutRequiringWhitespace) {
    const std::vector<std::string> expected = {"add", "$t0", "$t1", "$t2"};
    for (const std::string suffix : {"#comment", "# (malformed, operands)",
                                     "## another # comment"}) {
        SCOPED_TRACE(suffix);
        EXPECT_EQ(tokenize("add $t0,$t1,$t2" + suffix), expected);
    }
    EXPECT_TRUE(tokenize("#add $t0,$t1,$t2").empty());
    EXPECT_EQ(tokenize("addi $t0,$t1,12#34"),
              (std::vector<std::string>{"addi", "$t0", "$t1", "12"}));
}

TEST(TokenizerTest, HandlesTabsCarriageReturnsAndSpacedMemoryOperands) {
    EXPECT_EQ(tokenize("\tlw\t$ra,\t-0x10($zero)\r"),
              (std::vector<std::string>{"lw", "$ra", "-0x10", "$zero"}));
    EXPECT_EQ(tokenize(" lw $t0 , 010 ( $sp ) "),
              (std::vector<std::string>{"lw", "$t0", "010", "$sp"}));
    EXPECT_TRUE(tokenize("\t\r \t").empty());
}

TEST(TokenizerTest, PreservesNumericSpellingAndSymbolCharacters) {
    EXPECT_EQ(tokenize("addi $t0, $zero, +0X1F"),
              (std::vector<std::string>{"addi", "$t0", "$zero", "+0X1F"}));
    EXPECT_EQ(tokenize("beq $t0,$t1,a.b$c_9 # label reference"),
              (std::vector<std::string>{"beq", "$t0", "$t1", "a.b$c_9"}));
}
