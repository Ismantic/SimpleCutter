#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

int CharLen(const uint8_t* p, size_t remaining) {
    if (remaining == 0) return 1;
    uint8_t c = p[0];
    int len;
    if ((c & 0x80) == 0) return 1;
    else if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    else return 1;
    if (static_cast<size_t>(len) > remaining) return 1;
    for (int i = 1; i < len; ++i) {
        if ((p[i] & 0xC0) != 0x80) return 1;
    }
    return len;
}

bool IsChineseChar(const std::string& s) {
    if (s == "〇") return true;

    const unsigned char* p = reinterpret_cast<const unsigned char*>(s.data());
    int len = static_cast<int>(s.size());

    if (len == 3) {
        int cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        return (cp >= 0x3400 && cp <= 0x4DBF) ||
               (cp >= 0x4E00 && cp <= 0x9FFF) ||
               (cp >= 0xF900 && cp <= 0xFAFF);
    }

    if (len == 4) {
        int cp = ((p[0] & 0x07) << 18) |
                 ((p[1] & 0x3F) << 12) |
                 ((p[2] & 0x3F) << 6) |
                 (p[3] & 0x3F);
        return (cp >= 0x20000 && cp <= 0x2A6DF) ||
               (cp >= 0x2A700 && cp <= 0x2B73F) ||
               (cp >= 0x2B740 && cp <= 0x2B81F) ||
               (cp >= 0x2B820 && cp <= 0x2CEAF) ||
               (cp >= 0x2CEB0 && cp <= 0x2EBEF) ||
               (cp >= 0x30000 && cp <= 0x3134F) ||
               (cp >= 0x31350 && cp <= 0x323AF);
    }

    return false;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: count_chars <corpus.txt> <char_freq.txt>\n";
        return 1;
    }

    std::ifstream in(argv[1]);
    if (!in.is_open()) {
        std::cerr << "cannot open: " << argv[1] << "\n";
        return 1;
    }

    std::unordered_map<std::string, uint64_t> counts;
    std::string line;
    while (std::getline(in, line)) {
        const uint8_t* base = reinterpret_cast<const uint8_t*>(line.data());
        for (size_t i = 0; i < line.size();) {
            int len = CharLen(base + i, line.size() - i);
            std::string ch = line.substr(i, len);
            if (IsChineseChar(ch)) {
                ++counts[ch];
            }
            i += len;
        }
    }

    std::vector<std::pair<std::string, uint64_t>> items(counts.begin(), counts.end());
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    std::ofstream out(argv[2]);
    if (!out.is_open()) {
        std::cerr << "cannot open: " << argv[2] << "\n";
        return 1;
    }

    for (const auto& [ch, freq] : items) {
        out << ch << '\t' << freq << '\n';
    }

    std::cerr << "counted " << items.size() << " chars\n";
    return 0;
}
