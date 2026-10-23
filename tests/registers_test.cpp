#include <gtest/gtest.h>

#include "assembler/registers.h"
#include <stdexcept>

TEST(RegisterTest, ParsesNamedRegisters) {
    EXPECT_EQ(parseRegister("$zero"), 0);
    EXPECT_EQ(parseRegister("$at"), 1);

    EXPECT_EQ(parseRegister("$v0"), 2);
    EXPECT_EQ(parseRegister("$v1"), 3);

    EXPECT_EQ(parseRegister("$a0"), 4);
    EXPECT_EQ(parseRegister("$a3"), 7);

    EXPECT_EQ(parseRegister("$t0"), 8);
    EXPECT_EQ(parseRegister("$t7"), 15);

    EXPECT_EQ(parseRegister("$s0"), 16);
    EXPECT_EQ(parseRegister("$s7"), 23);

    EXPECT_EQ(parseRegister("$t8"), 24);
    EXPECT_EQ(parseRegister("$t9"), 25);

    EXPECT_EQ(parseRegister("$k0"), 26);
    EXPECT_EQ(parseRegister("$k1"), 27);

    EXPECT_EQ(parseRegister("$gp"), 28);
    EXPECT_EQ(parseRegister("$sp"), 29);
    EXPECT_EQ(parseRegister("$fp"), 30);
    EXPECT_EQ(parseRegister("$ra"), 31);
}

TEST(RegisterTest, RejectsInvalidRegister) {
    EXPECT_THROW(parseRegister("$t10"), std::runtime_error);
    EXPECT_THROW(parseRegister("$32"), std::runtime_error);
    EXPECT_THROW(parseRegister("t0"), std::runtime_error);
    EXPECT_THROW(parseRegister("$banana"), std::runtime_error);
}

