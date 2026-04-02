#!/usr/bin/env python3
"""Process downloaded datasets: t2s conversion, extract plain text, merge into one file."""

from __future__ import annotations

import argparse
import json
import re
import sys
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
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    converter = opencc.OpenCC("t2s")
    total = 0

    with args.output.open("w", encoding="utf-8") as fout:
        for in_name, field in SOURCES:
            in_path = args.input_dir / in_name
            if not in_path.exists():
                print(f"  skipping {in_name} (not found)", file=sys.stderr)
                continue
            count = 0
            with in_path.open("r", encoding="utf-8") as fin:
                for line in fin:
                    line = line.strip()
                    if not line:
                        continue
                    obj = json.loads(line)
                    text = obj.get(field, "")
                    if not text:
                        continue
                    text = clean_text(text)
                    text = converter.convert(text)
                    if text:
                        fout.write(text + "\n")
                        count += 1
            print(f"  {in_name}: {count} lines", file=sys.stderr)
            total += count

    print(f"wrote {total} lines to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
