if(NOT DEFINED MIPS_EXECUTABLE OR NOT EXISTS "${MIPS_EXECUTABLE}")
    message(FATAL_ERROR "MIPS_EXECUTABLE must name the built CLI executable")
endif()
if(NOT DEFINED TEST_DIRECTORY)
    message(FATAL_ERROR "TEST_DIRECTORY must name a directory for CLI fixtures")
endif()

file(MAKE_DIRECTORY "${TEST_DIRECTORY}")
set(source "${TEST_DIRECTORY}/program with spaces.asm")
set(source_text [=[addi $t0, $zero, 5
loop:
addi $t0, $t0, -1
bne $t0, $zero, loop
sll $zero, $zero, 0
]=])
file(WRITE "${source}" "${source_text}")
set(expected_hex "20080005\n2108FFFF\n1500FFFE\n00000000\n")
# Each instruction is stored least-significant byte first (little-endian).
set(expected_binary "05000820ffff0821feff001500000000")

function(expect_success name expected_stdout)
    execute_process(
        COMMAND "${MIPS_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 10
    )
    string(REPLACE "\r\n" "\n" stdout "${stdout}")
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "${name}: CLI failed (${result}): ${stderr}")
    endif()
    if(NOT "${stdout}" STREQUAL "${expected_stdout}")
        message(FATAL_ERROR "${name}: unexpected stdout: [${stdout}]")
    endif()
    if(NOT "${stderr}" STREQUAL "")
        message(FATAL_ERROR "${name}: unexpected stderr: ${stderr}")
    endif()
endfunction()

function(expect_failure name)
    execute_process(
        COMMAND "${MIPS_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 10
    )
    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "${name}: CLI unexpectedly succeeded")
    endif()
    if(NOT "${stdout}" STREQUAL "")
        message(FATAL_ERROR "${name}: failure wrote to stdout: [${stdout}]")
    endif()
    if("${stderr}" STREQUAL "")
        message(FATAL_ERROR "${name}: failure did not explain the error on stderr")
    endif()
endfunction()

function(expect_hex_file path)
    file(READ "${path}" actual)
    string(REPLACE "\r\n" "\n" actual "${actual}")
    if(NOT "${actual}" STREQUAL "${expected_hex}")
        message(FATAL_ERROR "Unexpected hex contents in ${path}: [${actual}]")
    endif()
endfunction()

function(expect_binary_file path)
    file(READ "${path}" actual HEX)
    if(NOT "${actual}" STREQUAL "${expected_binary}")
        message(FATAL_ERROR "Unexpected binary bytes in ${path}: ${actual}")
    endif()
endfunction()

expect_success("default hex stdout" "${expected_hex}" "${source}")

set(hex_output "${TEST_DIRECTORY}/output with spaces.hex")
expect_success("hex file" "" "${source}" -o "${hex_output}")
expect_hex_file("${hex_output}")

set(binary_output "${TEST_DIRECTORY}/output with spaces.bin")
expect_success("binary file and preceding output option" ""
    -o "${binary_output}" "${source}")
expect_binary_file("${binary_output}")

set(mixed_hex_output "${TEST_DIRECTORY}/mixed.HeX")
expect_success("case insensitive hex suffix" "" "${source}" -o "${mixed_hex_output}")
expect_hex_file("${mixed_hex_output}")
set(mixed_binary_output "${TEST_DIRECTORY}/mixed.BiN")
expect_success("case insensitive binary suffix" "" "${source}" -o "${mixed_binary_output}")
expect_binary_file("${mixed_binary_output}")

foreach(help_option IN ITEMS -h --help)
    execute_process(
        COMMAND "${MIPS_EXECUTABLE}" "${help_option}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
        TIMEOUT 10
    )
    string(TOLOWER "${stdout}" help_text)
    if(NOT "${result}" STREQUAL "0" OR NOT "${stderr}" STREQUAL ""
            OR NOT help_text MATCHES "usage")
        message(FATAL_ERROR "${help_option}: expected successful usage output; got ${result}: ${stdout}${stderr}")
    endif()
endforeach()

expect_failure("no arguments")
expect_failure("missing output value" "${source}" -o)
expect_failure("missing source argument" -o "${binary_output}")
expect_failure("duplicate output option" "${source}"
    -o "${hex_output}" -o "${binary_output}")
expect_failure("multiple sources" "${source}" "${source}")
expect_failure("unknown option" "${source}" --unknown)
expect_failure("unknown output extension" "${source}" -o "${TEST_DIRECTORY}/output.txt")
expect_failure("missing input file" "${TEST_DIRECTORY}/does not exist.asm")

# Failed assembly must leave an existing destination intact.
set(invalid_source "${TEST_DIRECTORY}/invalid.asm")
set(protected_output "${TEST_DIRECTORY}/protected.bin")
file(WRITE "${invalid_source}" "not_an_instruction $t0, $zero, 5\n")
file(WRITE "${protected_output}" "preserve this output")
expect_failure("assembly error" "${invalid_source}" -o "${protected_output}")
file(READ "${protected_output}" protected_contents)
if(NOT protected_contents STREQUAL "preserve this output")
    message(FATAL_ERROR "Assembly failure modified an existing output file")
endif()

# A directory is a deterministic write failure, even when running with broad permissions.
set(output_directory "${TEST_DIRECTORY}/directory.bin")
file(MAKE_DIRECTORY "${output_directory}")
expect_failure("output write error" "${source}" -o "${output_directory}")

# A valid output suffix ensures this exercises source protection, not suffix rejection.
set(same_path_source "${TEST_DIRECTORY}/source.hex")
file(WRITE "${same_path_source}" "${source_text}")
expect_failure("output equals input" "${same_path_source}" -o "${same_path_source}")
file(READ "${same_path_source}" same_path_contents)
if(NOT "${same_path_contents}" STREQUAL "${source_text}")
    message(FATAL_ERROR "Using the input as output modified the source file")
endif()

set(linked_output "${TEST_DIRECTORY}/linked source.bin")
file(REMOVE "${linked_output}")
file(CREATE_LINK "${source}" "${linked_output}" RESULT link_result)
if("${link_result}" STREQUAL "0")
    expect_failure("output hard links input" "${source}" -o "${linked_output}")
    file(READ "${source}" source_after_link_test)
    if(NOT "${source_after_link_test}" STREQUAL "${source_text}")
        message(FATAL_ERROR "Using a hard-linked output modified the source file")
    endif()
else()
    message(STATUS "Hard-link protection test skipped: ${link_result}")
endif()
