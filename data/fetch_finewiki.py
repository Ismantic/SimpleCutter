#!/usr/bin/env python3
"""Download the Chinese (zhwiki) subset of HuggingFaceFW/finewiki."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from datasets import load_dataset


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download zhwiki from HuggingFaceFW/finewiki."
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("data/zhwiki.jsonl"),
        help="output path (default: data/zhwiki.jsonl)",
    )
    parser.add_argument(
        "--columns",
        nargs="+",
        default=["title", "text"],
        help="columns to keep (default: title text)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)

    print("Loading HuggingFaceFW/finewiki zh split...", file=sys.stderr)
    ds = load_dataset("HuggingFaceFW/finewiki", name="zh", split="train")
    print(f"  {len(ds)} articles", file=sys.stderr)

    if args.columns:
        ds = ds.select_columns(args.columns)

    ds.to_json(str(args.output))
    print(f"wrote {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
