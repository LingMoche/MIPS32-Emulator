# MIPS32 Emulator

The project currently provides a MIPS32 assembler that converts assembly source
into hexadecimal text or binary machine code.

## Build and test

Requires CMake, Ninja, and a compiler with C++17 support. The first configuration
downloads GoogleTest.

```powershell
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure -j 4
```

## CLI usage

```powershell
# Print hex to the terminal by default
.\build\mips.exe program.asm

# Select the format from the output file extension (case-insensitive)
.\build\mips.exe program.asm -o program.hex
.\build\mips.exe program.asm -o program.bin

# -o can also precede the input file; quote paths containing spaces
.\build\mips.exe -o "output files\program.bin" "source files\program.asm"

.\build\mips.exe --help
```

Without `-o`, the CLI prints machine code to standard output and creates no output
file. With `-o`, it writes only to the specified file. The output directory must
already exist, and existing output files are overwritten. The input and output
must refer to different files.

Argument, assembly, and file I/O errors are reported on standard error with a
nonzero exit code. Successful commands return `0`.

## Output formats

Both `.hex` files and default terminal output use one instruction per line,
formatted as exactly eight uppercase hexadecimal digits without a `0x` prefix.
`.hex` files use LF line endings and contain no BOM. For example:

```text
20080005
2108FFFF
1500FFFE
```

`.bin` files always use **little-endian** byte order: four bytes per instruction,
least-significant byte first. They contain no header, addresses, instruction count,
or padding. The three instructions above produce these 12 bytes:

```text
05 00 08 20 FF FF 08 21 FE FF 00 15
```

Empty source produces an empty output file. Binary files contain only consecutive
instruction bytes; the emulator determines the load address.

The output API is declared in `assembler/output_writer.h`:

```cpp
void writeHexFile(const std::string& filename,
                  const std::vector<uint32_t>& machineCode);
void writeBinaryFile(const std::string& filename,
                     const std::vector<uint32_t>& machineCode);
```

Both functions throw `std::runtime_error` if the output file cannot be opened or
written.
