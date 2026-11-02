#include <gtest/gtest.h>

#include "assembler/registers.h"
#include <stdexcept>
#include <string>
#include <vector>

TEST(RegisterTest, ParsesNamedRegisters) {
    const std::vector<std::string> names = {
        "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
        "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
        "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
        "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
    };

    for (std::size_t index = 0; index < names.size(); ++index) {
        SCOPED_TRACE(names[index]);
        EXPECT_EQ(parseRegister(names[index]), static_cast<int>(index));
    }
}

TEST(RegisterTest, RejectsInvalidRegister) {
    EXPECT_THROW(parseRegister("$t10"), std::runtime_error);
    EXPECT_THROW(parseRegister("$32"), std::runtime_error);
    EXPECT_THROW(parseRegister("t0"), std::runtime_error);
    EXPECT_THROW(parseRegister("$banana"), std::runtime_error);
}

TEST(RegisterTest, ReportsMalformedAndOutOfRangeNamesWithOptionalLineNumber) {
    for (const std::string name : {"", "$", "$v2", "$a4", "$t10", "$s9",
                                   "$k2", "$t0junk", "t0"}) {
        for (const int lineNumber : {0, 19}) {
            SCOPED_TRACE(name);
            SCOPED_TRACE(lineNumber);
            const std::string prefix = lineNumber == 0 ? "" : "Line 19: ";
            const std::string suffix = name.empty() ? "" : ": " + name;
            try {
                parseRegister(name, lineNumber);
                FAIL() << "Expected invalid register name";
            }
            catch (const std::runtime_error& exception) {
                EXPECT_EQ(std::string(exception.what()),
                          prefix + "Invalid register name" + suffix);
            }
        }
    }
}

