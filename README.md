# IsmaCut

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

准备词频词典（`词\t词频`），启动 REPL：

```bash
./build/ismacut dict.txt
```

```
> 南京市长江大桥
南京市/长江大桥
```

### Pipe 模式

从 stdin 逐行读入，分词后输出到 stdout（空格分隔）：

```bash
./build/ismacut --dict dict.txt --pipe < input.txt > output.txt
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
# 冷启动
ismacut --dict words.txt --segment corpus.txt output/seg_cold.txt
ismacut --dict words.txt --count output/seg_cold.txt output/freq_cold.txt

# EM 迭代（3-5 轮即可收敛）
ismacut --dict output/freq_cold.txt --pipe < corpus.txt > output/seg_r0.txt
ismacut --dict words.txt --count output/seg_r0.txt output/freq_r0.txt

ismacut --dict output/freq_r0.txt --pipe < corpus.txt > output/seg_r1.txt
ismacut --dict words.txt --count output/seg_r1.txt output/freq_r1.txt
# ...
```

也可以直接用训练脚本：

```bash
scripts/train.sh words.txt corpus.txt 5
```

### 内置测试

```bash
./build/ismacut
```

### Python

```bash
uv pip install .
```

```python
import ismacut

cutter = ismacut.Cutter("dict.txt")
result = cutter.cut("南京市长江大桥")
print(result)  # ['南京市', '长江大桥']
```

## 项目结构

```
src/
  trie.h       - Double-Array Trie（构建、查找、前缀搜索）
  ustr.h/cc    - UTF-8 工具（字符长度、合法性校验）
  cut.h/cc     - DAG+DP 分词器
  segment.h/cc - 正向最长匹配分词（冷启动用）
  count.h/cc   - 词频统计
  main.cc      - CLI 入口（REPL / pipe / segment / count）
  pip.cc       - pybind11 Python 绑定
scripts/
  train.sh     - EM 词频学习脚本
python/
  ismacut/     - Python 包入口
```

## License

MIT
