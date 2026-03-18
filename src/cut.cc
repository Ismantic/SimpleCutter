#include "cut.h"

#include <limits>

namespace cut {

static int Utf8CharLen(uint8_t c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

std::vector<std::set<int>> NaiveCutter::DAG(const std::string& sentence) {
    int n = sentence.length();
    std::vector<std::set<int>> G(n);

    for (int i = 0; i < n; ) {
        int charlen = Utf8CharLen(static_cast<uint8_t>(sentence[i]));

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

std::vector<std::string> NaiveCutter::Cut(const std::string& sentence) {
    std::vector<std::set<int>> G = DAG(sentence);
    std::vector<float_i> R = Compute(sentence, G);

    std::vector<std::string> rs;
    int x = 0;
    int n = sentence.length();

    while (x < n) {
        int y = R[x].second + 1;
        if (y <= x) {
            // no match, output single utf-8 char and move forward
            int len = Utf8CharLen(static_cast<uint8_t>(sentence[x]));
            rs.push_back(sentence.substr(x, len));
            x += len;
        } else {
            rs.push_back(sentence.substr(x, y - x));
            x = y;
        }
    }

    return rs;
}

} // namespace cut
