#include "error.h"

#include <stdexcept>

[[noreturn]] void error(const std::string& word, const std::string& msg,
                       int lineNumber) {
    std::string message = msg;
    if (!word.empty()) {
        message += ": " + word;
    }
    if (lineNumber > 0) {
        message = "Line " + std::to_string(lineNumber) + ": " + message;
    }
    throw std::runtime_error(message);
}
