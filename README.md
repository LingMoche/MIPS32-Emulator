# MIPS32 Emulator

The project provides a MIPS32 assembler that converts assembly source into
hexadecimal text or binary machine code, and a CPU library that executes the
31 instructions supported by the assembler. The CLI currently runs the assembler.

## Build and test

Requires CMake, Ninja, and a compiler with C++17 support. The first configuration
downloads GoogleTest.

```powershell
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure -j 4
```

Assembler unit tests and CLI tests live in `tests/assembler/`. Run only this
group with `ctest --test-dir build -L assembler --output-on-failure`.
CPU, memory, and loader tests live in `tests/emulator/`. Run them with
`ctest --test-dir build -L emulator --output-on-failure`.
Assembler error handling lives in `assembler/error.h` and `assembler/error.cpp`.

## CPU execution

`CPU::step()` fetches and executes one instruction. Supported instructions are
`add`, `addu`, `sub`, `subu`, `and`, `or`, `xor`, `nor`, `slt`, `sltu`,
`sll`, `srl`, `sra`, `jr`, `addi`, `addiu`, `andi`, `ori`, `xori`,
`slti`, `sltiu`, `lui`, `lw`, `sw`, `lb`, `lbu`, `sb`, `beq`, `bne`,
`j`, and `jal`. The all-zero instruction executes as `nop`.

This emulator uses a simplified control-flow convention with **no delay slots**:
taken branches and jumps change the PC immediately, and `jal` saves `PC + 4`
in `$ra`. This differs from classic MIPS32 delayed branches and its `PC + 8`
return address. Branch offsets are signed instruction counts relative to `PC + 4`;
absolute jumps combine the upper four bits of `PC + 4` with the shifted target.

Signed `add`, `addi`, and `sub` throw `std::overflow_error` on overflow;
unsigned arithmetic wraps modulo 2^32. Logical immediates are zero-extended;
arithmetic, comparison, and memory offsets are sign-extended (including `sltiu`).
Word accesses and register jump targets must be four-byte aligned. Memory is
little-endian; byte accesses may use any address. Unsupported instructions,
misalignment, and invalid memory accesses throw `std::runtime_error`.
Failed instructions leave the PC, registers, and memory unchanged. Exceptions
are reported to the caller; architectural exception registers are not modeled.

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
void writeHexFile(
    const std::string& filename,
    const std::vector<uint32_t>& machineCode
);

void writeBinaryFile(
    const std::string& filename,
    const std::vector<uint32_t>& machineCode
);
```

Both functions throw `std::runtime_error` if the output file cannot be opened or written.

## Runtime memory layout

The current binary format is a **flat binary format**. It contains only instruction bytes and does not store section information, virtual addresses, an entry point, stack information, or other executable metadata.

The assembler and emulator therefore use a shared memory-layout convention.

```text
TEXT_BASE = 0x00400000
STACK_TOP = 0x7FFFFFFC
```

### Text segment

All instructions contained in a `.bin` file are loaded consecutively starting at:

```text
0x00400000
```

For example, a binary containing three instructions is mapped as:

```text
0x00400000    instruction 0
0x00400004    instruction 1
0x00400008    instruction 2
```

The initial program counter is:

```text
PC = TEXT_BASE
```

or:

```text
PC = 0x00400000
```

Each MIPS32 instruction occupies four bytes, so instruction addresses increase by four.

The assembler uses the same `TEXT_BASE` when constructing the symbol table and resolving labels. This ensures that addresses generated by the assembler agree with addresses used by the emulator.

### Stack

The stack is not stored in the `.bin` file.

The emulator creates the stack as part of the runtime environment and initializes the stack pointer to:

```text
$sp = STACK_TOP
$sp = 0x7FFFFFFC
```

The stack grows toward lower addresses.

### Data segment

The current flat binary format does not encode a `.data` section or its load address.

At the current stage of the project, binary output should therefore be treated as containing only the program's text/instruction bytes.

Support for initialized data may later introduce a separate data segment, for example:

```text
DATA_BASE = 0x10010000
```

A future executable format may store section sizes, load addresses, and an entry point so that `.text` and `.data` can be loaded independently.

### Binary loading

The emulator interprets a flat binary as:

```text
binary byte 0    -> memory[TEXT_BASE]
binary byte 1    -> memory[TEXT_BASE + 1]
binary byte 2    -> memory[TEXT_BASE + 2]
...
```

Because `.bin` files are little-endian, the bytes:

```text
05 00 08 20
```

loaded at:

```text
0x00400000
```

represent the 32-bit instruction:

```text
0x20080005
```

when fetched as a little-endian word.

These layout rules are conventions of this project rather than requirements imposed by the MIPS32 ISA itself.
