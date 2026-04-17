#include "cut.h"
#include "ustr.h"

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace cut {

// ---------------------------------------------------------------------------
// Pre-tokenization: Normalize → SplitText (cut=1)
// Ported from piece.cc — only the cut=1 path is kept.
// ---------------------------------------------------------------------------

static const std::string kSpace = "\xe2\x96\x81"; // ▁

static bool IsWordChar(uint32_t cp) {
    if (cp >= '0' && cp <= '9') return true;
    if (cp >= 'A' && cp <= 'Z') return true;
    if (cp >= 'a' && cp <= 'z') return true;
    if (cp >= 0x00C0 && cp <= 0x00FF && cp != 0x00D7 && cp != 0x00F7) return true;
    if (cp >= 0x0100 && cp <= 0x02FF) return true;
    if (cp >= 0x0300 && cp <= 0x036F) return true;
    if (cp >= 0x0370 && cp <= 0x07FF) return true;
    if (cp >= 0x0800 && cp <= 0x085F) return true;
    if (cp >= 0x0900 && cp <= 0x1CFF) return true;
    if (cp >= 0x1D00 && cp <= 0x1FFF) return true;
    if (cp >= 0x2E80 && cp <= 0x2FFF) return true;
    if (cp >= 0x3040 && cp <= 0x31FF) return true;
    if (cp >= 0x3200 && cp <= 0x33FF) return true;
    if (cp >= 0x3400 && cp <= 0x9FFF) return true;
    if (cp >= 0xA000 && cp <= 0xABFF) return true;
    if (cp >= 0xAC00 && cp <= 0xD7FF) return true;
    if (cp >= 0xF900 && cp <= 0xFAFF) return true;
    if (cp >= 0xFB00 && cp <= 0xFDFF) return true;
    if (cp >= 0xFE70 && cp <= 0xFEFF) return true;
    if (cp >= 0xFF10 && cp <= 0xFF19) return true;
    if ((cp >= 0xFF21 && cp <= 0xFF3A) ||
        (cp >= 0xFF41 && cp <= 0xFF5A)) return true;
    if (cp >= 0xFF66 && cp <= 0xFF9F) return true;
    if (cp >= 0xFFA0 && cp <= 0xFFDC) return true;
    if (cp >= 0x10000 && cp <= 0x103FF) return true;
    if (cp >= 0x10400 && cp <= 0x10FFF) return true;
    if (cp >= 0x20000 && cp <= 0x323AF) return true;
    return false;
}

static bool IsHanCP(uint32_t cp) {
    return (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0x20000 && cp <= 0x323AF);
}

static bool IsDigitCP(uint32_t cp) {
    if (cp >= '0' && cp <= '9') return true;
    if (cp >= 0xFF10 && cp <= 0xFF19) return true;
    return false;
}

static std::string Normalize(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    size_t start = 0;
    while (start < text.size() && text[start] == ' ') ++start;
    bool prev_space = true;
    for (size_t i = start; i < text.size(); ++i) {
        if (text[i] == ' ') {
            if (!prev_space) {
                out.append(kSpace);
                prev_space = true;
            }
        } else {
            out.push_back(text[i]);
            prev_space = false;
        }
    }
    if (out.size() >= kSpace.size() &&
        out.compare(out.size() - kSpace.size(), kSpace.size(), kSpace) == 0) {
        out.resize(out.size() - kSpace.size());
    }
    return out;
}

static std::vector<std::string> PreTokenize(const std::string& text) {
    std::string normalized = Normalize(text);

    std::vector<std::string> result;
    const char* begin = normalized.data();
    const char* end = begin + normalized.size();
    if (begin >= end) return result;

    auto char_len = [&](const char* p) -> int {
        return std::min<int>(ustr::CharLen(static_cast<uint8_t>(*p)), end - p);
    };

    enum Kind { SPACE, LETTER, DIGIT, HAN, PUNCT };
    auto classify = [&](const char* p, int len) -> Kind {
        std::string_view c(p, len);
        if (c == kSpace) return SPACE;
        uint32_t cp = ustr::DecodeUTF8(c);
        if (IsHanCP(cp)) return HAN;
        if (IsDigitCP(cp)) return DIGIT;
        if (IsWordChar(cp)) return LETTER;
        return PUNCT;
    };

    const char* p = begin;
    while (p < end) {
        const int clen = char_len(p);
        const Kind kind = classify(p, clen);

        if (kind == SPACE || kind == PUNCT) {
            result.emplace_back(p, clen);
            p += clen;
        } else {
            const char* run_start = p;
            p += clen;
            while (p < end) {
                const int wlen = char_len(p);
                Kind wkind = classify(p, wlen);
                if (wkind == kind) { p += wlen; continue; }
                // Apostrophe inside letter run (don't, they'll).
                if (kind == LETTER && *p == '\'' && p + 1 < end) {
                    const int nlen = char_len(p + 1);
                    if (classify(p + 1, nlen) == LETTER) {
                        p += 1 + nlen;
                        continue;
                    }
                }
                break;
            }
            result.emplace_back(run_start, p - run_start);
        }
    }

    return result;
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

// Check if a chunk starts with a Han character (i.e. is a Han run).
static bool IsHanChunk(const std::string& chunk) {
    if (chunk.empty()) return false;
    int len = ustr::CharLen(static_cast<uint8_t>(chunk[0]));
    if (len > static_cast<int>(chunk.size())) len = chunk.size();
    uint32_t cp = ustr::DecodeUTF8(std::string_view(chunk.data(), len));
    return IsHanCP(cp);
}

// ---------------------------------------------------------------------------
// Cutter
// ---------------------------------------------------------------------------

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
    auto chunks = PreTokenize(sentence);
    std::vector<std::string> rs;

    for (auto& chunk : chunks) {
        if (IsHanChunk(chunk)) {
            if (HasDict()) {
                auto words = CutSegment(chunk);
                rs.insert(rs.end(), words.begin(), words.end());
            } else {
                auto chars = SplitChars(chunk);
                rs.insert(rs.end(), chars.begin(), chars.end());
            }
        } else {
            rs.push_back(chunk);
        }
    }

    return rs;
}

void Cutter::CutWithLoss(const std::string& sentence,
                              std::unordered_map<std::string, double>& loss,
                              std::unordered_map<std::string, int>& count) {
    auto chunks = PreTokenize(sentence);

    for (auto& chunk : chunks) {
        if (!IsHanChunk(chunk)) continue;

        auto G = DAG(chunk);
        auto R = Compute(chunk, G);
        float_t best_score = R[0].first;

        // Walk optimal path, collect words
        struct Word { int start; int end; std::string text; };
        std::vector<Word> path;
        int x = 0;
        int n = chunk.length();
        while (x < n) {
            int y = R[x].second + 1;
            if (y <= x) {
                x += ustr::CharLen(static_cast<uint8_t>(chunk[x]));
            } else {
                path.push_back({x, R[x].second, chunk.substr(x, y - x)});
                x = y;
            }
        }

        // For each word on the optimal path, remove its edge and re-run DP
        for (auto& w : path) {
            int charlen = ustr::CharLen(static_cast<uint8_t>(chunk[w.start]));
            if (w.end - w.start + 1 == charlen) {
                float_t known = GetTrieValue(w.text);
                float_t unknown = log(1.0 / static_cast<float_t>(sum_));
                loss[w.text] += known - unknown;
                count[w.text]++;
                continue;
            }

            // Temporarily remove the edge
            G[w.start].erase(w.end);

            auto R2 = Compute(chunk, G);
            float_t alt_score = R2[0].first;
            loss[w.text] += best_score - alt_score;
            count[w.text]++;

            // Restore the edge
            G[w.start].insert(w.end);
        }
    }
}

} // namespace cut
