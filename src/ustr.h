#pragma once

#include <stdint.h>
#include <string>

namespace ustr {

// Returns the byte length of a UTF-8 character given its leading byte.
inline int CharLen(uint8_t c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// Returns true if the string is valid UTF-8 and not empty/whitespace-only.
bool IsValidWord(const std::string& s);

} // namespace ustr
