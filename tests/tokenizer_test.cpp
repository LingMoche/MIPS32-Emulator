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