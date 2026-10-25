#include <gtest/gtest.h>

#include "assembler/parser.h"
#include "assembler/instruction_table.h"
#include "assembler/tokenizer.h"
#include "error.h"

#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace {

template <typename Action>
void expectErrorMessage(Action action, const std::string& expected) {
    try {
        action();
        FAIL() << "Expected std::runtime_error: " << expected;
    }
    catch (const std::runtime_error& exception) {
        EXPECT_EQ(std::string(exception.what()), expected);
    }
}

} // namespace

TEST(ErrorTest, FormatsErrorsWithAndWithoutLineNumbers) {
    expectErrorMessage([] { error("test.asm", "Failed to open file"); },
                       "Failed to open file: test.asm");
    expectErrorMessage([] { error("$t10", "Invalid register name", 12); },
                       "Line 12: Invalid register name: $t10");
    expectErrorMessage([] { error("", "Cannot parse empty instruction", 7); },
                       "Line 7: Cannot parse empty instruction");
}

TEST(ParseNumberTest, ParsesSignedAndUnsignedBoundaries) {
    struct Case {
        std::string token;
        int bits;
        bool isUnsigned;
        int32_t expected;
    };
    const std::vector<Case> cases = {
        {"0", 5, true, 0},
        {"31", 5, true, 31},
        {"-32768", 16, false, -32768},
        {"32767", 16, false, 32767},
        {"0", 16, true, 0},
        {"65535", 16, true, 65535},
        {"-1", 1, false, -1},
        {"0", 1, false, 0},
        {"0", 1, true, 0},
        {"1", 1, true, 1},
        {"-2147483648", 32, false, std::numeric_limits<int32_t>::min()},
        {"2147483647", 32, false, std::numeric_limits<int32_t>::max()},
        {"2147483647", 31, true, std::numeric_limits<int32_t>::max()},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.token);
        EXPECT_EQ(parseNumber(testCase.token, testCase.bits,
                              testCase.isUnsigned),
                  testCase.expected);
    }
}

TEST(ParseNumberTest, RejectsNumbersOutsideRequestedRange) {
    struct Case {
        std::string token;
        int bits;
        bool isUnsigned;
    };
    const std::vector<Case> cases = {
        {"-1", 5, true},
        {"32", 5, true},
        {"-32769", 16, false},
        {"32768", 16, false},
        {"-1", 16, true},
        {"65536", 16, true},
        {"-2", 1, false},
        {"1", 1, false},
        {"2", 1, true},
        {"-2147483649", 32, false},
        {"2147483648", 32, false},
        {"2147483648", 31, true},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.token);
        const std::string signedness = testCase.isUnsigned ? "unsigned" : "signed";
        expectErrorMessage(
            [&] { parseNumber(testCase.token, testCase.bits, testCase.isUnsigned); },
            "Integer does not fit in " + std::to_string(testCase.bits) + " " +
                signedness + " bits: " + testCase.token);
    }
}

TEST(ParseNumberTest, RejectsInvalidBitCountsBeforeCalculatingRange) {
    for (const bool isUnsigned : {false, true}) {
        const int maxBits = isUnsigned ? 31 : 32;
        const std::string signedness = isUnsigned ? "unsigned" : "signed";
        for (const int bits : {-1, 0, maxBits + 1, std::numeric_limits<int>::max()}) {
            SCOPED_TRACE(bits);
            SCOPED_TRACE(signedness);
            expectErrorMessage(
                [&] { parseNumber("1", bits, isUnsigned); },
                "Bit count must be between 1 and " + std::to_string(maxBits) +
                    " for " + signedness + " integers: 1");
        }
    }
}

TEST(ParseNumberTest, RejectsMalformedOrPartiallyConsumedIntegers) {
    for (const std::string token : {"abc", "123abc", "0x", "08", "1.5",
                                    "+", "10 "}) {
        SCOPED_TRACE(token);
        expectErrorMessage([&] { parseNumber(token, 16, false); },
                           "Invalid integer: " + token);
    }
    expectErrorMessage([] { parseNumber("", 16, false); }, "Invalid integer");
}

TEST(ParseNumberTest, ReportsConversionOverflowAsOutOfRange) {
    for (const std::string token : {"999999999999999999999999999",
                                    "-999999999999999999999999999"}) {
        SCOPED_TRACE(token);
        expectErrorMessage([&] { parseNumber(token, 16, false); },
                           "Integer does not fit in 16 signed bits: " + token);
    }
}

TEST(ParseNumberTest, DetectsDecimalHexadecimalAndOctalBases) {
    EXPECT_EQ(parseNumber("31", 5, true), 31);
    EXPECT_EQ(parseNumber("0x1f", 5, true), 31);
    EXPECT_EQ(parseNumber("0XFFFF", 16, true), 65535);
    EXPECT_EQ(parseNumber("037", 5, true), 31);
    EXPECT_EQ(parseNumber("010", 5, true), 8);
    EXPECT_EQ(parseNumber("-0x8000", 16, false), -32768);
    EXPECT_EQ(parseNumber("-010", 16, false), -8);
    EXPECT_EQ(parseNumber("+31", 5, true), 31);
}

TEST(ParseNumberTest, IncludesLineNumberInEveryErrorKind) {
    expectErrorMessage([] { parseNumber("abc", 16, false, 61); },
                       "Line 61: Invalid integer: abc");
    expectErrorMessage([] { parseNumber("32768", 16, false, 62); },
                       "Line 62: Integer does not fit in 16 signed bits: 32768");
    expectErrorMessage([] { parseNumber("1", 32, true, 63); },
                       "Line 63: Bit count must be between 1 and 31 for unsigned integers: 1");
    expectErrorMessage(
        [] { parseNumber("999999999999999999999999999", 16, false, 64); },
        "Line 64: Integer does not fit in 16 signed bits: 999999999999999999999999999");
}

TEST(ParseNumberTest, EnforcesBoundariesForEverySupportedSignedWidth) {
    for (int bits = 1; bits <= 32; ++bits) {
        SCOPED_TRACE(bits);
        const long long minimum = -(1LL << (bits - 1));
        const long long maximum = (1LL << (bits - 1)) - 1;
        EXPECT_EQ(parseNumber(std::to_string(minimum), bits, false), minimum);
        EXPECT_EQ(parseNumber(std::to_string(maximum), bits, false), maximum);
        for (const long long value : {minimum - 1, maximum + 1}) {
            const std::string token = std::to_string(value);
            SCOPED_TRACE(token);
            expectErrorMessage(
                [&] { parseNumber(token, bits, false); },
                "Integer does not fit in " + std::to_string(bits) +
                    " signed bits: " + token);
        }
    }
}

TEST(ParseNumberTest, EnforcesBoundariesForEverySupportedUnsignedWidth) {
    for (int bits = 1; bits <= 31; ++bits) {
        SCOPED_TRACE(bits);
        const long long maximum = (1LL << bits) - 1;
        EXPECT_EQ(parseNumber("0", bits, true), 0);
        EXPECT_EQ(parseNumber(std::to_string(maximum), bits, true), maximum);
        for (const long long value : {-1LL, maximum + 1}) {
            const std::string token = std::to_string(value);
            SCOPED_TRACE(token);
            expectErrorMessage(
                [&] { parseNumber(token, bits, true); },
                "Integer does not fit in " + std::to_string(bits) +
                    " unsigned bits: " + token);
        }
    }
}

TEST(ParserTest, ParsesAdd) {
    std::vector<std::string> tokens = {
        "add",
        "$t0",
        "$t1",
        "$t2"
    };

    Instruction inst = parseInstruction(tokens);

    EXPECT_EQ(inst.mnemonic, "add");
    EXPECT_EQ(inst.rd, 8);
    EXPECT_EQ(inst.rs, 9);
    EXPECT_EQ(inst.rt, 10);
}

TEST(ParserTest, RejectsMissingOperand) {
    expectErrorMessage([] { parseInstruction({"add", "$t0", "$t1"}); },
                       "Expect 3 operands: add");
}

TEST(ParserTest, IncludesLineInInstructionAndOperandCountErrors) {
    struct Case {
        std::vector<std::string> tokens;
        std::string message;
    };
    const std::vector<Case> cases = {
        {{}, "Cannot parse empty instruction"},
        {{"invalid"}, "Invalid instruction: invalid"},
        {{"add", "$t0", "$t1"}, "Expect 3 operands: add"},
        {{"sll", "$t0", "$t1"}, "Expect 3 operands: sll"},
        {{"jr"}, "Expect 1 operand: jr"},
        {{"addi", "$t0", "$t1"}, "Expect 3 operands: addi"},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.message);
        expectErrorMessage([&] { parseInstruction(testCase.tokens, 17); },
                           "Line 17: " + testCase.message);
    }
}

TEST(ParserTest, ForwardsLineNumberToEveryRegisterOperand) {
    const std::vector<std::vector<std::string>> cases = {
        {"add", "$t10", "$t1", "$t2"},
        {"add", "$t0", "$t10", "$t2"},
        {"add", "$t0", "$t1", "$t10"},
        {"sll", "$t10", "$t1", "1"},
        {"sll", "$t0", "$t10", "1"},
        {"jr", "$t10"},
    };

    for (const auto& tokens : cases) {
        SCOPED_TRACE(::testing::PrintToString(tokens));
        expectErrorMessage([&] { parseInstruction(tokens, 23); },
                           "Line 23: Invalid register name: $t10");
    }
}

TEST(ParserTest, RejectsMalformedShiftAmountsWithLineNumber) {
    for (const std::string value : {"abc", "2junk"}) {
        SCOPED_TRACE(value);
        expectErrorMessage(
            [&] { parseInstruction({"sll", "$t0", "$t1", value}, 31); },
            "Line 31: Invalid integer: " + value);
    }
}

TEST(ParserTest, RejectsOutOfRangeShiftAmountsBeforeNarrowing) {
    for (const std::string value : {"-1", "32", "256",
                                    "999999999999999999999999999"}) {
        SCOPED_TRACE(value);
        expectErrorMessage(
            [&] { parseInstruction({"sll", "$t0", "$t1", value}, 41); },
            "Line 41: Integer does not fit in 5 unsigned bits: " + value);
    }
}

TEST(ParserTest, ParsesShiftAmountBoundaries) {
    for (const int amount : {0, 31}) {
        SCOPED_TRACE(amount);
        const Instruction inst = parseInstruction(
            {"sll", "$t0", "$t1", std::to_string(amount)}, 53);

        EXPECT_EQ(inst.mnemonic, "sll");
        EXPECT_EQ(inst.rd, 8);
        EXPECT_EQ(inst.rt, 9);
        EXPECT_EQ(inst.shamt, amount);
    }
}

TEST(ParserTest, ParsesHexadecimalAndOctalShiftAmounts) {
    for (const std::string token : {"0x1f", "037"}) {
        SCOPED_TRACE(token);
        const Instruction inst = parseInstruction(
            {"sll", "$t0", "$t1", token}, 71);

        EXPECT_EQ(inst.shamt, 31);
    }
}

TEST(ParserTest, ParsesEverySupportedMnemonicAndInitializesUnusedFields) {
    struct Case {
        std::vector<std::string> tokens;
        int rs;
        int rt;
        int rd;
        int shamt;
        int immediate;
        std::string label;
    };
    const std::vector<Case> cases = {
        {{"add", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"addu", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"sub", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"subu", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"and", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"or", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"xor", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"nor", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"slt", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"sltu", "$t0", "$t1", "$t2"}, 9, 10, 8, 0, 0, ""},
        {{"sll", "$t0", "$t1", "17"}, 0, 9, 8, 17, 0, ""},
        {{"srl", "$t0", "$t1", "17"}, 0, 9, 8, 17, 0, ""},
        {{"sra", "$t0", "$t1", "17"}, 0, 9, 8, 17, 0, ""},
        {{"jr", "$ra"}, 31, 0, 0, 0, 0, ""},
        {{"addi", "$t0", "$t1", "-17"}, 9, 8, 0, 0, -17, ""},
        {{"addiu", "$t0", "$t1", "-17"}, 9, 8, 0, 0, -17, ""},
        {{"andi", "$t0", "$t1", "65535"}, 9, 8, 0, 0, 65535, ""},
        {{"ori", "$t0", "$t1", "65535"}, 9, 8, 0, 0, 65535, ""},
        {{"xori", "$t0", "$t1", "65535"}, 9, 8, 0, 0, 65535, ""},
        {{"slti", "$t0", "$t1", "-17"}, 9, 8, 0, 0, -17, ""},
        {{"sltiu", "$t0", "$t1", "-17"}, 9, 8, 0, 0, -17, ""},
        {{"lui", "$t0", "65535"}, 0, 8, 0, 0, 65535, ""},
        {{"lw", "$t0", "-20", "$sp"}, 29, 8, 0, 0, -20, ""},
        {{"sw", "$t0", "-20", "$sp"}, 29, 8, 0, 0, -20, ""},
        {{"lb", "$t0", "-20", "$sp"}, 29, 8, 0, 0, -20, ""},
        {{"lbu", "$t0", "-20", "$sp"}, 29, 8, 0, 0, -20, ""},
        {{"sb", "$t0", "-20", "$sp"}, 29, 8, 0, 0, -20, ""},
        {{"beq", "$t0", "$t1", "Loop42"}, 8, 9, 0, 0, 0, "Loop42"},
        {{"bne", "$t0", "$t1", "Loop42"}, 8, 9, 0, 0, 0, "Loop42"},
        {{"j", "Loop42"}, 0, 0, 0, 0, 0, "Loop42"},
        {{"jal", "Loop42"}, 0, 0, 0, 0, 0, "Loop42"},
    };

    std::unordered_set<std::string> coveredMnemonics;
    for (const auto& testCase : cases) {
        const std::string& mnemonic = testCase.tokens[0];
        SCOPED_TRACE(mnemonic);
        EXPECT_TRUE(coveredMnemonics.insert(mnemonic).second);
        const Instruction inst = parseInstruction(testCase.tokens, 81);
        EXPECT_EQ(inst.mnemonic, mnemonic);
        EXPECT_EQ(inst.rs, testCase.rs);
        EXPECT_EQ(inst.rt, testCase.rt);
        EXPECT_EQ(inst.rd, testCase.rd);
        EXPECT_EQ(inst.shamt, testCase.shamt);
        EXPECT_EQ(inst.immediate, testCase.immediate);
        EXPECT_EQ(inst.target, 0u);
        EXPECT_EQ(inst.label, testCase.label);
    }
    EXPECT_EQ(coveredMnemonics.size(), instructionInfos.size());
    for (const auto& entry : instructionInfos) {
        EXPECT_EQ(coveredMnemonics.count(entry.first), 1u) << entry.first;
    }
}

TEST(ParserTest, RejectsTooFewAndTooManyOperandsForEveryFormat) {
    struct Case {
        std::vector<std::string> tokens;
        std::string message;
    };
    const std::vector<Case> cases = {
        {{"add", "$t0", "$t1", "$t2"}, "Expect 3 operands: add"},
        {{"sll", "$t0", "$t1", "1"}, "Expect 3 operands: sll"},
        {{"jr", "$ra"}, "Expect 1 operand: jr"},
        {{"addi", "$t0", "$t1", "1"}, "Expect 3 operands: addi"},
        {{"lui", "$t0", "1"}, "Expect 2 operands: lui"},
        {{"lw", "$t0", "12", "$sp"}, "Expect 3 operands: lw"},
        {{"beq", "$t0", "$t1", "loop"}, "Expect 3 operands: beq"},
        {{"j", "loop"}, "Expect 1 operand: j"},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.tokens[0]);
        auto tooFew = testCase.tokens;
        tooFew.pop_back();
        auto tooMany = testCase.tokens;
        tooMany.push_back("extra");
        for (const auto& tokens : {tooFew, tooMany}) {
            SCOPED_TRACE(::testing::PrintToString(tokens));
            expectErrorMessage([&] { parseInstruction(tokens, 91); },
                               "Line 91: " + testCase.message);
        }
    }
}

TEST(ParserTest, ForwardsLineNumberToEveryNewRegisterOperand) {
    struct Case {
        std::vector<std::string> tokens;
        std::vector<std::size_t> registerIndices;
    };
    const std::vector<Case> cases = {
        {{"addi", "$t0", "$t1", "1"}, {1, 2}},
        {{"addiu", "$t0", "$t1", "1"}, {1, 2}},
        {{"andi", "$t0", "$t1", "1"}, {1, 2}},
        {{"ori", "$t0", "$t1", "1"}, {1, 2}},
        {{"xori", "$t0", "$t1", "1"}, {1, 2}},
        {{"slti", "$t0", "$t1", "1"}, {1, 2}},
        {{"sltiu", "$t0", "$t1", "1"}, {1, 2}},
        {{"lui", "$t0", "1"}, {1}},
        {{"lw", "$t0", "12", "$sp"}, {1, 3}},
        {{"sw", "$t0", "12", "$sp"}, {1, 3}},
        {{"lb", "$t0", "12", "$sp"}, {1, 3}},
        {{"lbu", "$t0", "12", "$sp"}, {1, 3}},
        {{"sb", "$t0", "12", "$sp"}, {1, 3}},
        {{"beq", "$t0", "$t1", "loop"}, {1, 2}},
        {{"bne", "$t0", "$t1", "loop"}, {1, 2}},
    };

    for (const auto& testCase : cases) {
        for (const auto index : testCase.registerIndices) {
            auto tokens = testCase.tokens;
            tokens[index] = "$t10";
            SCOPED_TRACE(::testing::PrintToString(tokens));
            expectErrorMessage([&] { parseInstruction(tokens, 101); },
                               "Line 101: Invalid register name: $t10");
        }
    }
}

TEST(ParserTest, ParsesSignedImmediateAndOffsetBoundariesAndBases) {
    struct Case {
        std::string token;
        int expected;
    };
    const std::vector<Case> values = {
        {"-32768", -32768}, {"32767", 32767}, {"0", 0},
        {"-0x8000", -32768}, {"0x7fff", 32767}, {"-010", -8},
        {"077777", 32767},
    };

    for (const std::string mnemonic : {"addi", "addiu", "slti", "sltiu",
                                       "lw", "sw", "lb", "lbu", "sb"}) {
        const bool isMemory = mnemonic == "lw" || mnemonic == "sw" ||
            mnemonic == "lb" || mnemonic == "lbu" || mnemonic == "sb";
        for (const auto& value : values) {
            const std::vector<std::string> tokens = isMemory
                ? std::vector<std::string>{mnemonic, "$t0", value.token, "$sp"}
                : std::vector<std::string>{mnemonic, "$t0", "$t1", value.token};
            SCOPED_TRACE(::testing::PrintToString(tokens));
            const Instruction inst = parseInstruction(tokens);
            EXPECT_EQ(inst.immediate, value.expected);
        }
    }
}

TEST(ParserTest, ParsesUnsignedImmediateBoundariesAndBases) {
    struct Case {
        std::string token;
        int expected;
    };
    const std::vector<Case> values = {
        {"0", 0}, {"65535", 65535}, {"0XFFFF", 65535},
        {"0177777", 65535}, {"010", 8},
    };

    for (const std::string mnemonic : {"andi", "ori", "xori", "lui"}) {
        for (const auto& value : values) {
            const std::vector<std::string> tokens = mnemonic == "lui"
                ? std::vector<std::string>{mnemonic, "$t0", value.token}
                : std::vector<std::string>{mnemonic, "$t0", "$t1", value.token};
            SCOPED_TRACE(::testing::PrintToString(tokens));
            EXPECT_EQ(parseInstruction(tokens).immediate, value.expected);
        }
    }
}

TEST(ParserTest, RejectsMalformedImmediatesAndOffsetsWithLineNumber) {
    struct Case {
        std::vector<std::string> tokens;
        std::size_t immediateIndex;
    };
    const std::vector<Case> cases = {
        {{"addi", "$t0", "$t1", "1"}, 3},
        {{"addiu", "$t0", "$t1", "1"}, 3},
        {{"andi", "$t0", "$t1", "1"}, 3},
        {{"ori", "$t0", "$t1", "1"}, 3},
        {{"xori", "$t0", "$t1", "1"}, 3},
        {{"slti", "$t0", "$t1", "1"}, 3},
        {{"sltiu", "$t0", "$t1", "1"}, 3},
        {{"lui", "$t0", "1"}, 2},
        {{"lw", "$t0", "12", "$sp"}, 2},
        {{"sw", "$t0", "12", "$sp"}, 2},
        {{"lb", "$t0", "12", "$sp"}, 2},
        {{"lbu", "$t0", "12", "$sp"}, 2},
        {{"sb", "$t0", "12", "$sp"}, 2},
    };

    for (const auto& testCase : cases) {
        for (const std::string value : {"", "abc", "123junk", "0x", "08", "1.5"}) {
            auto tokens = testCase.tokens;
            tokens[testCase.immediateIndex] = value;
            SCOPED_TRACE(::testing::PrintToString(tokens));
            const std::string suffix = value.empty() ? "" : ": " + value;
            expectErrorMessage([&] { parseInstruction(tokens, 111); },
                               "Line 111: Invalid integer" + suffix);
        }
    }
}

TEST(ParserTest, RejectsOutOfRangeSignedImmediatesAndOffsetsWithLineNumber) {
    for (const std::string mnemonic : {"addi", "addiu", "slti", "sltiu",
                                       "lw", "sw", "lb", "lbu", "sb"}) {
        const bool isMemory = mnemonic == "lw" || mnemonic == "sw" ||
            mnemonic == "lb" || mnemonic == "lbu" || mnemonic == "sb";
        for (const std::string value : {"-32769", "32768", "65535",
                                        "999999999999999999999999999"}) {
            const std::vector<std::string> tokens = isMemory
                ? std::vector<std::string>{mnemonic, "$t0", value, "$sp"}
                : std::vector<std::string>{mnemonic, "$t0", "$t1", value};
            SCOPED_TRACE(::testing::PrintToString(tokens));
            expectErrorMessage(
                [&] { parseInstruction(tokens, 121); },
                "Line 121: Integer does not fit in 16 signed bits: " + value);
        }
    }
}

TEST(ParserTest, RejectsOutOfRangeUnsignedImmediatesWithLineNumber) {
    for (const std::string mnemonic : {"andi", "ori", "xori", "lui"}) {
        for (const std::string value : {"-1", "65536",
                                        "999999999999999999999999999"}) {
            const std::vector<std::string> tokens = mnemonic == "lui"
                ? std::vector<std::string>{mnemonic, "$t0", value}
                : std::vector<std::string>{mnemonic, "$t0", "$t1", value};
            SCOPED_TRACE(::testing::PrintToString(tokens));
            expectErrorMessage(
                [&] { parseInstruction(tokens, 131); },
                "Line 131: Integer does not fit in 16 unsigned bits: " + value);
        }
    }
}

TEST(ParserTest, PreservesSymbolicBranchAndJumpLabels) {
    for (const std::string mnemonic : {"beq", "bne", "j", "jal"}) {
        for (const std::string label : {"target", "Loop42", "_loop", ".L1",
                                        "$entry", "a.b$c_9"}) {
            const bool isBranch = mnemonic == "beq" || mnemonic == "bne";
            const std::vector<std::string> tokens = isBranch
                ? std::vector<std::string>{mnemonic, "$t0", "$t1", label}
                : std::vector<std::string>{mnemonic, label};
            SCOPED_TRACE(::testing::PrintToString(tokens));
            const Instruction inst = parseInstruction(tokens, 141);
            EXPECT_EQ(inst.label, label);
            EXPECT_EQ(inst.immediate, 0);
        }
    }
}

TEST(ParserTest, RejectsInvalidBranchAndJumpLabelsWithLineNumber) {
    for (const std::string mnemonic : {"beq", "bne", "j", "jal"}) {
        for (const std::string label : {"", "123", "-4", "0x10", "foo:",
                                        "foo-bar", "foo bar", "loop+4", "a/b"}) {
            const bool isBranch = mnemonic == "beq" || mnemonic == "bne";
            const std::vector<std::string> tokens = isBranch
                ? std::vector<std::string>{mnemonic, "$t0", "$t1", label}
                : std::vector<std::string>{mnemonic, label};
            SCOPED_TRACE(::testing::PrintToString(tokens));
            const std::string suffix = label.empty() ? "" : ": " + label;
            expectErrorMessage([&] { parseInstruction(tokens, 151); },
                               "Line 151: Invalid label" + suffix);
        }
    }
}

TEST(ParserTest, ParsesTokenizedMemorySyntaxForEveryMemoryInstruction) {
    for (const std::string mnemonic : {"lw", "sw", "lb", "lbu", "sb"}) {
        for (const std::string operand : {"-16($sp)", " -0x10 ( $sp ) "}) {
            const std::string line = "\t" + mnemonic + " $t0, " + operand +
                " # stack access";
            SCOPED_TRACE(line);
            const Instruction inst = parseInstruction(tokenize(line), 161);
            EXPECT_EQ(inst.mnemonic, mnemonic);
            EXPECT_EQ(inst.rt, 8);
            EXPECT_EQ(inst.rs, 29);
            EXPECT_EQ(inst.immediate, -16);
        }
    }
}

TEST(ParserTest, PreservesRegisterNumberBoundariesForEveryRegisterFormat) {
    struct Case {
        std::vector<std::string> tokens;
        int rs;
        int rt;
        int rd;
    };
    const std::vector<Case> cases = {
        {{"add", "$zero", "$ra", "$zero"}, 31, 0, 0},
        {{"add", "$ra", "$zero", "$ra"}, 0, 31, 31},
        {{"sll", "$zero", "$ra", "0"}, 0, 31, 0},
        {{"sll", "$ra", "$zero", "31"}, 0, 0, 31},
        {{"jr", "$zero"}, 0, 0, 0},
        {{"jr", "$ra"}, 31, 0, 0},
        {{"addi", "$zero", "$ra", "-32768"}, 31, 0, 0},
        {{"addi", "$ra", "$zero", "32767"}, 0, 31, 0},
        {{"lui", "$zero", "0"}, 0, 0, 0},
        {{"lui", "$ra", "65535"}, 0, 31, 0},
        {{"lw", "$zero", "0", "$ra"}, 31, 0, 0},
        {{"lw", "$ra", "0", "$zero"}, 0, 31, 0},
        {{"beq", "$zero", "$ra", "target"}, 0, 31, 0},
        {{"beq", "$ra", "$zero", "target"}, 31, 0, 0},
    };

    for (const auto& testCase : cases) {
        SCOPED_TRACE(::testing::PrintToString(testCase.tokens));
        const Instruction inst = parseInstruction(testCase.tokens);
        EXPECT_EQ(inst.rs, testCase.rs);
        EXPECT_EQ(inst.rt, testCase.rt);
        EXPECT_EQ(inst.rd, testCase.rd);
    }
}
