#!/usr/bin/env python3
"""Filter converted wiki word lists into a final lexicon."""

from __future__ import annotations

import sys
from pathlib import Path

ZH_RANGES = (
    (0x3400, 0x4DBF),
    (0x4E00, 0x9FFF),
    (0xF900, 0xFAFF),
    (0x20000, 0x2A6DF),
    (0x2A700, 0x2B73F),
    (0x2B740, 0x2B81F),
    (0x2B820, 0x2CEAF),
    (0x2CEB0, 0x2EBEF),
    (0x30000, 0x3134F),
    (0x31350, 0x323AF),
)


def is_chinese_char(ch: str) -> bool:
    if ch == "〇":
        return True
    code = ord(ch)
    return any(s <= code <= e for s, e in ZH_RANGES)


def is_pure_chinese(word: str) -> bool:
    return bool(word) and all(is_chinese_char(ch) for ch in word)


def read_lines(path: Path) -> list[str]:
    with path.open("r", encoding="utf-8") as f:
        return [line.strip() for line in f if line.strip()]


def filter_simple(path: Path, max_len: int) -> set[str]:
    words: set[str] = set()
    for word in read_lines(path):
        if is_pure_chinese(word) and len(word) <= max_len:
            words.add(word)
    return words


def filter_zhwiki(path: Path) -> set[str]:
    words: set[str] = set()
    for word in read_lines(path):
        if "·" in word:
            for part in word.split("·"):
                if is_pure_chinese(part) and len(part) <= 4:
                    words.add(part)
        elif is_pure_chinese(word) and len(word) <= 3:
            words.add(word)
    return words


def filter_redirects(path: Path) -> set[str]:
    words: set[str] = set()
    for line in read_lines(path):
        parts = line.split("\t")
        for part in parts:
            part = part.strip()
            if is_pure_chinese(part) and len(part) <= 3:
                words.add(part)
    return words


def main() -> None:
    conv_dir = Path(__file__).parent

    all_words: set[str] = set()

    sources = [
        ("zhwiki.raw", filter_zhwiki),
        ("zhwiktionary.raw", lambda p: filter_simple(p, 4)),
        ("zhwikisource.raw", lambda p: filter_simple(p, 3)),
        ("web-slang.raw", lambda p: filter_simple(p, 3)),
        ("zhwiki-redirects.txt", filter_redirects),
    ]

    for filename, fn in sources:
        path = conv_dir / filename
        if not path.exists():
            print(f"  skipping {filename} (not found)", file=sys.stderr)
            continue
        words = fn(path)
        print(f"  {filename}: {len(words)} words", file=sys.stderr)
        all_words.update(words)

    # Add constituent single chars from all multi-char words
    single_chars: set[str] = set()
    for word in all_words:
        if len(word) > 1:
            for ch in word:
                single_chars.add(ch)
    new_chars = single_chars - all_words
    all_words.update(new_chars)

    for word in sorted(all_words):
        print(word)

    print(f"total: {len(all_words)} words ({len(new_chars)} single chars added)", file=sys.stderr)


if __name__ == "__main__":
    main()
