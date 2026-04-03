#pragma once

#include <vector>
#include <string>
#include <set>
#include <unordered_map>

#include <stdint.h>
#include <math.h>

#include "trie.h"

namespace cut {

using float_t = double;
using float_i = std::pair<float_t, int>;

class NaiveCutter {
private:
    trie::DoubleArray<int> da_;
    uint64_t sum_;

    float_t GetTrieValue(const std::string& value) {
        auto r = da_.GetUnit(value);
        if (!r.found || r.value == 0) {
            // unknown word: penalty = log(1/sum)
            return log(1.0 / static_cast<float_t>(sum_));
        }
        float_t v = static_cast<float_t>(r.value);
        v = v / static_cast<float_t>(sum_);
        v = log(v);
        return v;
    }

public:
    void Build(const std::vector<std::string>& words,
               const std::vector<int>& freqs) {
        uint64_t sum = 0;
        for (int f : freqs) {
            sum += f;
        }
        sum_ = sum;

        std::vector<std::pair<std::string, int>> pairs;
        for (size_t i = 0; i < words.size(); ++i) {
            pairs.emplace_back(words[i], freqs[i]);
        }
        std::sort(pairs.begin(), pairs.end());

        std::vector<std::string> sorted_words;
        std::vector<int> sorted_freqs;
        for (auto& p : pairs) {
            sorted_words.push_back(std::move(p.first));
            sorted_freqs.push_back(p.second);
        }

        da_.Build(sorted_words, sorted_freqs);
    }

    std::vector<std::set<int>> DAG(const std::string& sentence);

    std::vector<float_i> Compute(const std::string& sentence,
                                 const std::vector<std::set<int>>& G);

    std::vector<std::string> CutSegment(const std::string& sentence);
    std::vector<std::string> Cut(const std::string& sentence);

    void CutWithLoss(const std::string& sentence,
                     std::unordered_map<std::string, double>& loss);
};

} // namespace cut
