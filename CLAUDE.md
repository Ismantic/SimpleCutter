# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Iscut (Is Semantic Cutter / 是语切词) is a Chinese word segmentation tool. It uses a Double-Array Trie for dictionary lookup, DAG + dynamic programming for optimal segmentation, and an EM algorithm for learning word frequencies from raw corpus data. Pure C++17, no external dependencies.

## Build & Run

```bash
# Build C++ binary
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests (no CMake target — compile manually; needs piece.cc for MixCutter symbols)
cmake --build build && g++ -std=c++17 -Isrc src/test.cc src/cut.cc src/ustr.cc src/piece.cc -o build/iscut-test && ./build/iscut-test

# Install Python package (requires pybind11)
uv pip install .

# Interactive mode
./build/iscut --dict dict.txt

# Pipe mode (stdin → stdout)
./build/iscut --dict dict.txt --pipe < input.txt > output.txt

# MixCutter mode (Chinese + English)
./build/iscut --dict dict.txt --piece-model piece.txt --semantic --cn --en
```

## Training Pipeline

Full pipeline to retrain the dictionary from scratch:

1. **Build dictionary** (`cd dict && make`) — downloads Wikimedia title dumps, converts traditional→simplified, filters by length rules
2. **Fetch corpus** (`cd data && make`) — downloads Chinese Wikipedia/Wikinews from HuggingFace, converts to simplified, splits into sentences
3. **Train** (`cd scripts && make`) — character frequency counting, dict filtering, EM training with pruning

Training requires: `datasets`, `huggingface_hub[cli]`, `opencc` Python packages. Control vocab size with `make VOCAB_SIZE=240000 SUB_ITERS=2` (default is 240000).

## Architecture

### Core C++ (`src/`)

- **`trie.h`** — `trie::DoubleArray<T>`: XOR-indexed double-array trie. Requires sorted input. Supports exact lookup (`GetUnit`) and common prefix search (`PrefixSearch`). Header-only.
- **`cut.h/cc`** — `cut::Cutter`: the main segmenter. Builds DAG via trie prefix search, runs backward DP to find max log-probability path. `CutWithLoss` computes per-word deletion loss for pruning. Also defines `cut::MixCutter`.
- **`segment.h/cc`** — `cut::Segmenter`: forward longest-match segmenter (no frequencies). Used for EM cold start.
- **`ustr.h/cc`** — UTF-8 utilities: char length, punctuation detection, `SplitByPunct` for pre-segmentation, `SplitByHan` for Han/non-Han splitting.
- **`count.h/cc`** — `cut::Counter`: word frequency accumulator.
- **`piece.h/cc`** — `cut::PieceTokenizer`: BPE tokenizer for non-Chinese text (English). Loads a `piece.txt` vocab, applies Unicode normalization and BPE merge rules.
- **`main.cc`** — CLI with modes: `--pipe`, `--segment`, `--cut`, `--count`, `--prune`, `--piece`, `--semantic`, or REPL.
- **`pip.cc`** — pybind11 wrapper exposing `Cutter` and `MixCutter` to Python (built via scikit-build-core when `SKBUILD` is set).
- **`test.cc`** — Smoke tests. Not wired into CMake; compile manually (see Build & Run).

### MixCutter (mixed-language segmentation)

`cut::MixCutter` handles text containing both Chinese and non-Chinese. `Cut(sentence, cn, en)` splits by Han/non-Han via `SplitByHan`, then:

- **Han + `cn=true`**: Cutter (DAG+DP) segmentation
- **Han + `cn=false`**: split into individual characters
- **non-Han + `en=true`**: `PieceTokenizer::Tokenize` (uses piece.txt settings for cut/reconstruct, then BPE)
- **non-Han + `en=false`**: `PieceTokenizer::PreTokenize` (SplitText with cut=1, no BPE)

`PieceTokenizer` is self-contained — `Tokenize()` uses settings from piece.txt (`cut`, `reconstruct`), no external override. `PreTokenize(text, cut=1)` is a separate entry point for SplitText-only use.

### Key design patterns

- **Punctuation pre-splitting**: `Cut()` first splits input by punctuation via `SplitByPunct`, then segments each non-punct span independently, preserving punctuation in output.
- **Han detection scope**: `IsHan` covers CJK unified ideographs, CJK symbols/punctuation, fullwidth forms, and extension blocks. CJK punctuation (U+3000–U+303F) is classified as both Han and punctuation — `SplitByPunct` runs first so punctuation is stripped before `SplitByHan` sees it.
- **Dictionary format**: TSV with `word\tfrequency`. Single-character entries are included (they serve as fallback for unknown words).
- **EM loop** (`scripts/run_em.sh`): cold start (longest match) → iterative cut/count → prune by per-word loss normalized by character count → final EM convergence.

## Coding Style

- C++17, standard library only — no external dependencies for the core.
- 4-space indentation in C++ and Python.
- `PascalCase` for classes and public methods (`BuildCutter`, `Cutter`, `Cut`, `SplitByPunct`), `snake_case` for Python functions, scripts, and Make targets (`filter_dict.py`, `count_chars`).
- Prefer small translation units and file-local helpers over heavy abstractions.

## Commit Style

Short, imperative subjects. Make them specific enough to identify the touched area, e.g. `scripts: tighten dict filter`. Include sample CLI output when segmentation behavior changes.

## Dict files at root

- `dict.txt` — current default dictionary (~120k words)
- `dict.12w.txt`, `dict.24w.txt` — alternative sizes (12万/24万 words)
- `piece.txt` — BPE vocabulary (~43k pieces) used by `PieceTokenizer`/`MixCutter` for non-Chinese text
