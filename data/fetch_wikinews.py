#!/usr/bin/env python3
"""Download Chinese Wikinews from bigscience-data/roots_zh_wikinews."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from datasets import load_dataset


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download bigscience-data/roots_zh_wikinews."
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("data/zh_wikinews.jsonl"),
        help="output path (default: data/zh_wikinews.jsonl)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    print("Loading bigscience-data/roots_zh_wikinews...", file=sys.stderr)
    ds = load_dataset("bigscience-data/roots_zh_wikinews", split="train")
    print(f"  {len(ds)} documents", file=sys.stderr)

    ds.to_json(str(args.output))
    print(f"wrote {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
