#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace ustr {

// Returns the byte length of a UTF-8 character given its leading byte.
inline int CharLen(uint8_t c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// Returns true if the string is valid UTF-8 and not empty/whitespace-only.
bool IsValidWord(const std::string& s);

inline uint32_t DecodeUTF8(std::string_view ch) {
    if (ch.empty()) return 0;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(ch.data());
    if (ch.size() == 1) return p[0];
    if (ch.size() == 2) {
        return ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    }
    if (ch.size() == 3) {
        return ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }
    if (ch.size() == 4) {
        return ((p[0] & 0x07) << 18) |
               ((p[1] & 0x3F) << 12) |
               ((p[2] & 0x3F) << 6) |
               (p[3] & 0x3F);
    }
    return 0;
}

inline bool IsHan(std::string_view ch) {
    const uint32_t cp = DecodeUTF8(ch);
    return (cp >= 0x3000 && cp <= 0x303F) ||   // CJK symbols & punctuation
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE30 && cp <= 0xFE4F) ||   // CJK compatibility forms
           (cp >= 0xFF01 && cp <= 0xFF60) ||   // fullwidth forms
           (cp >= 0x20000 && cp <= 0x2A6DF) ||
           (cp >= 0x2A700 && cp <= 0x2B73F) ||
           (cp >= 0x2B740 && cp <= 0x2B81F) ||
           (cp >= 0x2B820 && cp <= 0x2CEAF) ||
           (cp >= 0x2CEB0 && cp <= 0x2EBEF) ||
           (cp >= 0x30000 && cp <= 0x3134F);
}

// Returns true if the UTF-8 character is a punctuation or whitespace delimiter.
inline bool IsPunct(const std::string& ch) {
    if (ch.size() == 1) {
        unsigned char c = ch[0];
        // ASCII punctuation and whitespace
        return c <= 0x7F && !((c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'Z') ||
                              (c >= 'a' && c <= 'z'));
    }
    if (ch.size() == 3) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(ch.data());
        int cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        // CJK punctuation & symbols
        if (cp >= 0x3000 && cp <= 0x303F) return true;
        // Fullwidth punctuation
        if (cp >= 0xFF01 && cp <= 0xFF0F) return true;
        if (cp >= 0xFF1A && cp <= 0xFF20) return true;
        if (cp >= 0xFF3B && cp <= 0xFF40) return true;
        if (cp >= 0xFF5B && cp <= 0xFF65) return true;
        // General punctuation
        if (cp >= 0x2000 && cp <= 0x206F) return true;
    }
    return false;
}

// Split a string into segments by punctuation/whitespace delimiters.
// Each punctuation char is its own segment (marked true).
// Consecutive non-punct chars are merged into one segment (marked false).
inline std::vector<std::pair<std::string, bool>> SplitByPunct(const std::string& s) {
    std::vector<std::pair<std::string, bool>> result;
    const int n = static_cast<int>(s.size());
    int i = 0;
    int seg_start = 0;

    while (i < n) {
        int len = CharLen(static_cast<uint8_t>(s[i]));
        if (i + len > n) len = 1;

        if (IsPunct(s.substr(i, len))) {
            if (seg_start < i) {
                result.emplace_back(s.substr(seg_start, i - seg_start), false);
            }
            result.emplace_back(s.substr(i, len), true);
            seg_start = i + len;
        }
        i += len;
    }
    if (seg_start < n) {
        result.emplace_back(s.substr(seg_start, n - seg_start), false);
    }
    return result;
}

// Character type for non-Han pre-tokenization.
enum class CharType { kAlpha, kDigit, kSpace, kPunct };

inline CharType GetCharType(const std::string& ch) {
    if (ch.size() == 1) {
        unsigned char c = ch[0];
        if (c >= 'A' && c <= 'Z') return CharType::kAlpha;
        if (c >= 'a' && c <= 'z') return CharType::kAlpha;
        if (c >= '0' && c <= '9') return CharType::kDigit;
        if (c == ' ' || c == '\t') return CharType::kSpace;
        return CharType::kPunct;
    }
    // Multi-byte: check if it's punctuation (space already handled above)
    if (IsPunct(ch)) return CharType::kPunct;
    return CharType::kAlpha;  // Other multi-byte (e.g. accented letters)
}

// Check if position i starts an English contraction suffix ('s, 't, 'd, 'm, 'll, 've, 're).
inline int ContractionLen(const std::string& s, int i) {
    int n = static_cast<int>(s.size());
    if (i >= n || s[i] != '\'') return 0;
    if (i + 1 >= n) return 0;
    char c = s[i + 1] | 0x20;  // lowercase
    if (c == 's' || c == 't' || c == 'd' || c == 'm') return 2;
    if (i + 2 < n) {
        char c2 = s[i + 2] | 0x20;
        if (c == 'l' && c2 == 'l') return 3;
        if (c == 'v' && c2 == 'e') return 3;
        if (c == 'r' && c2 == 'e') return 3;
    }
    return 0;
}

// Split non-Han text into chunks, aligned with GPT-4 pre-tokenization:
//   - Contraction suffixes ('s, 't, 'd, 'm, 'll, 've, 're) as separate text chunks
//   - Optional single non-letter/non-number char + letters as one text chunk
//   - Digit runs (unlimited length)
//   - Optional single space + consecutive punct chars as one punct chunk
//   - Newlines (with optional preceding whitespace) as punct chunks
//   - Trailing whitespace / multi-space runs as text chunks
// Punctuation chunks are marked true. Everything else is marked false.
inline std::vector<std::pair<std::string, bool>> SplitNonHan(const std::string& s) {
    std::vector<std::pair<std::string, bool>> result;
    const int n = static_cast<int>(s.size());
    if (n == 0) return result;

    int i = 0;
    while (i < n) {
        // 1. Contraction: 's, 't, 'd, 'm, 'll, 've, 're
        int cl = ContractionLen(s, i);
        if (cl > 0) {
            result.emplace_back(s.substr(i, cl), false);
            i += cl;
            continue;
        }

        int len = CharLen(static_cast<uint8_t>(s[i]));
        if (len <= 0 || i + len > n) len = 1;

        // 2. Newlines: \s*[\r\n]+
        if (s[i] == '\n' || s[i] == '\r') {
            int start = i;
            while (i < n && (s[i] == '\n' || s[i] == '\r')) ++i;
            result.emplace_back(s.substr(start, i - start), true);
            continue;
        }

        CharType ct = GetCharType(s.substr(i, len));

        // 3. Single space/punct + letters → one text chunk
        //    (mirrors [^\r\n\p{L}\p{N}]?+\p{L}+)
        if (ct == CharType::kAlpha) {
            int seg_start = i;
            while (i < n) {
                int l = CharLen(static_cast<uint8_t>(s[i]));
                if (l <= 0 || i + l > n) l = 1;
                if (GetCharType(s.substr(i, l)) != CharType::kAlpha) break;
                i += l;
            }
            result.emplace_back(s.substr(seg_start, i - seg_start), false);
            continue;
        }

        // 4. Digits: consecutive run
        if (ct == CharType::kDigit) {
            int seg_start = i;
            while (i < n) {
                int l = CharLen(static_cast<uint8_t>(s[i]));
                if (l <= 0 || i + l > n) l = 1;
                if (GetCharType(s.substr(i, l)) != CharType::kDigit) break;
                i += l;
            }
            result.emplace_back(s.substr(seg_start, i - seg_start), false);
            continue;
        }

        // 5. Space handling
        if (ct == CharType::kSpace) {
            int space_start = i;
            int space_count = 0;
            while (i < n && (s[i] == ' ' || s[i] == '\t')) { ++i; ++space_count; }

            // Check if spaces precede a newline → attach to newline
            if (i < n && (s[i] == '\n' || s[i] == '\r')) {
                int nl_start = i;
                while (i < n && (s[i] == '\n' || s[i] == '\r')) ++i;
                result.emplace_back(s.substr(space_start, i - space_start), true);
                continue;
            }

            if (space_count == 1 && i < n) {
                // Single space: try to attach to next token
                int l = CharLen(static_cast<uint8_t>(s[i]));
                if (l <= 0 || i + l > n) l = 1;
                CharType next_ct = GetCharType(s.substr(i, l));

                if (next_ct == CharType::kAlpha) {
                    // " word" — single space + letters
                    while (i < n) {
                        int l2 = CharLen(static_cast<uint8_t>(s[i]));
                        if (l2 <= 0 || i + l2 > n) l2 = 1;
                        if (GetCharType(s.substr(i, l2)) != CharType::kAlpha) break;
                        i += l2;
                    }
                    result.emplace_back(s.substr(space_start, i - space_start), false);
                } else if (next_ct == CharType::kDigit) {
                    // " 123" — single space + digits
                    while (i < n) {
                        int l2 = CharLen(static_cast<uint8_t>(s[i]));
                        if (l2 <= 0 || i + l2 > n) l2 = 1;
                        if (GetCharType(s.substr(i, l2)) != CharType::kDigit) break;
                        i += l2;
                    }
                    result.emplace_back(s.substr(space_start, i - space_start), false);
                } else if (next_ct == CharType::kPunct) {
                    // " ..." — single space + punct run
                    while (i < n) {
                        int l2 = CharLen(static_cast<uint8_t>(s[i]));
                        if (l2 <= 0 || i + l2 > n) l2 = 1;
                        if (s[i] == '\'' && ContractionLen(s, i) > 0) break;
                        if (s[i] == '\n' || s[i] == '\r') break;
                        if (GetCharType(s.substr(i, l2)) != CharType::kPunct) break;
                        i += l2;
                    }
                    result.emplace_back(s.substr(space_start, i - space_start), true);
                } else {
                    result.emplace_back(s.substr(space_start, i - space_start), false);
                }
            } else {
                // Multiple spaces: standalone chunk (like \s+)
                result.emplace_back(s.substr(space_start, i - space_start), false);
            }
            continue;
        }

        // 6. Punctuation run: consecutive punct chars
        {
            int start = i;
            while (i < n) {
                int l = CharLen(static_cast<uint8_t>(s[i]));
                if (l <= 0 || i + l > n) l = 1;
                if (s[i] == '\'' && ContractionLen(s, i) > 0) break;
                if (s[i] == '\n' || s[i] == '\r') break;
                if (GetCharType(s.substr(i, l)) != CharType::kPunct) break;
                i += l;
            }
            result.emplace_back(s.substr(start, i - start), true);
        }
    }
    return result;
}

// Split a non-punctuation segment into contiguous Han / non-Han runs.
inline std::vector<std::pair<std::string, bool>> SplitByHan(const std::string& s) {
    std::vector<std::pair<std::string, bool>> result;
    const int n = static_cast<int>(s.size());
    if (n == 0) return result;

    int i = 0;
    int seg_start = 0;
    int first_len = CharLen(static_cast<uint8_t>(s[0]));
    if (first_len <= 0 || first_len > n) first_len = 1;
    bool seg_is_han = IsHan(std::string_view(s.data(), first_len));

    while (i < n) {
        int len = CharLen(static_cast<uint8_t>(s[i]));
        if (len <= 0 || i + len > n) len = 1;
        bool is_han = IsHan(std::string_view(s.data() + i, len));

        if (i > seg_start && is_han != seg_is_han) {
            result.emplace_back(s.substr(seg_start, i - seg_start), seg_is_han);
            seg_start = i;
            seg_is_han = is_han;
        }
        i += len;
    }

    if (seg_start < n) {
        result.emplace_back(s.substr(seg_start, n - seg_start), seg_is_han);
    }
    return result;
}

} // namespace ustr
