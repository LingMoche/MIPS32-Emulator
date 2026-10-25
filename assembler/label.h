#pragma once

#include <string>

// Label names start with an ASCII letter, '_', '.', or '$'. Subsequent
// characters may also be digits. The name must not include a trailing ':'.
// Invalid names are reported through error() with the source line number.
void validateLabelName(const std::string& label, int lineNumber = 0);
