#include "piece.h"

#include <climits>
#include <fstream>
#include <sstream>
#include <string>

namespace piece {

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

std::string PieceTokenizer::Normalize(std::string_view text) const {
    // Skip leading spaces, replace spaces with space symbol (▁),
    // collapse consecutive spaces, trim trailing space symbol.
    std::string out;
    out.reserve(text.size());
    size_t start = 0;
    while (start < text.size() && text[start] == ' ') ++start;
    bool prev_space = true; // suppress leading ▁ on first space
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
    // Trim trailing space symbol
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

std::vector<std::string> PieceTokenizer::Tokenize(
    std::string_view text) const {
    auto encoded = Encode(text);
    // Merge adjacent tokens that form incomplete UTF-8 sequences.
    std::vector<std::string> tokens;
    tokens.reserve(encoded.size());
    std::string buf;
    for (auto& [piece, id] : encoded) {
        if (buf.empty()) {
            if (IsValidUTF8(piece)) {
                tokens.push_back(std::move(piece));
            } else {
                buf = std::move(piece);
            }
        } else {
            buf += piece;
            if (IsValidUTF8(buf)) {
                tokens.push_back(std::move(buf));
                buf.clear();
            }
        }
    }
    if (!buf.empty()) {
        tokens.push_back(std::move(buf));
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
