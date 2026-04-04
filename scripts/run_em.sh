#!/bin/bash
set -euo pipefail

DICT=${1:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> <vocab_size> [sub_iters=2]}
CORPUS=${2:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> <vocab_size> [sub_iters=2]}
OUT=${3:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> <vocab_size> [sub_iters=2]}
VOCAB_SIZE=${4:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> <vocab_size> [sub_iters=2]}
SUB_ITERS=${5:-2}

ISCUT="$(dirname "$0")/../build/iscut"
mkdir -p "$OUT"

# Cold start: longest match → count → initial dict
echo "=== Cold start ==="
"$ISCUT" --dict "$DICT" --segment "$CORPUS" "$OUT/data.cut"
"$ISCUT" --dict "$DICT" --count "$OUT/data.cut" "$OUT/dict.current"

current_size=$(wc -l < "$OUT/dict.current")
echo "Initial vocab: $current_size, target: $VOCAB_SIZE"

round=0
while [ "$current_size" -gt "$VOCAB_SIZE" ]; do
    round=$((round + 1))

    # EM sub-iterations: cut → count → update dict
    for i in $(seq 1 "$SUB_ITERS"); do
        echo "--- Round $round, EM iter $i ---"
        "$ISCUT" --dict "$OUT/dict.current" --cut "$CORPUS" "$OUT/data.cut"
        "$ISCUT" --dict "$OUT/dict.current" --count "$OUT/data.cut" "$OUT/dict.new"
        mv "$OUT/dict.new" "$OUT/dict.current"
    done

    # Prune: compute loss, keep top words
    new_size=$(python3 -c "print(max($VOCAB_SIZE, int($current_size * 75 / 100)))")
    echo "--- Round $round, pruning: $current_size → $new_size ---"
    "$ISCUT" --dict "$OUT/dict.current" --prune "$CORPUS" "$OUT/prune.${round}.txt"
    cp "$OUT/dict.current" "$OUT/dict.${round}.txt"

    # Re-rank by avg_loss/nchar, take top new_size words
    python3 -c "
import sys
items = []
for line in open('$OUT/prune.${round}.txt'):
    p = line.strip().split('\t')
    w, l, c = p[0], float(p[1]), int(p[2])
    n = len(w.encode('utf-8')) // 3 if ord(w[0]) > 127 else len(w)
    score = l / c / max(n, 1) if c > 0 else 0
    items.append((score, w))
items.sort(reverse=True)
with open('$OUT/keep.txt', 'w') as f:
    for _, w in items[:$new_size]:
        f.write(w + '\n')
"
    sort "$OUT/keep.txt" -o "$OUT/keep.txt"
    awk -F'\t' 'NR==FNR{keep[$1]=1; next} $1 in keep' \
        "$OUT/keep.txt" "$OUT/dict.current" > "$OUT/dict.pruned"
    mv "$OUT/dict.pruned" "$OUT/dict.current"

    current_size=$(wc -l < "$OUT/dict.current")
    echo "--- Round $round done, vocab: $current_size ---"
done

# Final EM to converge with pruned vocab
echo "=== Final EM ==="
for i in $(seq 1 "$SUB_ITERS"); do
    echo "--- Final EM iter $i ---"
    "$ISCUT" --dict "$OUT/dict.current" --cut "$CORPUS" "$OUT/data.cut"
    "$ISCUT" --dict "$OUT/dict.current" --count "$OUT/data.cut" "$OUT/dict.new"
    mv "$OUT/dict.new" "$OUT/dict.current"
done

# Clean up temp files
rm -f "$OUT/data.cut" "$OUT/keep.txt"
cp "$OUT/dict.current" "$OUT/dict.txt"
rm -f "$OUT/dict.current"

final_size=$(wc -l < "$OUT/dict.txt")
echo "=== Done. Final dict: $OUT/dict.txt ($final_size words) ==="
