
#pragma once

#include <array>
#include <cstdint>
#include <iostream>

#include "emulator/memory.h"
#include "memory_layout.h"
#include "emulator/decoder.h"

class CPU {
public:
    explicit CPU(Memory& memory);

    // Initialize CPU state
    void reset();

    // Register access
    uint32_t getRegister(uint8_t index) const;
    void setRegister(uint8_t index, uint32_t value);

    // Program counter
    uint32_t getPC() const;

    // Print PC and all general-purpose registers without changing CPU state.
    void printState(std::ostream& output = std::cout) const;

    // Execute one instruction, without branch delay slots (jal links PC + 4).
    // Execution errors throw before committing changes to CPU state or memory.
    void step();

private:
    std::array<uint32_t, 32> registers_{};
    uint32_t pc_ = TEXT_BASE;

    void execute(
        const DecodedInstruction& inst,
        uint32_t& nextPC
    );

    Memory& memory_;
};
