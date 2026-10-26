#include "assembler/assembler.h"
#include "assembler/output_writer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

struct Options {
    std::string input;
    std::string output;
    bool help = false;
};

void printUsage() {
    std::cout << "Usage: mips <source.asm> [-o <output.hex|output.bin>]\n"
                 "       mips --help\n\n"
                 "Without -o, print uppercase 8-digit hex instructions to stdout.\n"
                 ".hex: one instruction per line, without a 0x prefix.\n"
                 ".bin: four bytes per instruction in little-endian order.\n";
}

Options parseArguments(int argc, char* argv[]) {
    Options options;
    bool positionalOnly = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (!positionalOnly && argument == "--") {
            positionalOnly = true;
        }
        else if (!positionalOnly && (argument == "-h" || argument == "--help")) {
            options.help = true;
        }
        else if (!positionalOnly && argument == "-o") {
            if (!options.output.empty()) {
                throw std::runtime_error("Output file specified more than once");
            }
            if (index + 1 == argc || std::string(argv[index + 1]).empty()
                || argv[index + 1][0] == '-') {
                throw std::runtime_error("Missing output filename after -o");
            }
            options.output = argv[++index];
        }
        else if (!positionalOnly && !argument.empty() && argument[0] == '-') {
            throw std::runtime_error("Unknown option: " + argument);
        }
        else {
            if (argument.empty()) {
                throw std::runtime_error("Input filename cannot be empty");
            }
            if (!options.input.empty()) {
                throw std::runtime_error("Expected exactly one input file");
            }
            options.input = argument;
        }
    }
    if (!options.help && options.input.empty()) {
        throw std::runtime_error("Missing input file. Run mips --help for usage.");
    }
    return options;
}

std::filesystem::path argumentPath(const std::string& filename) {
#ifdef _WIN32
    // Match the Windows code page used by main's argv and narrow file streams.
    const auto length = static_cast<int>(filename.size());
    const int wideLength = MultiByteToWideChar(
        CP_ACP, MB_ERR_INVALID_CHARS, filename.data(), length, nullptr, 0);
    if (wideLength == 0) {
        throw std::runtime_error("Invalid filename: " + filename);
    }
    std::wstring wideFilename(wideLength, L'\0');
    if (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, filename.data(), length,
                            wideFilename.data(), wideLength) != wideLength) {
        throw std::runtime_error("Invalid filename: " + filename);
    }
    return std::filesystem::path(wideFilename);
#else
    return std::filesystem::path(filename);
#endif
}

std::string outputExtension(const Options& options) {
    if (options.output.empty()) {
        return {};
    }
    const auto outputPath = argumentPath(options.output);
    auto extension = outputPath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return std::tolower(character); });
    if (extension != ".hex" && extension != ".bin") {
        throw std::runtime_error("Output filename must end in .hex or .bin: "
                                 + options.output);
    }
    std::error_code ignored;
    if (std::filesystem::equivalent(argumentPath(options.input), outputPath, ignored)) {
        throw std::runtime_error("Output file must differ from input file: "
                                 + options.output);
    }
    return extension;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parseArguments(argc, argv);
        if (options.help) {
            printUsage();
            return 0;
        }

        const auto extension = outputExtension(options);
        const auto machineCode = assembleFile(options.input);
        if (extension == ".hex") {
            writeHexFile(options.output, machineCode);
        }
        else if (extension == ".bin") {
            writeBinaryFile(options.output, machineCode);
        }
        else {
            for (uint32_t instruction : machineCode) {
                std::cout << std::hex << std::uppercase << std::setw(8)
                          << std::setfill('0') << instruction << '\n';
            }
            std::cout.flush();
            if (!std::cout) {
                throw std::runtime_error("Failed to write to stdout");
            }
        }
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << "Error: " << exception.what() << '\n';
        return 1;
    }
}
