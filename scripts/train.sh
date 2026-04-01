#!/bin/bash
set -euo pipefail

DICT=${1:?usage: train.sh <dict_file> <corpus_file> [rounds=5]}
CORPUS=${2:?usage: train.sh <dict_file> <corpus_file> [rounds=5]}
ROUNDS=${3:-5}

ISCUT="$(dirname "$0")/../build/iscut"
OUT="$(dirname "$0")/../output"
mkdir -p "$OUT"

echo "=== Cold start: longest match ==="
"$ISCUT" --dict "$DICT" --segment "$CORPUS" "$OUT/seg_cold.txt"
"$ISCUT" --dict "$DICT" --count "$OUT/seg_cold.txt" "$OUT/freq_cold.txt"
rm -f "$OUT/seg_cold.txt"

prev_freq="$OUT/freq_cold.txt"
for i in $(seq 0 $((ROUNDS - 1))); do
    echo "=== Round $i ==="
    "$ISCUT" --dict "$prev_freq" --cut "$CORPUS" "$OUT/seg_r${i}.txt"
    "$ISCUT" --dict "$DICT" --count "$OUT/seg_r${i}.txt" "$OUT/freq_r${i}.txt"

    if [ "$i" -gt 0 ]; then
        prev=$((i - 1))
        changed=$(paste "$OUT/seg_r${prev}.txt" "$OUT/seg_r${i}.txt" | awk -F'\t' '$1 != $2 { n++ } END { print n+0 }')
        echo "Changed lines: $changed"
        rm -f "$OUT/seg_r${prev}.txt"
    fi

    prev_freq="$OUT/freq_r${i}.txt"
done

echo "=== Done. Final freq: $prev_freq ==="
