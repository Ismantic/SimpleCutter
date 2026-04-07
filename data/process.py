#!/usr/bin/env python3
"""Process downloaded datasets: t2s conversion, extract plain text, merge into one file."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from multiprocessing import Pool
from pathlib import Path

import opencc

SOURCES = [
    ("zhwiki.jsonl", "text"),
    ("zh_wikinews.jsonl", "text"),
]


def strip_markdown_headings(text: str) -> str:
    return re.sub(r"^#+\s+.*$", "", text, flags=re.MULTILINE)


def clean_text(text: str) -> str:
    text = strip_markdown_headings(text)
    text = text.strip()
    text = re.sub(r"\s+", " ", text)
    return text


_converter = None


def _init_worker():
    global _converter
    _converter = opencc.OpenCC("t2s")


def _process_line(line: str) -> str | None:
    line = line.strip()
    if not line:
        return None
    obj = json.loads(line)
    text = obj.get("text", "")
    if not text:
        return None
    text = clean_text(text)
    text = _converter.convert(text)
    return text if text else None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Process jsonl datasets into plain text with t2s conversion."
    )
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=Path("data"),
        help="directory containing downloaded jsonl files",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("data.txt"),
        help="output file (default: data.txt)",
    )
    parser.add_argument(
        "--nproc",
        type=int,
        default=4,
        help="number of worker processes (default: all cores)",
    )
    return parser.parse_args()


BATCH_SIZE = 1024


def main() -> None:
    args = parse_args()
    total = 0

    with args.output.open("w", encoding="utf-8") as fout, \
         Pool(args.nproc, initializer=_init_worker) as pool:
        for in_name, field in SOURCES:
            in_path = args.input_dir / in_name
            if not in_path.exists():
                print(f"  skipping {in_name} (not found)", file=sys.stderr)
                continue
            count = 0
            with in_path.open("r", encoding="utf-8") as fin:
                batch = []
                for line in fin:
                    batch.append(line)
                    if len(batch) >= BATCH_SIZE:
                        for result in pool.map(_process_line, batch):
                            if result:
                                fout.write(result + "\n")
                                count += 1
                        batch = []
                if batch:
                    for result in pool.map(_process_line, batch):
                        if result:
                            fout.write(result + "\n")
                            count += 1
            print(f"  {in_name}: {count} lines", file=sys.stderr)
            total += count

    print(f"wrote {total} lines to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
