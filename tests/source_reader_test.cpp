#include <gtest/gtest.h>
#include <fstream>

#include "assembler/source_reader.h"

TEST(SourceReaderTest, PreservesEmptyLines) {
    std::ofstream file("temporary_test.asm");

    file << "main:\n";
    file << "\n";
    file << "add $t0, $t1, $t2\n";

    file.close();

    auto lines = readSourceFile("temporary_test.asm");

    ASSERT_EQ(lines.size(), 3);

    EXPECT_EQ(lines[0].text, "main:");
    EXPECT_EQ(lines[1].text, "");
    EXPECT_EQ(lines[2].text, "add $t0, $t1, $t2");

    std::remove("temporary_test.asm");
}