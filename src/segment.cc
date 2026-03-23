#include "segment.h"

#include <algorithm>
#include <stdint.h>

namespace cut {

static int Utf8CharLen(uint8_t c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

void Segmenter::Build(const std::vector<std::string>& words) {
    std::vector<std::string> sorted = words;
    std::sort(sorted.begin(), sorted.end());
    da_.Build(sorted);
}

std::vector<std::string> Segmenter::Cut(const std::string& sentence) const {
    std::vector<std::string> result;
    int n = static_cast<int>(sentence.size());
    int i = 0;

    while (i < n) {
        auto matches = da_.PrefixSearch(sentence.substr(i));

        // Find longest match
        std::size_t best_len = 0;
        for (const auto& m : matches) {
            if (m.length > best_len) {
                best_len = m.length;
            }
        }

        if (best_len > 0) {
            result.push_back(sentence.substr(i, best_len));
            i += static_cast<int>(best_len);
        } else {
            // Fallback: single UTF-8 character
            int len = Utf8CharLen(static_cast<uint8_t>(sentence[i]));
            result.push_back(sentence.substr(i, len));
            i += len;
        }
    }

    return result;
}

} // namespace cut
