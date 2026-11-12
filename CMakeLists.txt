
cmake_minimum_required(VERSION 3.16)

project(MIPS32_Emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(
    ENABLE_COVERAGE
    "Instrument the assembler for gcov coverage (GCC)"
    OFF
)

# ============================================================
# Assembler Library
# ============================================================

add_library(assembler
    assembler/source_reader.cpp
    assembler/tokenizer.cpp
    assembler/registers.cpp
    assembler/instruction_table.cpp
    assembler/label.cpp
    assembler/parser.cpp
    assembler/encoder.cpp
    assembler/symbol_table.cpp
    assembler/label_resolver.cpp
    assembler/assembler.cpp
    assembler/output_writer.cpp
    assembler/error.cpp
)

target_include_directories(assembler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================
# Emulator Library
# ============================================================

add_library(emulator
    emulator/memory.cpp
    emulator/loader.cpp
    emulator/cpu.cpp
    emulator/decoder.cpp
)

target_include_directories(emulator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================================
# Coverage
# ============================================================

if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR
            "ENABLE_COVERAGE requires GCC and its matching gcov"
        )
    endif()

    target_compile_options(assembler PRIVATE
        --coverage
        -O0
        -g
    )

    target_link_options(assembler PUBLIC
        --coverage
    )
endif()

# ============================================================
# Main Executable
# ============================================================

add_executable(mips
    main.cpp
)

target_link_libraries(mips PRIVATE
    assembler
)

# ============================================================
# GoogleTest
# ============================================================

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(googletest)

enable_testing()

include(GoogleTest)

# ============================================================
# Assembler Unit & Integration Tests
# ============================================================

add_executable(assembler_tests
    tests/assembler/source_reader_test.cpp
    tests/assembler/tokenizer_test.cpp
    tests/assembler/registers_test.cpp
    tests/assembler/parser_test.cpp
    tests/assembler/encoder_test.cpp
    tests/assembler/symbol_table_test.cpp
    tests/assembler/label_resolver_test.cpp
    tests/assembler/assembler_test.cpp
    tests/assembler/output_writer_test.cpp
)

target_link_libraries(assembler_tests PRIVATE
    assembler
    GTest::gtest_main
)

gtest_discover_tests(assembler_tests
    PROPERTIES LABELS assembler
)

# ============================================================
# Emulator Unit & Integration Tests
# ============================================================

add_executable(emulator_tests
    tests/emulator/memory_test.cpp
    tests/emulator/loader_test.cpp
    tests/emulator/cpu_test.cpp
    tests/emulator/extended_instructions_test.cpp
    tests/emulator/syscall_test.cpp
)

target_link_libraries(emulator_tests PRIVATE
    emulator
    assembler
    GTest::gtest_main
)

gtest_discover_tests(emulator_tests
    PROPERTIES LABELS emulator
)

# ============================================================
# CLI Integration Tests
# ============================================================

add_test(
    NAME cli_tests
    COMMAND ${CMAKE_COMMAND}
        "-DMIPS_EXECUTABLE=$<TARGET_FILE:mips>"
        "-DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/cli_test"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/assembler/cli_test.cmake"
)

set_tests_properties(cli_tests
    PROPERTIES LABELS assembler
)
