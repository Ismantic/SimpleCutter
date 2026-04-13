#!/usr/bin/env python3
"""Parallel word segmentation using iscut Python package."""

import argparse
import os
import sys
from multiprocessing import Pool

import iscut

_cutter = None
_en = False


def _init_worker(dict_path: str, piece_path: str, en: bool):
    global _cutter, _en
    _cutter = iscut.SemanticCutter(dict_path, piece_path)
    _en = en


def _cut_line(line: str) -> str:
    line = line.rstrip("\n")
    if not line:
        return ""
    return " ".join(_cutter.cut(line, en=_en))


BATCH_SIZE = 4096


def main() -> None:
    parser = argparse.ArgumentParser(description="Parallel segmentation with iscut")
    parser.add_argument("--dict", required=True, help="dictionary file")
    parser.add_argument("--piece", default="", help="piece model file for BPE")
    parser.add_argument("--en", action="store_true", help="use PieceTokenizer for non-Han")
    parser.add_argument("input", help="input text file")
    parser.add_argument("output", help="output segmented file")
    parser.add_argument("--nproc", type=int, default=os.cpu_count(),
                        help="number of processes (default: all cores)")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as fin, \
         open(args.output, "w", encoding="utf-8") as fout, \
         Pool(args.nproc, initializer=_init_worker,
              initargs=(args.dict, args.piece, args.en)) as pool:

        lines_done = 0
        batch = []
        for line in fin:
            batch.append(line)
            if len(batch) >= BATCH_SIZE:
                for result in pool.map(_cut_line, batch):
                    fout.write(result + "\n")
                lines_done += len(batch)
                if lines_done % 500000 < BATCH_SIZE:
                    print(f"  {lines_done} lines", file=sys.stderr)
                batch = []
        if batch:
            for result in pool.map(_cut_line, batch):
                fout.write(result + "\n")
            lines_done += len(batch)

    print(f"done: {lines_done} lines -> {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
