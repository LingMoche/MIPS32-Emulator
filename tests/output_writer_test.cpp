#include <gtest/gtest.h>

#include "assembler/output_writer.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class TemporaryOutputDirectory {
public:
    TemporaryOutputDirectory() {
        static std::atomic<unsigned long long> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now()
                                   .time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            directory_ = std::filesystem::temp_directory_path() /
                ("mips32_output_writer_" + std::to_string(timestamp) + "_" +
                 std::to_string(sequence.fetch_add(1)));
            if (std::filesystem::create_directory(directory_)) {
                return;
            }
        }
        throw std::runtime_error("Could not create a unique test directory");
    }

    ~TemporaryOutputDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }

    TemporaryOutputDirectory(const TemporaryOutputDirectory&) = delete;
    TemporaryOutputDirectory& operator=(const TemporaryOutputDirectory&) = delete;

    std::string path(const std::string& filename) const {
        return (directory_ / filename).string();
    }

private:
    std::filesystem::path directory_;
};

std::string readBytes(const std::string& filename) {
    std::ifstream input(filename, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("Could not open output writer test result");
    }
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

using Writer = void (*)(const std::string&, const std::vector<uint32_t>&);

void expectOpenError(Writer writer, const std::string& filename) {
    try {
        writer(filename, {0x20080005u});
        FAIL() << "Expected an output-file error";
    }
    catch (const std::runtime_error& exception) {
        EXPECT_EQ(std::string(exception.what()),
                  "Failed to open output file: " + filename);
    }
}

} // namespace

TEST(OutputWriterTest, HexMatchesLoopInstructionsExactly) {
    TemporaryOutputDirectory directory;
    const std::string filename = directory.path("program.hex");

    writeHexFile(filename, {0x20080005u, 0x2108FFFFu, 0x1500FFFEu});

    EXPECT_EQ(readBytes(filename), "20080005\n2108FFFF\n1500FFFE\n");
}

TEST(OutputWriterTest, HexPadsEveryWordAndPreservesAllBits) {
    TemporaryOutputDirectory directory;
    const std::string filename = directory.path("program.hex");

    writeHexFile(filename, {0u, 1u, 0xABCDu, 0x80000000u, 0xFFFFFFFFu});

    EXPECT_EQ(readBytes(filename),
              "00000000\n00000001\n0000ABCD\n80000000\nFFFFFFFF\n");
}

TEST(OutputWriterTest, BinaryWritesLittleEndianBytesWithoutHeadersOrPadding) {
    TemporaryOutputDirectory directory;
    const std::string filename = directory.path("program.bin");

    writeBinaryFile(filename, {0x20080005u, 0x2108FFFFu, 0x1500FFFEu});

    const unsigned char expected[] = {
        0x05, 0x00, 0x08, 0x20,
        0xFF, 0xFF, 0x08, 0x21,
        0xFE, 0xFF, 0x00, 0x15,
    };
    EXPECT_EQ(readBytes(filename),
              std::string(reinterpret_cast<const char*>(expected), sizeof(expected)));
}

TEST(OutputWriterTest, BinaryPreservesZeroAndHighBitsInEveryByte) {
    TemporaryOutputDirectory directory;
    const std::string filename = directory.path("program.bin");

    writeBinaryFile(filename, {0u, 0x01020304u, 0x80000000u,
                               0xFF80FE81u, 0xFFFFFFFFu});

    const unsigned char expected[] = {
        0x00, 0x00, 0x00, 0x00,
        0x04, 0x03, 0x02, 0x01,
        0x00, 0x00, 0x00, 0x80,
        0x81, 0xFE, 0x80, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF,
    };
    EXPECT_EQ(readBytes(filename),
              std::string(reinterpret_cast<const char*>(expected), sizeof(expected)));
}

TEST(OutputWriterTest, EmptyMachineCodeCreatesEmptyFiles) {
    TemporaryOutputDirectory directory;
    for (Writer writer : {writeHexFile, writeBinaryFile}) {
        const std::string filename = directory.path(
            writer == writeHexFile ? "empty.hex" : "empty.bin");
        SCOPED_TRACE(filename);

        writer(filename, {});

        EXPECT_TRUE(std::filesystem::exists(filename));
        EXPECT_EQ(readBytes(filename), "");
    }
}

TEST(OutputWriterTest, HexOverwritesExistingLongerFilesAndTruncatesToEmpty) {
    TemporaryOutputDirectory directory;
    const std::string filename = directory.path("program.hex");
    writeHexFile(filename, {0xFFFFFFFFu, 0xFFFFFFFFu});

    writeHexFile(filename, {1u});
    EXPECT_EQ(readBytes(filename), "00000001\n");

    writeHexFile(filename, {});
    EXPECT_EQ(readBytes(filename), "");
}

TEST(OutputWriterTest, BinaryOverwritesExistingLongerFilesAndTruncatesToEmpty) {
    TemporaryOutputDirectory directory;
    const std::string filename = directory.path("program.bin");
    writeBinaryFile(filename, {0xFFFFFFFFu, 0xFFFFFFFFu});

    writeBinaryFile(filename, {0x01020304u});
    EXPECT_EQ(readBytes(filename), std::string("\x04\x03\x02\x01", 4));

    writeBinaryFile(filename, {});
    EXPECT_EQ(readBytes(filename), "");
}

TEST(OutputWriterTest, HexReportsMissingParentDirectory) {
    TemporaryOutputDirectory directory;

    expectOpenError(writeHexFile, directory.path("missing/program.hex"));
}

TEST(OutputWriterTest, BinaryReportsMissingParentDirectory) {
    TemporaryOutputDirectory directory;

    expectOpenError(writeBinaryFile, directory.path("missing/program.bin"));
}
