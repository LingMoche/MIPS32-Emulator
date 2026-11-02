#pragma once

#include <string>

// Omit lineNumber (or use 0) when no source line is available.
[[noreturn]] void error(const std::string& word, const std::string& msg,
                       int lineNumber = 0);
