#include "cut.h"
#include "ustr.h"

#include <limits>

namespace cut {

std::vector<std::set<int>> NaiveCutter::DAG(const std::string& sentence) {
    int n = sentence.length();
    std::vector<std::set<int>> G(n);

    for (int i = 0; i < n; ) {
        int charlen = ustr::CharLen(static_cast<uint8_t>(sentence[i]));

        auto rs = da_.PrefixSearch(sentence.substr(i));
        for (auto& r : rs) {
            size_t j = i + r.length;
            if (r.length > 0 && j <= static_cast<size_t>(n)) {
                G[i].insert(j - 1);
            }
        }

        // always add single utf-8 char fallback edge
        if (i + charlen <= n) {
            G[i].insert(i + charlen - 1);
        }

        i += charlen;
    }

    return G;
}

std::vector<float_i> NaiveCutter::Compute(const std::string& sentence,
                                          const std::vector<std::set<int>>& G) {
    int n = sentence.length();

    const float_t INF = std::numeric_limits<float_t>::infinity();
    std::vector<float_i> route(n + 1, {-INF, -1});

    route[n] = {1.0, n};

    for (int i = n - 1; i >= 0; --i) {
        float_i c = {-INF, -1};

        for (int x : G[i]) {
            float_t v = GetTrieValue(sentence.substr(i, x - i + 1));
            v += route[x + 1].first;

            if (v > c.first) {
                c.first = v;
                c.second = x;
            }
        }

        route[i] = c;
    }

    return route;
}

std::vector<std::string> NaiveCutter::CutSegment(const std::string& sentence) {
    std::vector<std::set<int>> G = DAG(sentence);
    std::vector<float_i> R = Compute(sentence, G);

    std::vector<std::string> rs;
    int x = 0;
    int n = sentence.length();

    while (x < n) {
        int y = R[x].second + 1;
        if (y <= x) {
            int len = ustr::CharLen(static_cast<uint8_t>(sentence[x]));
            rs.push_back(sentence.substr(x, len));
            x += len;
        } else {
            rs.push_back(sentence.substr(x, y - x));
            x = y;
        }
    }

    return rs;
}

std::vector<std::string> NaiveCutter::Cut(const std::string& sentence) {
    auto segments = ustr::SplitByPunct(sentence);
    std::vector<std::string> rs;

    for (auto& [seg, is_punct] : segments) {
        if (is_punct) {
            rs.push_back(seg);
        } else {
            auto words = CutSegment(seg);
            rs.insert(rs.end(), words.begin(), words.end());
        }
    }

    return rs;
}

void NaiveCutter::CutWithLoss(const std::string& sentence,
                              std::unordered_map<std::string, double>& loss) {
    auto segments = ustr::SplitByPunct(sentence);

    for (auto& [seg, is_punct] : segments) {
        if (is_punct) continue;

        auto G = DAG(seg);
        int n = seg.length();

        const float_t NEG_INF = -std::numeric_limits<float_t>::infinity();

        // DP with best and second-best tracking
        std::vector<float_t> best(n + 1, NEG_INF);
        std::vector<int> best_end(n + 1, -1);
        std::vector<float_t> second(n + 1, NEG_INF);

        best[n] = 1.0;
        best_end[n] = n;

        for (int i = n - 1; i >= 0; --i) {
            for (int x : G[i]) {
                float_t v = GetTrieValue(seg.substr(i, x - i + 1))
                          + best[x + 1];
                if (v > best[i]) {
                    second[i] = best[i];
                    best[i] = v;
                    best_end[i] = x;
                } else if (v > second[i]) {
                    second[i] = v;
                }
            }
        }

        // Walk optimal path, accumulate loss per word
        int x = 0;
        while (x < n) {
            int y = best_end[x] + 1;
            if (y <= x) {
                x += ustr::CharLen(static_cast<uint8_t>(seg[x]));
            } else {
                std::string word = seg.substr(x, y - x);
                loss[word] += best[x] - second[x];
                x = y;
            }
        }
    }
}

} // namespace cut
