#include "cut.h"
#include "ustr.h"

#include <limits>

namespace cut {

std::vector<std::set<int>> Cutter::DAG(const std::string& sentence) {
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

std::vector<float_i> Cutter::Compute(const std::string& sentence,
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

std::vector<std::string> Cutter::CutSegment(const std::string& sentence) {
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

std::vector<std::string> Cutter::Cut(const std::string& sentence) {
    auto segments = ustr::SplitByPunct(sentence);
    std::vector<std::string> rs;

    for (auto& [seg, is_punct] : segments) {
        if (is_punct) {
            rs.push_back(seg);
            continue;
        }
        auto words = CutSegment(seg);
        rs.insert(rs.end(), words.begin(), words.end());
    }

    return rs;
}

void Cutter::CutWithLoss(const std::string& sentence,
                              std::unordered_map<std::string, double>& loss,
                              std::unordered_map<std::string, int>& count) {
    auto segments = ustr::SplitByPunct(sentence);

    for (auto& [seg, is_punct] : segments) {
        if (is_punct) continue;

        auto G = DAG(seg);
        auto R = Compute(seg, G);
        float_t best_score = R[0].first;

        // Walk optimal path, collect words
        struct Word { int start; int end; std::string text; };
        std::vector<Word> path;
        int x = 0;
        int n = seg.length();
        while (x < n) {
            int y = R[x].second + 1;
            if (y <= x) {
                x += ustr::CharLen(static_cast<uint8_t>(seg[x]));
            } else {
                path.push_back({x, R[x].second, seg.substr(x, y - x)});
                x = y;
            }
        }

        // For each word on the optimal path, remove its edge and re-run DP
        for (auto& w : path) {
            int charlen = ustr::CharLen(static_cast<uint8_t>(seg[w.start]));
            if (w.end - w.start + 1 == charlen) {
                // Single character: edge is also the fallback, can't remove.
                // Loss = score(known) - score(unknown)
                float_t known = GetTrieValue(w.text);
                float_t unknown = log(1.0 / static_cast<float_t>(sum_));
                loss[w.text] += known - unknown;
                count[w.text]++;
                continue;
            }

            // Temporarily remove the edge
            G[w.start].erase(w.end);

            auto R2 = Compute(seg, G);
            float_t alt_score = R2[0].first;
            loss[w.text] += best_score - alt_score;
            count[w.text]++;

            // Restore the edge
            G[w.start].insert(w.end);
        }
    }
}

void MixCutter::Build(const std::vector<std::string>& words,
                           const std::vector<int>& freqs) {
    cutter_.Build(words, freqs);
}

bool MixCutter::LoadPiece(const std::string& path) {
    return piece_.Load(path);
}

// Split a string into individual UTF-8 characters.
static std::vector<std::string> SplitChars(const std::string& s) {
    std::vector<std::string> chars;
    size_t i = 0;
    while (i < s.size()) {
        int len = ustr::CharLen(static_cast<uint8_t>(s[i]));
        if (i + len > s.size()) len = 1;
        chars.push_back(s.substr(i, len));
        i += len;
    }
    return chars;
}

std::vector<std::string> MixCutter::Cut(const std::string& sentence,
                                              bool cn, bool en,
                                              bool space, int cut) {
    // Split by Han/non-Han so each run gets appropriate treatment.
    auto runs = ustr::SplitByHan(sentence);
    std::vector<std::string> rs;

    for (auto& [run, is_han] : runs) {
        if (is_han && cn) {
            auto words = cutter_.Cut(run);
            rs.insert(rs.end(), words.begin(), words.end());
        } else if (!is_han && en && piece_.Ready()) {
            auto tokens = piece_.Tokenize(run, space, cut);
            rs.insert(rs.end(), tokens.begin(), tokens.end());
        } else if (!is_han) {
            auto tokens = piece_.PreTokenize(run, space, cut);
            rs.insert(rs.end(), tokens.begin(), tokens.end());
        }
    }

    return rs;
}

} // namespace cut
