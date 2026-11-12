
#pragma once

#include <array>
#include <cstdint>
#include <iostream>

#include "emulator/memory.h"
#include "memory_layout.h"
#include "emulator/decoder.h"

class CPU {
public:
    // Streams must outlive this CPU. Defaults provide console input/output.
    explicit CPU(Memory& memory, std::istream& input = std::cin,
                 std::ostream& output = std::cout);

    // Initialize CPU state
    void reset();

    // Register access
    uint32_t getRegister(uint8_t index) const;
    void setRegister(uint8_t index, uint32_t value);

    // Program counter
    uint32_t getPC() const;

    bool isHalted() const;
    int32_t getExitCode() const;

    // Print PC and all general-purpose registers without changing CPU state.
    void printState(std::ostream& output = std::cout) const;

    // Execute one instruction, without branch delay slots (jal links PC + 4).
    // A halted CPU does nothing. Execution errors preserve CPU state and memory;
    // syscall input/output already performed on host streams cannot be undone.
    void step();

private:
    std::array<uint32_t, 32> registers_{};
    uint32_t pc_ = TEXT_BASE;
    bool halted_ = false;
    int32_t exitCode_ = 0;

    void executeSyscall();

    void execute(
        const DecodedInstruction& inst,
        uint32_t& nextPC
    );

    Memory& memory_;
    std::istream& input_;
    std::ostream& output_;
};
