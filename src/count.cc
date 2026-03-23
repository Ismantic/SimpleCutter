#include "count.h"
#include "ustr.h"

#include <algorithm>

namespace cut {

void Counter::Add(const std::vector<std::string>& words) {
    for (const auto& w : words) {
        if (ustr::IsValidWord(w)) {
            ++counts_[w];
        }
    }
}

void Counter::Add(const std::string& word) {
    if (ustr::IsValidWord(word)) {
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
