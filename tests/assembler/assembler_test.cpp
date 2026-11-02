#include <gtest/gtest.h>

#include "assembler/assembler.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace {

class TemporaryAssemblyFile {
public:
    TemporaryAssemblyFile() {
        static std::atomic<unsigned long long> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now()
                                   .time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            directory_ = std::filesystem::temp_directory_path() /
                ("mips32_assembler_" + std::to_string(timestamp) + "_" +
                 std::to_string(sequence.fetch_add(1)));
            if (std::filesystem::create_directory(directory_)) {
                return;
            }
        }
        throw std::runtime_error("Could not create a unique test directory");
    }

    ~TemporaryAssemblyFile() {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }

    TemporaryAssemblyFile(const TemporaryAssemblyFile&) = delete;
    TemporaryAssemblyFile& operator=(const TemporaryAssemblyFile&) = delete;

    std::string path() const {
        return (directory_ / "program.asm").string();
    }

    void write(const std::string& contents) const {
        std::ofstream file(path(), std::ios::binary);
        file << contents;
        file.close();
        if (!file) {
            throw std::runtime_error("Could not write assembly test input");
        }
    }

private:
    std::filesystem::path directory_;
};

template <typename Action>
void expectAssemblyError(Action action, const std::string& expected) {
    try {
        action();
        FAIL() << "Expected std::runtime_error: " << expected;
    }
    catch (const std::runtime_error& exception) {
        EXPECT_EQ(std::string(exception.what()), expected);
    }
}

} // namespace

TEST(AssemblerTest, AssemblesSimpleLoop) {
    std::vector<SourceLine> lines = {
        {1, "main:"},
        {2, "addi $t0, $zero, 5"},
        {3, ""},
        {4, "loop:"},
        {5, "addi $t0, $t0, -1"},
        {6, "bne $t0, $zero, loop"}
    };

    auto result = assemble(lines);

    ASSERT_EQ(result.size(), 3);

    EXPECT_EQ(result[0], 0x20080005u);
    EXPECT_EQ(result[1], 0x2108FFFFu);
    EXPECT_EQ(result[2], 0x1500FFFEu);
}

TEST(AssemblerTest, EncodesEveryRegisterArithmeticInstruction) {
    const std::vector<SourceLine> lines = {
        {1, "add $t0, $t1, $t2"},
        {2, "addu $t0, $t1, $t2"},
        {3, "sub $t0, $t1, $t2"},
        {4, "subu $t0, $t1, $t2"},
        {5, "and $t0, $t1, $t2"},
        {6, "or $t0, $t1, $t2"},
        {7, "xor $t0, $t1, $t2"},
        {8, "nor $t0, $t1, $t2"},
        {9, "slt $t0, $t1, $t2"},
        {10, "sltu $t0, $t1, $t2"},
    };
    const std::vector<uint32_t> expected = {
        0x012A4020u, 0x012A4021u, 0x012A4022u, 0x012A4023u,
        0x012A4024u, 0x012A4025u, 0x012A4026u, 0x012A4027u,
        0x012A402Au, 0x012A402Bu,
    };

    EXPECT_EQ(assemble(lines), expected);
}

TEST(AssemblerTest, EncodesShiftsAndRegisterJump) {
    const std::vector<SourceLine> lines = {
        {1, "sll $t0, $t1, 0"},
        {2, "srl $t0, $t1, 31"},
        {3, "sra $s0, $s1, 4"},
        {4, "jr $ra"},
    };
    const std::vector<uint32_t> expected = {
        0x00094000u, 0x000947C2u, 0x00118103u, 0x03E00008u,
    };

    EXPECT_EQ(assemble(lines), expected);
}

TEST(AssemblerTest, EncodesImmediateInstructionsAtSignedAndUnsignedBoundaries) {
    const std::vector<SourceLine> lines = {
        {1, "addi $t0, $t1, -32768"},
        {2, "addiu $t0, $t1, 32767"},
        {3, "andi $t0, $t1, 65535"},
        {4, "ori $t0, $t1, 0x8000"},
        {5, "xori $t0, $t1, 0x1234"},
        {6, "slti $t0, $t1, -1"},
        {7, "sltiu $t0, $t1, -32768"},
        {8, "lui $t0, 0xabcd"},
    };
    const std::vector<uint32_t> expected = {
        0x21288000u, 0x25287FFFu, 0x3128FFFFu, 0x35288000u,
        0x39281234u, 0x2928FFFFu, 0x2D288000u, 0x3C08ABCDu,
    };

    EXPECT_EQ(assemble(lines), expected);
}

TEST(AssemblerTest, EncodesEveryLoadAndStoreWithMemorySyntax) {
    const std::vector<SourceLine> lines = {
        {1, "lw $t0, -4($sp)"},
        {2, "sw $t0, 32767($sp)"},
        {3, "lb $t0, -32768($sp)"},
        {4, "lbu $t0, 0($sp)"},
        {5, "sb $t0, 16($sp)"},
    };
    const std::vector<uint32_t> expected = {
        0x8FA8FFFCu, 0xAFA87FFFu, 0x83A88000u, 0x93A80000u,
        0xA3A80010u,
    };

    EXPECT_EQ(assemble(lines), expected);
}

TEST(AssemblerTest, EmptyAndNonInstructionSourcesProduceNoMachineCode) {
    EXPECT_TRUE(assemble({}).empty());
    EXPECT_TRUE(assemble({{1, ""}, {2, " \t "}, {3, "# comment"},
                          {4, "entry: # label"}, {5, "alias:"}}).empty());
}

TEST(AssemblerTest, CommentsBlankLinesAndConsecutiveLabelsDoNotAdvancePc) {
    const std::vector<SourceLine> lines = {
        {1, "# header"},
        {2, "start:"},
        {3, "alias: # same address as start"},
        {4, " \t "},
        {5, "beq $zero, $zero, finish # forward branch"},
        {6, "addi $t0, $zero, 1"},
        {7, "# another comment"},
        {8, "bne $t0, $zero, alias"},
        {9, ""},
        {10, "finish:"},
        {11, "jr $ra"},
        {12, "# trailing comment"},
    };
    const std::vector<uint32_t> expected = {
        0x10000002u, 0x20080001u, 0x1500FFFDu, 0x03E00008u,
    };

    EXPECT_EQ(assemble(lines), expected);
}

TEST(AssemblerTest, EncodesBranchesToNextInstructionAndSelf) {
    const std::vector<SourceLine> lines = {
        {1, "beq $t0, $t1, next"},
        {2, "next:"},
        {3, "bne $t0, $t1, next"},
    };
    const std::vector<uint32_t> expected = {0x11090000u, 0x1509FFFFu};

    EXPECT_EQ(assemble(lines), expected);
}

TEST(AssemblerTest, EncodesForwardJumpAndBackwardJumpAndLinkFromTextBase) {
    const std::vector<SourceLine> lines = {
        {1, "entry:"},
        {2, "j forward"},
        {3, "jal entry"},
        {4, "forward:"},
        {5, "jr $ra"},
    };
    const std::vector<uint32_t> expected = {
        0x08100002u, 0x0C100000u, 0x03E00008u,
    };

    EXPECT_EQ(assemble(lines), expected);
}

TEST(AssemblerTest, IndependentAssemblyCallsDoNotReuseSymbolsOrPc) {
    EXPECT_EQ(assemble({{1, "entry:"}, {2, "j entry"}}),
              (std::vector<uint32_t>{0x08100000u}));
    EXPECT_EQ(assemble({{1, "entry:"}, {2, "jal entry"}}),
              (std::vector<uint32_t>{0x0C100000u}));
    expectAssemblyError([] { assemble({{41, "j entry"}}); },
                        "Line 41: Undefined label: entry");
}

TEST(AssemblerTest, ReportsOriginalSourceLineForInstructionErrors) {
    struct Case {
        std::string instruction;
        std::string message;
    };
    const std::vector<Case> cases = {
        {"unknown $t0", "Invalid instruction: unknown"},
        {"add $t0, $t1", "Expect 3 operands: add"},
        {"jr $bad", "Invalid register name: $bad"},
        {"addi $t0, $zero, 32768",
         "Integer does not fit in 16 signed bits: 32768"},
        {"sll $t0, $t1, nope", "Invalid integer: nope"},
        {"j absent", "Undefined label: absent"},
        {"beq $t0, $t1, absent", "Undefined label: absent"},
        {"j bad-name", "Invalid label: bad-name"},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.instruction);
        expectAssemblyError(
            [&] {
                assemble({{2, "# ignored"}, {8, "entry:"},
                          {16, ""}, {37, testCase.instruction}});
            },
            "Line 37: " + testCase.message);
    }
}

TEST(AssemblerTest, ReportsOriginalSourceLineForSymbolErrors) {
    expectAssemblyError(
        [] { assemble({{12, "entry:"}, {28, "jr $ra"}, {91, "entry:"}}); },
        "Line 91: Duplicate label: entry");
    expectAssemblyError([] { assemble({{73, "123bad:"}}); },
                        "Line 73: Invalid label: 123bad");
}

TEST(AssemblerTest, AssemblesFileWithCommentsCrLfAndNoFinalNewline) {
    TemporaryAssemblyFile file;
    file.write("# header\r\nmain:\r\n\tlui $t0, 0x1234 # high bits\r\n"
               "ori $t0, $t0, 0x5678\r\nsw $t0, -4($sp)\r\n"
               "beq $t0, $zero, done\r\njal main\r\ndone:\r\njr $ra");
    const std::vector<uint32_t> expected = {
        0x3C081234u, 0x35085678u, 0xAFA8FFFCu,
        0x11000001u, 0x0C100000u, 0x03E00008u,
    };

    EXPECT_EQ(assembleFile(file.path()), expected);
}

TEST(AssemblerTest, AssemblesEmptyFileAndFileWithoutInstructions) {
    TemporaryAssemblyFile file;
    file.write("");
    EXPECT_TRUE(assembleFile(file.path()).empty());
    file.write("# header\n\nentry:\n");
    EXPECT_TRUE(assembleFile(file.path()).empty());
}

TEST(AssemblerTest, FileErrorsRetainPhysicalLineNumbers) {
    TemporaryAssemblyFile file;
    file.write("# header\n\nentry:\naddi $t0, $zero, 1\nj missing\n");

    expectAssemblyError([&] { assembleFile(file.path()); },
                        "Line 5: Undefined label: missing");
}

TEST(AssemblerTest, ReportsMissingInputFile) {
    TemporaryAssemblyFile file;

    expectAssemblyError([&] { assembleFile(file.path()); },
                        "Failed to open file: " + file.path());
}
