#pragma once

#include <cstdint>

inline constexpr uint32_t TEXT_BASE = 0x00400000;
inline constexpr uint32_t STACK_TOP = 0x7FFFFFFC;
// STACK_TOP is the address of the highest stack word (four usable bytes).
inline constexpr uint32_t STACK_SIZE = 1024 * 1024;
inline constexpr uint32_t STACK_BASE = STACK_TOP + 4 - STACK_SIZE;
