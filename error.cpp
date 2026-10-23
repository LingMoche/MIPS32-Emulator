#include "error.h"

#include <stdexcept>

void error(const std::string& word, const std::string& msg) {
    throw std::runtime_error(msg + ": " + word);
}