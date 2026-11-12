# MIPS32 Emulator

The project provides a MIPS32 assembler that converts assembly source into
hexadecimal text or binary machine code, and a CPU library that executes the
46 instructions supported by the assembler. The CLI currently runs the assembler.

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
`j`, `jal`, `syscall`, `mult`, `multu`, `div`, `divu`, `mfhi`, `mflo`,
`mthi`, `mtlo`, `lh`, `lhu`, `sh`, `sllv`, `srlv`, and `srav`.
The all-zero instruction executes as `nop`.

This emulator uses a simplified control-flow convention with **no delay slots**:
taken branches and jumps change the PC immediately, and `jal` saves `PC + 4`
in `$ra`. This differs from classic MIPS32 delayed branches and its `PC + 8`
return address. Branch offsets are signed instruction counts relative to `PC + 4`;
absolute jumps combine the upper four bits of `PC + 4` with the shifted target.

Signed `add`, `addi`, and `sub` throw `std::overflow_error` on overflow;
unsigned arithmetic wraps modulo 2^32. Logical immediates are zero-extended;
arithmetic, comparison, and memory offsets are sign-extended (including `sltiu`).
Word accesses and register jump targets must be four-byte aligned; halfword
instructions require two-byte alignment. Memory is
little-endian; byte accesses may use any address. Unsupported instructions,
misalignment, and invalid memory accesses throw `std::runtime_error`.
Failed instructions leave the PC, general-purpose registers, HI/LO, and memory unchanged. Exceptions
are reported to the caller; architectural exception registers are not modeled.

### Multiply, divide and HI/LO

`mult rs, rt` multiplies signed 32-bit operands; `multu rs, rt` multiplies unsigned
operands. Both replace HI with the upper 32 bits and LO with the lower 32 bits of
the full 64-bit product, without a multiplication overflow error.

`div rs, rt` divides signed operands, rounding the quotient toward zero; a nonzero
remainder has the dividend's sign. `divu rs, rt` divides unsigned operands. Both
replace LO with the quotient and HI with the remainder, leaving general-purpose
registers unchanged. Only the two-register instruction form is accepted.

This emulator reports division by zero as `std::runtime_error`, and signed
`INT32_MIN / -1` as `std::overflow_error`, preserving all CPU state on either error.
This is an emulator diagnostic policy: the
[MIPS32 instruction reference](https://pages.hmc.edu/harris/class/e155/11/MIPS32InstructionSet.pdf)
specifies that integer division does not raise an architectural arithmetic exception.

`mfhi rd` / `mflo rd` copy HI / LO into a general-purpose register; writing `$zero`
discards the value. `mthi rs` / `mtlo rs` replace only the named special register.
HI and LO initialize to zero, are cleared by `reset()`, and can be inspected using
`getHI()`, `getLO()`, or `printState()`. Results are available to the next instruction;
instruction latency and pipeline hazards are not modeled.

```asm
addiu $t0, $zero, -7
addiu $t1, $zero, 3
mult $t0, $t1
mflo $s0                  # -21
mfhi $s1                  # 0xFFFFFFFF, upper half of the signed product
div $t0, $t1
mflo $s2                  # -2, quotient
mfhi $s3                  # -1, remainder
```

### Halfwords and variable shifts

`lh rt, offset(rs)` loads a little-endian 16-bit halfword with sign extension;
`lhu` zero-extends it. `sh rt, offset(rs)` stores the low 16 bits of `rt`, preserving
adjacent bytes. Offsets are signed 16-bit values; effective addresses use 32-bit
wrapping addition and must be even. Invalid halfword accesses throw before changing
CPU state or memory. The raw `Memory::readHalfWord` / `writeHalfWord` API checks the
complete two-byte range without enforcing instruction alignment, matching the word API.

`sllv rd, rt, rs`, `srlv rd, rt, rs`, and `srav rd, rt, rs` shift `rt` by the low
five bits of `rs` (0 through 31). The shift amount is a register, not an immediate.
`srav` fills vacated bits with the sign bit; `srlv` fills them with zeroes. These
instructions preserve HI/LO and support a destination that aliases either source.

## Syscall services

The assembler accepts `syscall` with no operands and encodes it as `0000000C`.
The CPU implements the following subset of the
[MARS/SPIM service convention](https://dpetersanderson.github.io/Help/SyscallHelp.html).
Put the service number in `$v0`, arguments in `$a0`/`$a1`, then execute `syscall`.
These are emulator services, not a Linux syscall ABI or architectural exception handler.

| `$v0` | Service | Arguments and result |
| --- | --- | --- |
| 1 | Print integer | Print signed 32-bit `$a0` in decimal, without a newline. |
| 4 | Print string | Print bytes starting at address `$a0`, stopping before NUL. |
| 5 | Read integer | Read one decimal integer line; return its 32-bit value in `$v0`. |
| 8 | Read string | `$a0` is a buffer address; signed `$a1` is its capacity including NUL. |
| 10 | Exit | Halt the CPU with exit code 0. |
| 11 | Print character | Print the low byte of `$a0`, without a newline. |
| 12 | Read character | Read one byte, including whitespace; return it zero-extended in `$v0`. |
| 17 | Exit with code | Halt the CPU with signed 32-bit exit code `$a0`. |

Service 5 consumes an entire line (or the final line before EOF), accepts surrounding
whitespace and an optional sign, and rejects trailing non-whitespace or values outside
the signed 32-bit range. Its decimal parsing is independent of input stream formatting.
Service 8 reads at most `capacity - 1` bytes, retaining a newline if one is encountered,
then writes a NUL. Excess input remains available for the next read. A capacity of 1
writes only NUL without reading; nonpositive capacities do nothing, ignoring the address.
For positive capacities the entire buffer must fit in one mapped memory region, and
it is validated before reading input. A partial string at EOF succeeds; EOF before
any input byte (services 5, 8 with capacity greater than 1, and 12) throws.
String services operate on bytes; they do not convert character encodings.

Unknown services, invalid string addresses, malformed input, and stream failures
throw `std::runtime_error`, preserving PC, registers, memory and halt state.
An unterminated print string is rejected before any output. Host input already consumed
and output already written cannot be rolled back after an error. Output services flush
their stream and preserve its formatting. Heap allocation, file I/O and floating-point
services are not implemented.

`CPU(memory)` uses `std::cin` and `std::cout`. Pass streams to redirect I/O; they must
outlive the CPU. A successful exit advances PC past the syscall, sets `isHalted()`,
and exposes the exit code through `getExitCode()`. Further `step()` calls do nothing.
`reset()` clears the halt state and exit code, preserving memory and stream state.
The host process remains running.

```cpp
std::istringstream input("42\n");           // Include <sstream>
std::ostringstream output;
CPU cpu(memory, input, output);            // Program already loaded into memory
for (unsigned steps = 0; !cpu.isHalted() && steps < 100000; ++steps) {
    cpu.step();
}
// Check cpu.isHalted() to distinguish exit from exhausting the step limit.
```

For example, this program prints `42` and exits (it uses no pseudo-instructions):

```asm
addiu $a0, $zero, 42
addiu $v0, $zero, 1
syscall
addiu $v0, $zero, 10
syscall
```

The CLI assembles this program; execution currently uses the CPU library API.

## Printing runtime state

Call these read-only methods whenever a runtime snapshot is needed:

```cpp
cpu.printState();                         // PC, HI/LO and all 32 registers to stdout
memory.printState();                      // All mapped regions, including stack
memory.printState(TEXT_BASE, 64);          // 64 bytes starting at TEXT_BASE

std::ostringstream snapshot;              // Include <sstream>
cpu.printState(snapshot);
memory.printState(TEXT_BASE, 16, snapshot);
```

CPU output includes PC and HI/LO, plus general-purpose register aliases and numeric indices, with eight-digit
uppercase hexadecimal values. Memory output includes the starting address and
byte count, followed by up to 16 hexadecimal bytes per addressed row in increasing
address order. The final row may be shorter; byte ranges need not be aligned.
Each mapped region has its own header; a requested range must fit within one
region. Invalid ranges throw `std::runtime_error` before any output is written.
A zero-byte range prints only its header and may start at the end of a region.
Printing preserves CPU state, memory contents, and the output stream's formatting.
The CLI remains an assembler; these methods are called from emulator code.

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

`Memory(baseAddress, size)` maps the requested program memory and a separate,
zero-initialized 1 MiB stack at `STACK_BASE = 0x7FF00000` through `0x7FFFFFFF`
(inclusive). `STACK_TOP` is the address of the highest four-byte stack word, so
`sw $t0, 0($sp)` is valid at the initial stack pointer. The gap between program
memory and the stack is unmapped and consumes no backing storage.

```cpp
Memory memory(TEXT_BASE, 64 * 1024);       // 64 KiB program memory + 1 MiB stack
CPU cpu(memory);                         // PC = TEXT_BASE, $sp = STACK_TOP

Memory smallStack(TEXT_BASE, 1024, 4096);  // Optional 4 KiB stack
Memory programOnly(TEXT_BASE, 1024, 0);    // Disable stack mapping
```

A custom stack size must be a multiple of four and ends at `0x80000000`
(exclusive). Overlapping regions and regions outside the 32-bit address space
are rejected. Accesses outside a mapped region throw `std::runtime_error`;
failed word stores leave memory unchanged. Changing `$sp` alone does not access
memory or enforce stack bounds; the subsequent load/store performs that check.
`CPU::reset()` restores `$sp` but preserves stack contents, as it does all memory.

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
