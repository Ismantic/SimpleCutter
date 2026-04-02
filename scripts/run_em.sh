#!/bin/bash
set -euo pipefail

DICT=${1:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> [rounds=8]}
CORPUS=${2:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> [rounds=8]}
OUT=${3:?usage: run_em.sh <dict_file> <corpus_file> <output_dir> [rounds=8]}
ROUNDS=${4:-8}

ISCUT="$(dirname "$0")/../build/iscut"
mkdir -p "$OUT"

# Cold start: longest match → data.cut.0 → dict.1
echo "=== Cold start ==="
"$ISCUT" --dict "$DICT" --segment "$CORPUS" "$OUT/data.cut.0"
"$ISCUT" --dict "$DICT" --count "$OUT/data.cut.0" "$OUT/dict.1"

# EM rounds: dict.i → data.cut.i → dict.(i+1)
for i in $(seq 1 $((ROUNDS - 1))); do
    echo "=== Round $i ==="
    "$ISCUT" --dict "$OUT/dict.${i}" --cut "$CORPUS" "$OUT/data.cut.${i}"
    "$ISCUT" --dict "$DICT" --count "$OUT/data.cut.${i}" "$OUT/dict.$((i + 1))"

    if [ "$i" -gt 1 ]; then
        prev=$((i - 1))
        changed=$(paste "$OUT/data.cut.${prev}" "$OUT/data.cut.${i}" | awk -F'\t' '$1 != $2 { n++ } END { print n+0 }')
        echo "Changed lines: $changed"
        rm -f "$OUT/data.cut.${prev}"
    fi
done

echo "=== Done. Final: $OUT/dict.${ROUNDS} ==="
