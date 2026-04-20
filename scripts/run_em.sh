#!/bin/bash
set -euo pipefail

DICT=${1:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> <vocab_size> [sub_iters=2] [min_count=5] [nproc=auto]}
CORPUS=${2:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> <vocab_size> [sub_iters=2] [min_count=5] [nproc=auto]}
OUT=${3:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> <vocab_size> [sub_iters=2] [min_count=5] [nproc=auto]}
VOCAB_SIZE=${4:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> <vocab_size> [sub_iters=2] [min_count=5] [nproc=auto]}
SUB_ITERS=${5:-2}
MIN_COUNT=${6:-5}
NPROC=${7:-$(nproc)}

ISCUT="$(dirname "$0")/../build/iscut"
mkdir -p "$OUT"

# Clean up background processes on exit
trap 'kill $(jobs -p) 2>/dev/null || true; wait 2>/dev/null' EXIT

# ---------------------------------------------------------------------------
# Split corpus into NPROC chunks (once, reused across all rounds)
# ---------------------------------------------------------------------------
CHUNKS_DIR="$OUT/chunks"
rm -rf "$CHUNKS_DIR"
mkdir -p "$CHUNKS_DIR"
echo "=== Splitting corpus into $NPROC chunks ==="
split -n "l/$NPROC" "$CORPUS" "$CHUNKS_DIR/part."
CHUNKS=("$CHUNKS_DIR"/part.*)
echo "Split into ${#CHUNKS[@]} chunks"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# parallel_segment <dict> — longest-match segment each chunk, output chunk.cut files
parallel_segment() {
    local dict=$1
    local pids=()
    for chunk in "${CHUNKS[@]}"; do
        "$ISCUT" --dict "$dict" --segment "$chunk" "$chunk.cut" &
        pids+=($!)
    done
    for pid in "${pids[@]}"; do wait "$pid"; done
}

# parallel_cut <dict> — DAG+DP cut each chunk, output chunk.cut files
parallel_cut() {
    local dict=$1
    local pids=()
    for chunk in "${CHUNKS[@]}"; do
        "$ISCUT" --dict "$dict" --cut "$chunk" "$chunk.cut" &
        pids+=($!)
    done
    for pid in "${pids[@]}"; do wait "$pid"; done
}

# parallel_count <dict> — count each chunk.cut, then merge frequencies
# Output: $OUT/dict.counted
parallel_count() {
    local dict=$1
    local pids=()
    for chunk in "${CHUNKS[@]}"; do
        "$ISCUT" --dict "$dict" --count "$chunk.cut" "$chunk.cnt" &
        pids+=($!)
    done
    for pid in "${pids[@]}"; do wait "$pid"; done

    # Merge: sum frequencies per word, output sorted by word
    awk -F'\t' '{freq[$1]+=$2} END{for(w in freq) print w"\t"freq[w]}' \
        "${CHUNKS[@]/%/.cnt}" | sort -t$'\t' -k1,1 > "$OUT/dict.counted"
}

# parallel_prune <dict> <output> — prune each chunk, then merge loss+count
parallel_prune() {
    local dict=$1
    local output=$2
    local pids=()
    for chunk in "${CHUNKS[@]}"; do
        "$ISCUT" --dict "$dict" --prune "$chunk" "$chunk.prune" &
        pids+=($!)
    done
    for pid in "${pids[@]}"; do wait "$pid"; done

    # Merge: sum loss and count per word
    awk -F'\t' '{loss[$1]+=$2; cnt[$1]+=$3} END{for(w in loss) print w"\t"loss[w]"\t"cnt[w]}' \
        "${CHUNKS[@]/%/.prune}" | sort -t$'\t' -k2,2 -g -r > "$output"
}

# ---------------------------------------------------------------------------
# Cold start: longest match → count → initial dict
# ---------------------------------------------------------------------------
echo "=== Cold start ==="
parallel_segment "$DICT"
parallel_count "$DICT"
mv "$OUT/dict.counted" "$OUT/dict.current"

current_size=$(wc -l < "$OUT/dict.current")
echo "Initial vocab: $current_size, target: $VOCAB_SIZE"

# ---------------------------------------------------------------------------
# EM + prune loop
# ---------------------------------------------------------------------------
round=0
while [ "$current_size" -gt "$VOCAB_SIZE" ]; do
    round=$((round + 1))

    # EM sub-iterations: cut → count → update dict
    for i in $(seq 1 "$SUB_ITERS"); do
        echo "--- Round $round, EM iter $i ---"
        parallel_cut "$OUT/dict.current"
        parallel_count "$OUT/dict.current"
        mv "$OUT/dict.counted" "$OUT/dict.current"
    done

    # Prune: compute loss, keep top words
    echo "--- Round $round, pruning ---"
    parallel_prune "$OUT/dict.current" "$OUT/prune.${round}.txt"
    cp "$OUT/dict.current" "$OUT/dict.${round}.txt"

    # Filter by min_count, compute target size from eligible count, rank by avg_loss/nchar
    # Single characters are always kept to ensure full character coverage.
    # We collect singles from dict.current (not prune.txt) so that chars
    # fully covered by multi-char words in this round are not lost.
    python3 -c "
import sys
singles = set()
for line in open('$OUT/dict.current'):
    w = line.strip().split('\t')[0]
    n = len(w.encode('utf-8')) // 3 if ord(w[0]) > 127 else len(w)
    if n == 1:
        singles.add(w)
items = []
for line in open('$OUT/prune.${round}.txt'):
    p = line.strip().split('\t')
    w, l, c = p[0], float(p[1]), int(p[2])
    if w in singles:
        continue
    if c < $MIN_COUNT:
        continue
    n = len(w.encode('utf-8')) // 3 if ord(w[0]) > 127 else len(w)
    score = l / c / max(n, 1) if c > 0 else 0
    items.append((score, w))
new_size = max($VOCAB_SIZE - len(singles), int(len(items) * 75 / 100))
print(f'singles={len(singles)}, eligible={len(items)}, new_size={new_size + len(singles)}', file=sys.stderr)
items.sort(reverse=True)
with open('$OUT/keep.txt', 'w') as f:
    for w in sorted(singles):
        f.write(w + '\n')
    for _, w in items[:new_size]:
        f.write(w + '\n')
"
    sort "$OUT/keep.txt" -o "$OUT/keep.txt"
    awk -F'\t' 'NR==FNR{keep[$1]=1; next} $1 in keep' \
        "$OUT/keep.txt" "$OUT/dict.current" > "$OUT/dict.pruned"
    # Ensure all single chars from keep.txt are present (freq 0 if unseen)
    awk -F'\t' 'NR==FNR{seen[$1]=1; next} !($1 in seen){print $1"\t0"}' \
        "$OUT/dict.pruned" "$OUT/keep.txt" >> "$OUT/dict.pruned"
    mv "$OUT/dict.pruned" "$OUT/dict.current"

    current_size=$(wc -l < "$OUT/dict.current")
    echo "--- Round $round done, vocab: $current_size ---"
done

# ---------------------------------------------------------------------------
# Final EM to converge with pruned vocab
# ---------------------------------------------------------------------------
echo "=== Final EM ==="
for i in $(seq 1 "$SUB_ITERS"); do
    echo "--- Final EM iter $i ---"
    parallel_cut "$OUT/dict.current"
    parallel_count "$OUT/dict.current"
    mv "$OUT/dict.counted" "$OUT/dict.current"
done

# Clean up
rm -rf "$CHUNKS_DIR" "$OUT/keep.txt"
cp "$OUT/dict.current" "$OUT/dict.txt"
rm -f "$OUT/dict.current"

final_size=$(wc -l < "$OUT/dict.txt")
echo "=== Done. Final dict: $OUT/dict.txt ($final_size words) ==="
