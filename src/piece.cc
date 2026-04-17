#include "piece.h"
#include "ustr.h"

#include <algorithm>
#include <climits>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace piece {

// ---------------------------------------------------------------------------
// IsWordChar / SplitText — ported from Tokenizer/src/ustr.cc
// ---------------------------------------------------------------------------

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
    if (cp >= 0xFF10 && cp <= 0xFF19) return true;  // fullwidth digits
    return false;
}

static bool IsPunctuationToken(const char* p, const char* end) {
    int len = ustr::CharLen(static_cast<uint8_t>(*p));
    if (p + len > end) len = 1;
    uint32_t cp = ustr::DecodeUTF8(std::string_view(p, len));
    if (IsWordChar(cp)) return false;
    if (cp == 0x20 || cp == 0xA0 || cp == 0x1680 || cp == 0x3000) return false;
    if (cp >= 0x2000 && cp <= 0x200A) return false;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F) return false;
    if (cp == 0x85) return false;
    if (cp < 0x20 || cp == 0x7F) return false;
    if (cp >= 0x80 && cp <= 0x9F) return false;
    return true;
}

static uint32_t DecodeCP(const char* p, const char* end) {
    int len = ustr::CharLen(static_cast<uint8_t>(*p));
    if (p + len > end) len = 1;
    return ustr::DecodeUTF8(std::string_view(p, len));
}

static std::vector<std::string_view> SplitText(std::string_view text,
                                                std::string_view space,
                                                int cut = 0) {
    std::vector<std::string_view> result;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    if (begin >= end) return result;

    // cut=1: spaces and punctuation each become independent tokens.
    // Letters, digits, and Han also form separate runs (same as cut=0).
    if (cut == 1) {
        enum Kind1 { kSpace1, kLetter1, kDigit1, kHan1, kPunct1 };
        auto classify1 = [&](const char* p, int len) -> Kind1 {
            std::string_view c(p, len);
            if (c == space) return kSpace1;
            uint32_t cp = DecodeCP(p, end);
            if (IsHanCP(cp)) return kHan1;
            if (IsDigitCP(cp)) return kDigit1;
            if (IsWordChar(cp)) return kLetter1;
            return kPunct1;
        };
        auto char_len1 = [&](const char* p) -> int {
            return std::min<int>(ustr::CharLen(static_cast<uint8_t>(*p)), end - p);
        };

        const char* p = begin;
        while (p < end) {
            const int clen = char_len1(p);
            const Kind1 kind = classify1(p, clen);

            if (kind == kSpace1 || kind == kPunct1) {
                // Space and punctuation → each standalone token.
                result.emplace_back(p, clen);
                p += clen;
            } else {
                // Letter/Digit/Han → consume same-kind run.
                // For letter runs: apostrophe between letters is not a break
                // (keeps contractions like don't, they'll intact).
                const char* run_start = p;
                p += clen;
                while (p < end) {
                    const int wlen = char_len1(p);
                    Kind1 wkind = classify1(p, wlen);
                    if (wkind == kind) { p += wlen; continue; }
                    // Apostrophe inside letter run: peek ahead.
                    if (kind == kLetter1 && *p == '\'' && p + 1 < end) {
                        const int nlen = char_len1(p + 1);
                        if (classify1(p + 1, nlen) == kLetter1) {
                            p += 1 + nlen;  // consume ' + next letter
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

    auto char_len = [&](const char* p) -> int {
        return std::min<int>(ustr::CharLen(static_cast<uint8_t>(*p)), end - p);
    };

    enum Kind { kSpace, kLetter, kDigit, kHan, kPunct };
    auto classify = [&](const char* p, int len) -> Kind {
        std::string_view c(p, len);
        if (c == space) return kSpace;
        uint32_t cp = DecodeCP(p, end);
        if (IsHanCP(cp)) return kHan;
        if (IsDigitCP(cp)) return kDigit;
        if (IsWordChar(cp)) return kLetter;
        return kPunct;
    };

    const char* pending_space = nullptr;
    int pending_space_len = 0;

    while (begin < end) {
        const int clen = char_len(begin);
        const Kind kind = classify(begin, clen);

        if (kind == kSpace) {
            if (pending_space != nullptr)
                result.emplace_back(pending_space, pending_space_len);
            pending_space = begin;
            pending_space_len = clen;
            begin += clen;
            continue;
        }

        if (kind == kLetter) {
            // Space attaches as prefix to letter runs.
            const char* run_start = pending_space ? pending_space : begin;
            pending_space = nullptr;
            const char* run_end = begin;
            while (run_end < end && classify(run_end, char_len(run_end)) == kLetter)
                run_end += char_len(run_end);
            result.emplace_back(run_start, run_end - run_start);
            begin = run_end;
            continue;
        }

        if (kind == kDigit) {
            // Space does NOT attach to digit runs.
            if (pending_space) {
                result.emplace_back(pending_space, pending_space_len);
                pending_space = nullptr;
            }
            const char* run_start = begin;
            const char* run_end = begin;
            while (run_end < end && classify(run_end, char_len(run_end)) == kDigit)
                run_end += char_len(run_end);
            result.emplace_back(run_start, run_end - run_start);
            begin = run_end;
            continue;
        }

        if (kind == kHan) {
            // Space does NOT attach to Han runs.
            if (pending_space) {
                result.emplace_back(pending_space, pending_space_len);
                pending_space = nullptr;
            }
            const char* run_start = begin;
            const char* run_end = begin;
            while (run_end < end && classify(run_end, char_len(run_end)) == kHan)
                run_end += char_len(run_end);
            result.emplace_back(run_start, run_end - run_start);
            begin = run_end;
            continue;
        }

        // kind == kPunct
        // Punct-as-prefix: one punct char absorbs into a following letter run.
        // Only when no pending space, and only for letter runs (not digit/Han).
        if (pending_space == nullptr && begin + clen < end) {
            const int nlen = char_len(begin + clen);
            if (classify(begin + clen, nlen) == kLetter) {
                const char* run_start = begin;
                const char* run_end = begin + clen;
                while (run_end < end && classify(run_end, char_len(run_end)) == kLetter)
                    run_end += char_len(run_end);
                result.emplace_back(run_start, run_end - run_start);
                begin = run_end;
                continue;
            }
        }

        // Regular punct run. Space attaches as prefix to punct runs.
        const char* run_start = pending_space ? pending_space : begin;
        pending_space = nullptr;
        const char* run_end = begin;
        while (run_end < end && classify(run_end, char_len(run_end)) == kPunct)
            run_end += char_len(run_end);
        result.emplace_back(run_start, run_end - run_start);
        begin = run_end;
    }

    if (pending_space != nullptr)
        result.emplace_back(pending_space, pending_space_len);

    return result;
}

// Unescape \xHH byte sequences in piece text.
static std::string Unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (i + 3 < s.size() && s[i] == '\\' && s[i + 1] == 'x') {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                return -1;
            };
            int hi = hex(s[i + 2]);
            int lo = hex(s[i + 3]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 3;
                continue;
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

bool PieceTokenizer::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    enum Section { NONE, COUNTER, NORMALIZER, PIECES } section = NONE;

    int expected_size = 0;

    while (std::getline(file, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line == "[CounterSpec]") { section = COUNTER; continue; }
        if (line == "[NormalizerSpec]") { section = NORMALIZER; continue; }
        if (line == "[Pieces]") { section = PIECES; continue; }

        if (section == COUNTER) {
            if (line.compare(0, 7, "unk_id=") == 0)
                unk_id_ = std::stoi(line.substr(7));
            else if (line.compare(0, 7, "bos_id=") == 0)
                bos_id_ = std::stoi(line.substr(7));
            else if (line.compare(0, 7, "eos_id=") == 0)
                eos_id_ = std::stoi(line.substr(7));
        } else if (section == NORMALIZER) {
            if (line.compare(0, 6, "space=") == 0)
                space_ = line.substr(6);
            else if (line.compare(0, 13, "reconstruct=") == 0)
                reconstruct_ = std::stoi(line.substr(13)) != 0;
        } else if (section == PIECES) {
            if (line.compare(0, 5, "size=") == 0) {
                expected_size = std::stoi(line.substr(5));
                pieces_.reserve(expected_size);
                continue;
            }

            // Format: id\tpiece\tscore\ttype\tu\tv
            // Fields are tab-separated. piece/u/v may contain \xHH escapes.
            std::vector<std::string> fields;
            std::istringstream iss(line);
            std::string field;
            while (std::getline(iss, field, '\t')) {
                fields.push_back(field);
            }
            // Handle trailing empty fields from consecutive tabs
            if (!line.empty() && line.back() == '\t') {
                fields.push_back("");
            }
            if (fields.size() < 4) continue;

            int id = std::stoi(fields[0]);
            std::string text = Unescape(fields[1]);
            int type = std::stoi(fields[3]);
            std::string u = fields.size() > 4 ? Unescape(fields[4]) : "";
            std::string v = fields.size() > 5 ? Unescape(fields[5]) : "";

            // Grow pieces_ to fit this id
            if (id >= static_cast<int>(pieces_.size())) {
                pieces_.resize(id + 1);
            }

            PieceType pt = NORMAL;
            if (type == 2) pt = UNKNOWN;
            else if (type == 3) pt = CONTROL;

            pieces_[id] = {std::move(text), pt};

            // Merge rules will be built after all pieces are loaded.
            // Store u/v temporarily via a second pass below.
            // Actually, we can build merge_ranks_ inline since pieces
            // are loaded in order and u/v reference earlier pieces.
            if (!u.empty() && !v.empty()) {
                // u and v should already be loaded (earlier ids)
                auto u_it = piece_to_id_.find(u);
                auto v_it = piece_to_id_.find(v);
                if (u_it != piece_to_id_.end() && v_it != piece_to_id_.end()) {
                    merge_ranks_[{u_it->second, v_it->second}] = id;
                }
            }

            // Update piece_to_id_ (must point to stable storage in pieces_)
            piece_to_id_[pieces_[id].text] = id;
        }
    }

    if (pieces_.empty()) return false;

    // Build byte → id lookup
    for (int b = 0; b < 256; ++b) {
        std::string byte_str(1, static_cast<char>(b));
        auto it = piece_to_id_.find(byte_str);
        byte_to_id_[b] = it != piece_to_id_.end() ? it->second : unk_id_;
    }

    ready_ = true;
    return true;
}

std::string PieceTokenizer::Normalize(std::string_view text,
                                      bool reconstruct) const {
    std::string out;
    out.reserve(text.size());

    if (reconstruct) {
        // Preserve all spaces: every space → ▁, no skipping or collapsing.
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == ' ') {
                out.append(space_);
            } else {
                out.push_back(text[i]);
            }
        }
        return out;
    }

    // Default: skip leading spaces, collapse consecutive, trim trailing ▁.
    size_t start = 0;
    while (start < text.size() && text[start] == ' ') ++start;
    bool prev_space = true;
    for (size_t i = start; i < text.size(); ++i) {
        if (text[i] == ' ') {
            if (!prev_space) {
                out.append(space_);
                prev_space = true;
            }
        } else {
            out.push_back(text[i]);
            prev_space = false;
        }
    }
    if (out.size() >= space_.size() &&
        out.compare(out.size() - space_.size(), space_.size(), space_) == 0) {
        out.resize(out.size() - space_.size());
    }
    return out;
}

std::string PieceTokenizer::Denormalize(std::string_view text) const {
    // Replace space symbol (▁) with regular space.
    std::string out;
    out.reserve(text.size());
    size_t pos = 0;
    while (pos < text.size()) {
        size_t found = text.find(space_, pos);
        if (found == std::string_view::npos) {
            out.append(text.substr(pos));
            break;
        }
        out.append(text.substr(pos, found - pos));
        out.push_back(' ');
        pos = found + space_.size();
    }
    // Trim leading space from the leading ▁
    if (!out.empty() && out[0] == ' ') {
        out.erase(0, 1);
    }
    return out;
}

std::vector<int> PieceTokenizer::BuildInitialTokenIds(
    const std::string& text) const {
    std::vector<int> ids;
    ids.reserve(text.size());
    for (unsigned char c : text) {
        ids.push_back(byte_to_id_[c]);
    }
    return ids;
}

void PieceTokenizer::GreedyMerge(std::vector<int>& ids) const {
    while (ids.size() >= 2) {
        int best_rank = INT_MAX;
        size_t best_pos = 0;
        for (size_t i = 0; i + 1 < ids.size(); ++i) {
            auto it = merge_ranks_.find({ids[i], ids[i + 1]});
            if (it != merge_ranks_.end() && it->second < best_rank) {
                best_rank = it->second;
                best_pos = i;
            }
        }
        if (best_rank == INT_MAX) break;
        ids[best_pos] = best_rank;
        ids.erase(ids.begin() + static_cast<ptrdiff_t>(best_pos) + 1);
    }
}

PieceTokenizer::EncodeResult PieceTokenizer::Encode(
    std::string_view text) const {
    std::string normalized = Normalize(text);
    std::vector<int> ids = BuildInitialTokenIds(normalized);
    GreedyMerge(ids);

    EncodeResult result;
    result.reserve(ids.size());
    for (int id : ids) {
        if (id >= 0 && id < static_cast<int>(pieces_.size())) {
            result.emplace_back(pieces_[id].text, id);
        } else {
            result.emplace_back(pieces_[unk_id_].text, unk_id_);
        }
    }
    return result;
}

// Check if a string is valid UTF-8.
static bool IsValidUTF8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        auto c = static_cast<unsigned char>(s[i]);
        int len = 1;
        if ((c & 0x80) == 0) len = 1;
        else if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        else return false;
        if (i + len > s.size()) return false;
        for (int j = 1; j < len; ++j) {
            if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80)
                return false;
        }
        i += len;
    }
    return true;
}

// BPE-encode a pre-normalized chunk (no Normalize call), returning
// string tokens with incomplete UTF-8 sequences merged.
std::vector<std::string> PieceTokenizer::TokenizeChunk(
    std::string_view chunk) const {
    std::vector<int> ids = BuildInitialTokenIds(std::string(chunk));
    GreedyMerge(ids);

    std::vector<std::string> tokens;
    tokens.reserve(ids.size());
    std::string buf;
    for (int id : ids) {
        const std::string& piece =
            (id >= 0 && id < static_cast<int>(pieces_.size()))
                ? pieces_[id].text : pieces_[unk_id_].text;
        if (buf.empty()) {
            if (IsValidUTF8(piece)) {
                tokens.push_back(piece);
            } else {
                buf = piece;
            }
        } else {
            buf += piece;
            if (IsValidUTF8(buf)) {
                tokens.push_back(std::move(buf));
                buf.clear();
            }
        }
    }
    if (!buf.empty()) tokens.push_back(std::move(buf));
    return tokens;
}

std::vector<std::string> PieceTokenizer::Tokenize(
    std::string_view text, bool space, int cut) const {
    // Normalize → SplitText → BPE each chunk.
    std::string normalized = Normalize(text, space);
    auto chunks = SplitText(normalized, space_, cut);

    std::vector<std::string> tokens;
    for (auto chunk : chunks) {
        auto chunk_tokens = TokenizeChunk(chunk);
        tokens.insert(tokens.end(), chunk_tokens.begin(), chunk_tokens.end());
    }
    return tokens;
}

std::vector<int> PieceTokenizer::EncodeIds(std::string_view text) const {
    std::string normalized = Normalize(text);
    std::vector<int> ids = BuildInitialTokenIds(normalized);
    GreedyMerge(ids);
    return ids;
}

std::string PieceTokenizer::Decode(const std::vector<int>& ids) const {
    std::string result;
    for (int id : ids) {
        if (id < 0 || id >= static_cast<int>(pieces_.size())) {
            result += pieces_[unk_id_].text;
            continue;
        }
        const auto& entry = pieces_[id];
        if (entry.type != UNKNOWN && entry.type != CONTROL) {
            result += entry.text;
        }
    }
    return Denormalize(result);
}

int PieceTokenizer::PieceID(std::string_view piece) const {
    auto it = piece_to_id_.find(piece);
    return it != piece_to_id_.end() ? it->second : unk_id_;
}

} // namespace piece
