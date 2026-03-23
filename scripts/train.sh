#!/bin/bash
set -euo pipefail

DICT=${1:?usage: train.sh <dict_file> <corpus_file> [rounds=5]}
CORPUS=${2:?usage: train.sh <dict_file> <corpus_file> [rounds=5]}
ROUNDS=${3:-5}

ISMACUT="$(dirname "$0")/../build/ismacut"
OUT="$(dirname "$0")/../output"
mkdir -p "$OUT"

echo "=== Cold start: longest match ==="
"$ISMACUT" --dict "$DICT" --segment "$CORPUS" "$OUT/seg_cold.txt"
"$ISMACUT" --dict "$DICT" --count "$OUT/seg_cold.txt" "$OUT/freq_cold.txt"

prev_freq="$OUT/freq_cold.txt"
for i in $(seq 0 $((ROUNDS - 1))); do
    echo "=== Round $i ==="
    "$ISMACUT" --dict "$prev_freq" --pipe < "$CORPUS" > "$OUT/seg_r${i}.txt"
    "$ISMACUT" --dict "$DICT" --count "$OUT/seg_r${i}.txt" "$OUT/freq_r${i}.txt"

    if [ "$i" -gt 0 ]; then
        prev=$((i - 1))
        changed=$(diff "$OUT/seg_r${prev}.txt" "$OUT/seg_r${i}.txt" | grep -c '^[<>]' || true)
        echo "Changed lines: $changed"
    fi

    prev_freq="$OUT/freq_r${i}.txt"
done

echo "=== Done. Final freq: $prev_freq ==="
