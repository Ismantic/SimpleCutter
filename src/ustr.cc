#include "ustr.h"

namespace ustr {

bool IsValidWord(const std::string& s) {
    if (s.empty()) return false;
    // Check valid UTF-8
    size_t i = 0;
    while (i < s.size()) {
        uint8_t c = static_cast<uint8_t>(s[i]);
        int len = CharLen(c);
        if ((c & 0x80) != 0) {
            // Multi-byte: verify leading byte is valid
            if (len == 1) return false;  // invalid leading byte
            if (i + len > s.size()) return false;
            for (int j = 1; j < len; ++j) {
                if ((static_cast<uint8_t>(s[i + j]) & 0xC0) != 0x80) return false;
            }
        }
        i += len;
    }
    // Reject whitespace-only
    for (char c : s) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return true;
    }
    return false;
}

} // namespace ustr
