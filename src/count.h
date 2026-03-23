#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace cut {

class Counter {
public:
    void Add(const std::vector<std::string>& words);
    void Add(const std::string& word);

    const std::unordered_map<std::string, int>& Counts() const { return counts_; }

    // Export sorted by word (for trie building)
    void Export(std::vector<std::string>& words, std::vector<int>& freqs) const;

private:
    std::unordered_map<std::string, int> counts_;
};

} // namespace cut
