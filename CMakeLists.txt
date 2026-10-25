cmake_minimum_required(VERSION 3.16)

project(MIPS32_Emulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_COVERAGE "Instrument the assembler for gcov coverage (GCC)" OFF)

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
    error.cpp
)

target_include_directories(assembler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "ENABLE_COVERAGE requires GCC and its matching gcov")
    endif()
    target_compile_options(assembler PRIVATE --coverage -O0 -g)
    target_link_options(assembler PUBLIC --coverage)
endif()

add_executable(mips
    main.cpp
)

target_link_libraries(mips PRIVATE assembler)


include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(assembler_tests
    tests/source_reader_test.cpp
    tests/tokenizer_test.cpp
    tests/registers_test.cpp
    tests/parser_test.cpp
    tests/encoder_test.cpp
    tests/symbol_table_test.cpp
    tests/label_resolver_test.cpp
    tests/assembler_test.cpp
)

target_link_libraries(
    assembler_tests
    PRIVATE
    assembler
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(assembler_tests)
