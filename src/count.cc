#include "count.h"

#include <algorithm>
#include <stdint.h>

namespace cut {

static bool IsValidWord(const std::string& s) {
    if (s.empty()) return false;
    // Check valid UTF-8
    size_t i = 0;
    while (i < s.size()) {
        uint8_t c = static_cast<uint8_t>(s[i]);
        int len = 0;
        if ((c & 0x80) == 0) { len = 1; }
        else if ((c & 0xE0) == 0xC0) { len = 2; }
        else if ((c & 0xF0) == 0xE0) { len = 3; }
        else if ((c & 0xF8) == 0xF0) { len = 4; }
        else { return false; }
        if (i + len > s.size()) return false;
        for (int j = 1; j < len; ++j) {
            if ((static_cast<uint8_t>(s[i + j]) & 0xC0) != 0x80) return false;
        }
        i += len;
    }
    // Reject whitespace-only
    for (char c : s) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return true;
    }
    return false;
}

void Counter::Add(const std::vector<std::string>& words) {
    for (const auto& w : words) {
        if (IsValidWord(w)) {
            ++counts_[w];
        }
    }
}

void Counter::Add(const std::string& word) {
    if (IsValidWord(word)) {
        ++counts_[word];
    }
}

void Counter::Export(std::vector<std::string>& words, std::vector<int>& freqs) const {
    words.clear();
    freqs.clear();
    words.reserve(counts_.size());
    freqs.reserve(counts_.size());

    for (const auto& [word, freq] : counts_) {
        words.push_back(word);
        freqs.push_back(freq);
    }

    // Sort by word for trie building
    std::vector<std::size_t> order(words.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return words[a] < words[b];
    });

    std::vector<std::string> sw(words.size());
    std::vector<int> sf(freqs.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        sw[i] = std::move(words[order[i]]);
        sf[i] = freqs[order[i]];
    }
    words = std::move(sw);
    freqs = std::move(sf);
}

} // namespace cut
