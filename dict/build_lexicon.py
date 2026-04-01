#!/usr/bin/env python3
"""Build an Iscut seed lexicon from Xinhua and Chinese Wikipedia sources."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Iterable

try:
    import opencc  # type: ignore
except ImportError:  # pragma: no cover
    opencc = None


DEFAULT_XINHUA_DIR = Path("/home/tfbao/new/chinese-xinhua")
DEFAULT_WIKI_DIR = Path("/home/tfbao/new/fcitx5-pinyin-zhwiki")

ZH_REANGES = (
    (0x3400, 0x4DBF),   # CJK Extension A
    (0x4E00, 0x9FFF),   # CJK Unified Ideographs
    (0xF900, 0xFAFF),   # CJK Compatibility Ideographs
    (0x20000, 0x2A6DF), # CJK Extension B
    (0x2A700, 0x2B73F), # CJK Extension C
    (0x2B740, 0x2B81F), # CJK Extension D
    (0x2B820, 0x2CEAF), # CJK Extension E/F
    (0x2CEB0, 0x2EBEF), # CJK Extension G/I
    (0x30000, 0x3134F), # CJK Extension G
    (0x31350, 0x323AF), # CJK Extension H
)

DROP_SUBSTRINGS = (
    "消歧义",
    "消歧義",
    "列表",
    "对照表",
    "對照表",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build a seed lexicon for Iscut from curated Chinese sources."
    )
    parser.add_argument(
        "--xinhua-dir",
        type=Path,
        default=DEFAULT_XINHUA_DIR,
        help=f"path to chinese-xinhua repo (default: {DEFAULT_XINHUA_DIR})",
    )
    parser.add_argument(
        "--wiki-dir",
        type=Path,
        default=DEFAULT_WIKI_DIR,
        help=f"path to fcitx5-pinyin-zhwiki repo (default: {DEFAULT_WIKI_DIR})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("dict/generated/iscut_dict_raw.txt"),
        help="output lexicon path",
    )
    parser.add_argument(
        "--corpus",
        type=Path,
        default=Path("corpus_all.txt"),
        help="corpus used to compute character frequencies",
    )
    parser.add_argument(
        "--char-freq-output",
        type=Path,
        default=Path("dict/generated/char_freq.txt"),
        help="where to write the computed character frequencies",
    )
    parser.add_argument(
        "--char-freq-input",
        type=Path,
        help="optional precomputed character-frequency file to reuse",
    )
    parser.add_argument(
        "--min-char-freq",
        type=int,
        default=2,
        help="minimum corpus frequency required for every character in a kept word",
    )
    parser.add_argument(
        "--disable-opencc",
        action="store_true",
        help="skip traditional-to-simplified conversion even if OpenCC is installed",
    )
    return parser.parse_args()


def build_converter(disable_opencc: bool):
    if disable_opencc or opencc is None:
        return None
    return opencc.OpenCC("t2s.json")


def to_simplified(text: str, converter) -> str:
    text = text.strip()
    if not text:
        return ""
    return converter.convert(text) if converter is not None else text


def is_chinese_char(ch: str) -> bool:
    if ch == "〇":
        return True
    code = ord(ch)
    return any(start <= code <= end for start, end in ZH_REANGES)


def is_pure_chinese(word: str) -> bool:
    return bool(word) and all(is_chinese_char(ch) for ch in word)


def is_good_candidate(word: str) -> bool:
    if not is_pure_chinese(word):
        return False
    if any(token in word for token in DROP_SUBSTRINGS):
        return False
    return True


def keep_word(word: str, idioms: set[str]) -> bool:
    if not is_good_candidate(word):
        return False
    n = len(word)
    if n <= 3:
        return True
    if n == 4:
        return word in idioms
    return False


def count_chars_in_corpus(path: Path) -> Counter[str]:
    counts: Counter[str] = Counter()
    with path.open("r", encoding="utf-8") as fh:
        for line in fh:
            for ch in line.rstrip("\n"):
                if is_chinese_char(ch):
                    counts[ch] += 1
    return counts


def passes_char_freq(word: str, char_freq: Counter[str], min_char_freq: int) -> bool:
    return all(char_freq.get(ch, 0) >= min_char_freq for ch in word)


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def load_xinhua_words(base_dir: Path, converter) -> tuple[set[str], Counter]:
    stats: Counter[str] = Counter()
    words: set[str] = set()

    for item in load_json(base_dir / "data" / "word.json"):
        word = to_simplified(item.get("word", ""), converter)
        if keep_word(word, set()):
            words.add(word)
            stats["xinhua_word"] += 1

    for item in load_json(base_dir / "data" / "ci.json"):
        word = to_simplified(item.get("ci", ""), converter)
        if keep_word(word, set()):
            words.add(word)
            stats["xinhua_ci"] += 1

    return words, stats


def load_xinhua_idioms(base_dir: Path, converter) -> tuple[set[str], Counter]:
    stats: Counter[str] = Counter()
    idioms: set[str] = set()
    for item in load_json(base_dir / "data" / "idiom.json"):
        word = to_simplified(item.get("word", ""), converter)
        if is_good_candidate(word) and len(word) == 4:
            idioms.add(word)
            stats["xinhua_idiom"] += 1
    return idioms, stats


def iter_wiki_raw_words(path: Path) -> Iterable[str]:
    with path.open("r", encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line:
                continue
            yield line.split("\t", 1)[0].strip()


def load_wiki_raw(path: Path, idioms: set[str], converter, stat_key: str) -> tuple[set[str], Counter]:
    stats: Counter[str] = Counter()
    words: set[str] = set()
    for raw_word in iter_wiki_raw_words(path):
        word = to_simplified(raw_word, converter)
        if keep_word(word, idioms):
            words.add(word)
            stats[stat_key] += 1
    return words, stats


def load_redirects(path: Path, idioms: set[str], converter) -> tuple[set[str], Counter]:
    stats: Counter[str] = Counter()
    words: set[str] = set()
    with path.open("r", encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line or "\t" not in line:
                continue
            source, _target = line.split("\t", 1)
            word = to_simplified(source, converter)
            if keep_word(word, idioms):
                words.add(word)
                stats["wiki_redirect"] += 1
    return words, stats


def write_output(path: Path, words: Iterable[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        for word in sorted(set(words)):
            fh.write(f"{word}\n")


def write_char_freq(path: Path, char_freq: Counter[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as fh:
        for ch, freq in sorted(char_freq.items(), key=lambda item: (-item[1], item[0])):
            fh.write(f"{ch}\t{freq}\n")


def load_char_freq(path: Path) -> Counter[str]:
    counts: Counter[str] = Counter()
    with path.open("r", encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line or "\t" not in line:
                continue
            ch, freq = line.split("\t", 1)
            counts[ch] = int(freq)
    return counts


def main() -> None:
    args = parse_args()
    converter = build_converter(args.disable_opencc)
    if args.char_freq_input:
        char_freq = load_char_freq(args.char_freq_input)
    else:
        char_freq = count_chars_in_corpus(args.corpus)

    idioms, idiom_stats = load_xinhua_idioms(args.xinhua_dir, converter)
    xinhua_words, xinhua_stats = load_xinhua_words(args.xinhua_dir, converter)

    wiki_words: set[str] = set()
    wiki_stats: Counter[str] = Counter()

    for filename, key in (
        ("zhwiktionary.raw", "wiki_zhwiktionary"),
        ("zhwiki.raw", "wiki_zhwiki"),
        ("zhwikisource.raw", "wiki_zhwikisource"),
    ):
        words, stats = load_wiki_raw(args.wiki_dir / filename, idioms, converter, key)
        wiki_words.update(words)
        wiki_stats.update(stats)

    redirect_words, redirect_stats = load_redirects(
        args.wiki_dir / "zhwiki-redirects.txt", idioms, converter
    )

    final_words = {
        word
        for word in (xinhua_words | idioms | wiki_words | redirect_words)
        if passes_char_freq(word, char_freq, args.min_char_freq)
    }
    write_output(args.output, final_words)
    write_char_freq(args.char_freq_output, char_freq)

    all_stats = Counter()
    all_stats.update(idiom_stats)
    all_stats.update(xinhua_stats)
    all_stats.update(wiki_stats)
    all_stats.update(redirect_stats)

    print(f"wrote {len(final_words)} words to {args.output}")
    print(f"wrote {len(char_freq)} chars to {args.char_freq_output}")
    print(f"min_char_freq: {args.min_char_freq}")
    for key in sorted(all_stats):
        print(f"{key}: {all_stats[key]}")


if __name__ == "__main__":
    main()
