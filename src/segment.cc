#include "segment.h"
#include "ustr.h"

#include <algorithm>

namespace cut {

void Segmenter::Build(const std::vector<std::string>& words) {
    std::vector<std::string> sorted = words;
    std::sort(sorted.begin(), sorted.end());
    da_.Build(sorted);
}

std::vector<std::string> Segmenter::CutSegment(const std::string& sentence) const {
    std::vector<std::string> result;
    int n = static_cast<int>(sentence.size());
    int i = 0;

    while (i < n) {
        auto matches = da_.PrefixSearch(sentence.substr(i));

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
            int len = ustr::CharLen(static_cast<uint8_t>(sentence[i]));
            result.push_back(sentence.substr(i, len));
            i += len;
        }
    }

    return result;
}

std::vector<std::string> Segmenter::Cut(const std::string& sentence) const {
    auto segments = ustr::SplitByPunct(sentence);
    std::vector<std::string> result;

    for (auto& [seg, is_punct] : segments) {
        if (is_punct) {
            result.push_back(seg);
        } else {
            auto words = CutSegment(seg);
            result.insert(result.end(), words.begin(), words.end());
        }
    }

    return result;
}

} // namespace cut
