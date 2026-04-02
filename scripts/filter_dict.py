#!/usr/bin/env python3
"""Filter a word list by character frequency: drop words containing rare chars."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def load_char_freq(path: Path) -> dict[str, int]:
    freq: dict[str, int] = {}
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or "\t" not in line:
                continue
            ch, count = line.split("\t", 1)
            freq[ch] = int(count)
    return freq


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Filter word list by character frequency."
    )
    parser.add_argument("char_freq", type=Path, help="char frequency file (char\\tcount)")
    parser.add_argument("dict_in", type=Path, help="input word list (one word per line)")
    parser.add_argument("dict_out", type=Path, help="output filtered word list")
    parser.add_argument("--min-freq", type=int, default=2, help="minimum char frequency (default: 2)")
    args = parser.parse_args()

    freq = load_char_freq(args.char_freq)
    kept = 0
    total = 0

    with args.dict_in.open("r", encoding="utf-8") as fin, \
         args.dict_out.open("w", encoding="utf-8") as fout:
        for line in fin:
            word = line.rstrip("\n")
            if not word:
                continue
            total += 1
            if all(freq.get(ch, 0) >= args.min_freq for ch in word):
                fout.write(word + "\n")
                kept += 1

        # Add single chars with freq >= min_freq
        added = 0
        for ch, cnt in freq.items():
            if cnt >= args.min_freq and len(ch) == 1 and '\u4e00' <= ch <= '\u9fff':
                fout.write(ch + "\n")
                added += 1

    print(f"{kept}/{total} words kept, {added} single chars added (min_freq={args.min_freq})", file=sys.stderr)


if __name__ == "__main__":
    main()
