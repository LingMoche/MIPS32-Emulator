#include "label.h"

#include "../error.h"

#include <algorithm>

namespace {

bool isLabelStart(char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') ||
           character == '_' || character == '.' || character == '$';
}

} // namespace

void validateLabelName(const std::string& label, int lineNumber) {
    if (label.empty() || !isLabelStart(label.front()) ||
        !std::all_of(label.begin(), label.end(), [](char character) {
            return isLabelStart(character) ||
                   (character >= '0' && character <= '9');
        })) {
        error(label, "Invalid label", lineNumber);
    }
}
