#pragma once

#include "trie.h"

#include <string>
#include <vector>

namespace cut {

class Segmenter {
public:
    void Build(const std::vector<std::string>& words);
    std::vector<std::string> CutSegment(const std::string& sentence) const;
    std::vector<std::string> Cut(const std::string& sentence) const;

private:
    trie::DoubleArray<int> da_;
};

} // namespace cut
