#!/usr/bin/env python3
"""将每行一个词条的语料拆成每行一个句子

按 。！？ 分句，保留句末标点。跳过空句。
"""
import re
import sys

input_file = sys.argv[1] if len(sys.argv) > 1 else "data.txt"
output_file = sys.argv[2] if len(sys.argv) > 2 else "sentences.txt"

# 按句末标点分割，保留标点
pattern = re.compile(r'([。！？])')

lines = 0
sentences = 0

with open(input_file, "r") as fin, open(output_file, "w") as fout:
    for line in fin:
        line = line.rstrip("\n")
        if not line:
            continue
        lines += 1

        parts = pattern.split(line)
        # parts: [text, punct, text, punct, ..., text]
        i = 0
        while i < len(parts):
            seg = parts[i]
            # 把标点粘回前面的文本
            if i + 1 < len(parts) and pattern.match(parts[i + 1]):
                seg += parts[i + 1]
                i += 2
            else:
                i += 1

            seg = seg.strip()
            if seg:
                fout.write(seg + "\n")
                sentences += 1

        if lines % 200000 == 0:
            print(f"{lines} articles, {sentences} sentences", file=sys.stderr)

print(f"done: {lines} articles → {sentences} sentences", file=sys.stderr)
