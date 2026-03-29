# Iscut (Is Semantic Cutter) 是语切词

想办法构建一个词表，找一大批中文语料，就能构建一个还不错的分词器。
基于 Double-Array Trie 的中文分词工具。支持 DAG+DP 分词和基于 EM 迭代的无监督词频学习。

## 特性

- **DAG + 动态规划分词** — 前缀搜索构建 DAG，后向 DP 求最大概率路径
- **EM 词频自举** — 给定词典和生语料，无需标注数据即可学出词频
- **冷启动分词** — 正向最长匹配，用于 EM 的初始化
- **Double-Array Trie** — XOR 索引，支持精确查找和公共前缀搜索
- **纯 C++17** — 无外部依赖，< 1000 行

## 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 使用

### 交互模式

使用内置词典启动 REPL：

```bash
./build/ismacut dict/dict.txt
./build/ismacut --dict dict/dict.txt
```

```
> 南京市长江大桥
南京市/长江/大桥
```

### Pipe 模式

从 stdin 逐行读入，分词后输出到 stdout（空格分隔）：

```bash
./build/ismacut --dict dict/dict.txt --pipe < input.txt > output.txt
```

### Cut 模式

从文件读入，DP 分词后写入文件：

```bash
./build/ismacut --dict dict/dict.txt --cut input.txt output.txt
```

### 冷启动分词（最长匹配）

用词典做正向最长匹配，无需词频：

```bash
./build/ismacut --dict words.txt --segment input.txt output.txt
```

### 词频统计

统计分词结果中的词频，可用 `--dict` 过滤只保留词典内的词：

```bash
./build/ismacut --count input.txt freq.txt
./build/ismacut --dict words.txt --count input.txt freq.txt
```

### EM 词频学习

给定词典和生语料，通过冷启动 → 统计 → DP 分词 → 统计的迭代学出词频：

```bash
scripts/train.sh words.txt corpus.txt 5
```

### Python

```bash
uv pip install .
```

```python
import ismacut

cutter = ismacut.Cutter("dict/dict.txt")
result = cutter.cut("南京市长江大桥")
print(result)  # ['南京市', '长江', '大桥']
```

### 内置测试

```bash
./build/ismacut
```

## 项目结构

```
dict/
  dict.txt     - 内置词频词典（词\t频次，274,506 词条）
src/
  trie.h       - Double-Array Trie（构建、查找、前缀搜索）
  ustr.h/cc    - UTF-8 工具（字符长度、合法性校验）
  cut.h/cc     - DAG+DP 分词器
  segment.h/cc - 正向最长匹配分词（冷启动用）
  count.h/cc   - 词频统计
  main.cc      - CLI 入口（REPL / pipe / cut / segment / count）
  pip.cc       - pybind11 Python 绑定
scripts/
  train.sh     - EM 词频学习脚本
python/
  ismacut/     - Python 包入口
```

## License

MIT
