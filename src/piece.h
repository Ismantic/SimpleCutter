#pragma once

#include <climits>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace piece {

class PieceTokenizer {
public:
    using EncodeResult = std::vector<std::pair<std::string, int>>;

    PieceTokenizer() = default;

    // Load model from piece.txt format.
    bool Load(const std::string& path);

    bool Ready() const { return ready_; }
    int VocabSize() const { return static_cast<int>(pieces_.size()); }

    // Encode text into (piece, id) pairs.
    EncodeResult Encode(std::string_view text) const;

    // Encode text into piece strings.
    // space=true: preserve all spaces (reconstruct mode).
    std::vector<std::string> Tokenize(std::string_view text,
                                      bool space = false,
                                      int cut = 0) const;

    // Encode text into token IDs.
    std::vector<int> EncodeIds(std::string_view text) const;

    // Decode token IDs back to text.
    std::string Decode(const std::vector<int>& ids) const;

    // Look up piece → id. Returns unk_id if not found.
    int PieceID(std::string_view piece) const;

    int UnkID() const { return unk_id_; }
    int BosID() const { return bos_id_; }
    int EosID() const { return eos_id_; }

private:
    struct PairHash {
        size_t operator()(const std::pair<int, int>& p) const {
            return std::hash<int>{}(p.first) ^
                   (std::hash<int>{}(p.second) << 1);
        }
    };

    enum PieceType {
        NORMAL = 1,
        UNKNOWN = 2,
        CONTROL = 3,
    };

    struct PieceEntry {
        std::string text;
        PieceType type = NORMAL;
    };

    std::string Normalize(std::string_view text, bool reconstruct = false) const;
    std::string Denormalize(std::string_view text) const;
    std::vector<int> BuildInitialTokenIds(const std::string& text) const;
    void GreedyMerge(std::vector<int>& ids) const;
    std::vector<std::string> TokenizeChunk(std::string_view chunk) const;

    std::vector<PieceEntry> pieces_;
    std::unordered_map<std::string_view, int> piece_to_id_;
    std::unordered_map<std::pair<int, int>, int, PairHash> merge_ranks_;
    int byte_to_id_[256]{};
    int unk_id_ = 0;
    int bos_id_ = 1;
    int eos_id_ = 2;
    std::string space_ = "\xe2\x96\x81"; // ▁
    std::string normalizer_name_ = "identity";
    bool reconstruct_ = false;
    bool ready_ = false;
};

} // namespace piece
