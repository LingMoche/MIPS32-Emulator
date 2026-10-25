#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "assembler/source_reader.h"

namespace {

class TemporarySourceFile {
public:
    TemporarySourceFile() {
        static std::atomic<unsigned long long> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now()
                                   .time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            directory_ = std::filesystem::temp_directory_path() /
                ("mips32_source_reader_" + std::to_string(timestamp) + "_" +
                 std::to_string(sequence.fetch_add(1)));
            if (std::filesystem::create_directory(directory_)) {
                return;
            }
        }
        throw std::runtime_error("Could not create a unique test directory");
    }

    ~TemporarySourceFile() {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }

    TemporarySourceFile(const TemporarySourceFile&) = delete;
    TemporarySourceFile& operator=(const TemporarySourceFile&) = delete;

    std::string path() const {
        return (directory_ / "source.asm").string();
    }

    void write(const std::string& contents) const {
        std::ofstream file(path(), std::ios::binary);
        file << contents;
        file.close();
        if (!file) {
            throw std::runtime_error("Could not write source reader test input");
        }
    }

private:
    std::filesystem::path directory_;
};

} // namespace

TEST(SourceReaderTest, PreservesEmptyLines) {
    TemporarySourceFile file;
    file.write("main:\n\nadd $t0, $t1, $t2\n");

    const auto lines = readSourceFile(file.path());

    ASSERT_EQ(lines.size(), 3);

    EXPECT_EQ(lines[0].text, "main:");
    EXPECT_EQ(lines[0].lineNumber, 1);
    EXPECT_EQ(lines[1].text, "");
    EXPECT_EQ(lines[1].lineNumber, 2);
    EXPECT_EQ(lines[2].text, "add $t0, $t1, $t2");
    EXPECT_EQ(lines[2].lineNumber, 3);
}

TEST(SourceReaderTest, EmptyFileHasNoLines) {
    TemporarySourceFile file;
    file.write("");

    EXPECT_TRUE(readSourceFile(file.path()).empty());
}

TEST(SourceReaderTest, ReadsFinalLineWithOrWithoutTrailingNewline) {
    TemporarySourceFile file;
    for (const std::string ending : {"", "\n"}) {
        SCOPED_TRACE(::testing::PrintToString(ending));
        file.write("main:\njr $ra" + ending);

        const auto lines = readSourceFile(file.path());

        ASSERT_EQ(lines.size(), 2u);
        EXPECT_EQ(lines[0].text, "main:");
        EXPECT_EQ(lines[0].lineNumber, 1);
        EXPECT_EQ(lines[1].text, "jr $ra");
        EXPECT_EQ(lines[1].lineNumber, 2);
    }
}

TEST(SourceReaderTest, PreservesLeadingAndTrailingEmptyLines) {
    TemporarySourceFile file;
    file.write("\n\nmain:\n\n\n");

    const auto lines = readSourceFile(file.path());

    ASSERT_EQ(lines.size(), 5u);
    for (std::size_t index = 0; index < lines.size(); ++index) {
        EXPECT_EQ(lines[index].lineNumber, static_cast<int>(index + 1));
        EXPECT_EQ(lines[index].text, index == 2 ? "main:" : "");
    }
}

TEST(SourceReaderTest, PreservesWhitespaceAndCommentsVerbatim) {
    TemporarySourceFile file;
    const std::vector<std::string> expected = {
        "\t # header  ", "  main:  ", "\tadd $t0, $t1, $t2 # add  ", " \t ",
    };
    file.write(expected[0] + "\n" + expected[1] + "\n" + expected[2] +
               "\n" + expected[3]);

    const auto lines = readSourceFile(file.path());

    ASSERT_EQ(lines.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(lines[index].text, expected[index]);
        EXPECT_EQ(lines[index].lineNumber, static_cast<int>(index + 1));
    }
}

TEST(SourceReaderTest, ReadsLongSourceLinesWithoutTruncation) {
    TemporarySourceFile file;
    const std::string longComment = "# " + std::string(10000, 'x');
    file.write(longComment + "\njr $ra");

    const auto lines = readSourceFile(file.path());

    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0].text, longComment);
    EXPECT_EQ(lines[0].lineNumber, 1);
    EXPECT_EQ(lines[1].text, "jr $ra");
    EXPECT_EQ(lines[1].lineNumber, 2);
}

TEST(SourceReaderTest, MissingFileErrorIncludesRequestedPath) {
    TemporarySourceFile file;

    try {
        readSourceFile(file.path());
        FAIL() << "Expected a missing-file error";
    }
    catch (const std::runtime_error& exception) {
        EXPECT_EQ(std::string(exception.what()),
                  "Failed to open file: " + file.path());
    }
}
