# Dictionary Build

This directory contains lexicon-building utilities for `Iscut`.

## Seed Lexicon

`build_lexicon.py` merges three source layers into a single seed lexicon:

- Base: `chinese-xinhua`
- Extension: `zhwiktionary.raw`, `zhwiki.raw`, `zhwikisource.raw`
- Alias: `zhwiki-redirects.txt`

The filtering rules are intentionally strict:

- 1-character words: keep
- 2-character words: keep
- 3-character words: keep
- 4-character words: keep only if they are in the Xinhua idiom list
- 5+ character words: drop
- Every character in every kept word must appear in `corpus_all.txt` at least `2` times by default

The script writes a plain word list, one word per line. Frequency learning is handled later by the existing `Iscut` training flow.

## Usage

```bash
python3 dict/build_lexicon.py
```

This also writes a character-frequency table to `dict/generated/char_freq.txt`.

For large corpora, use the C++ counter once and then reuse the result:

```bash
g++ -O3 -std=c++17 -Isrc dict/count_chars.cc src/ustr.cc -o build/count_chars
./build/count_chars corpus_all.txt dict/generated/char_freq.txt
python3 dict/build_lexicon.py --char-freq-input dict/generated/char_freq.txt
```

Custom source paths:

```bash
python3 dict/build_lexicon.py \
  --xinhua-dir /path/to/chinese-xinhua \
  --wiki-dir /path/to/fcitx5-pinyin-zhwiki \
  --output dict/generated/iscut_dict_raw.txt \
  --corpus corpus_all.txt \
  --min-char-freq 2
```

If `opencc` is installed, the script converts source words to simplified Chinese. Use `--disable-opencc` to skip that step.

## TODO

- Proper names need separate handling. The current rule drops most 4+ character entries, but valid person names such as `莎士比亚` should likely be preserved by a dedicated name rule or whitelist step later.
