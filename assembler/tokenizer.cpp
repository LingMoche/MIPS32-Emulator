#include "tokenizer.h"

#include <sstream>
#include <algorithm>

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;

    const auto commentPos = line.find('#');
    std::string cleaned = commentPos == std::string::npos ? line : line.substr(0, commentPos);

    std::replace(cleaned.begin(), cleaned.end(), ',', ' ');
    std::replace(cleaned.begin(), cleaned.end(), '(', ' ');
    std::replace(cleaned.begin(), cleaned.end(), ')', ' ');

    std::stringstream ss(cleaned);
    std::string token;
    while (ss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}