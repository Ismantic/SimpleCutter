# IsCut (Is Semantic Cutter) 是语切词

想办法构建一个词表，找一大批中文语料，就能构建一个还不错的分词器。

## 特性

- **DAG + 动态规划分词** — 前缀搜索构建 DAG，后向 DP 求最大概率路径
- **SemanticCutter** — 中英混合文本分流切分：中文走 Unigram 分词，英文走 BPE 分词
- **EM 词频自举** — 给定词典和生语料，无需标注数据即可学出词频
- **冷启动分词** — 正向最长匹配，用于 EM 的初始化
- **Double-Array Trie** — XOR 索引，支持精确查找和公共前缀搜索
- **完整可复现** — 从数据获取到词表训练全流程自动化

## 使用

### 编译

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### SemanticCutter（中英混合切分）

```bash
# 交互模式：cn=中文分词，en=英文BPE分词
./build/iscut --dict dict.txt --piece-model piece.txt --semantic --cn --en
```

```
> 南京市长江大桥and Natural Language Processing技术
南京市/长江/大桥/and/Natural/Language/Processing/技术
```

切分规则：
- 第一层：按 Han/non-Han 分割（CJK 标点归入 Han）
- `--cn`：Han 段用 NaiveCutter（DAG+DP）分词，关闭则拆成单字
- `--en`：non-Han 段先做 GPT-4 风格预分割，再用 PieceTokenizer（BPE）分词，关闭则拆成单字

### NaiveCutter（纯中文分词）

```bash
# 交互模式
./build/iscut --dict dict.txt

# Pipe 模式
./build/iscut --dict dict.txt --pipe < input.txt > output.txt
```

```
> 南京市长江大桥
南京市/长江/大桥
```

### Python

```bash
uv pip install .
```

```python
import iscut

# SemanticCutter：中英混合
sc = iscut.SemanticCutter("dict.txt", "piece.txt")
sc.cut("Hello世界", cn=True, en=True)  # ['Hell', 'o', '世界']

# Cutter：纯中文
cutter = iscut.Cutter("dict.txt")
cutter.cut("南京市长江大桥")  # ['南京市', '长江', '大桥']
```

## 训练过程

以下通过维基百科数据来说明从数据获取到词频训练的完全过程。

### 1. 获取词表

从中文维基百科、维基词典、维基文库、网络用语中提取候选词表：

```bash
cd dict
make download   # 下载 Wikimedia title dump 和 SQL dump
make convert    # 繁简转换 + 过滤，生成 .raw 文件
make filter     # 按规则过滤，生成 dict.raw
```

过滤规则：
- 全部要求纯汉字
- zhwiktionary ≤4 字保留（4 字大概率成语）
- 其它来源 ≤3 字保留
- zhwiki 中含 `·` 的词按 `·` 拆分，每段 ≤4 字保留（处理人名地名）

### 2. 获取语料

从 HuggingFace 下载中文维基百科和维基新闻语料：

```bash
cd data
make download   # 下载 finewiki(zhwiki) + roots_zh_wikinews
make process    # 繁简转换，合并为 data.txt
```

需要安装 `datasets` 和 `opencc`：
```bash
uv pip install datasets huggingface_hub[cli] opencc
```

### 3. 训练词频（EM + 剪枝）

```bash
cd scripts
make count_chars   # 统计语料字频 → chars.cnt
make dict.0        # 用字频过滤词表 → dict.0
make em            # EM 训练 + 剪枝 → output/dict.txt
make replace       # 替换 dict.txt
```

或者一步到位：

```bash
cd scripts
make
```

可通过参数控制词表大小和 EM 迭代次数：

```bash
make VOCAB_SIZE=100000 SUB_ITERS=3
```

训练流程类似 SentencePiece Unigram：

1. **冷启动**：用初始词表做最长匹配分词，统计词频
2. **EM + 剪枝循环**（词表大小 > 目标时重复）：
   - EM 子迭代：DAG+DP 分词 → 统计词频 → 更新词表（默认 2 轮）
   - 剪枝：计算每个词的全局 loss（删除后似然损失），保留 top 75%
3. **最终 EM**：在剪枝后的词表上再跑一轮 EM 收敛词频

## 项目结构

```
src/           - C++ 分词器核心
  trie.h       - Double-Array Trie（XOR 索引，前缀搜索）
  cut.h/cc     - NaiveCutter（DAG+DP）和 SemanticCutter（中英分流）
  piece.h/cc   - PieceTokenizer（BPE 分词，sentencepiece 格式）
  segment.h/cc - 正向最长匹配（EM 冷启动用）
  ustr.h/cc    - UTF-8 工具：SplitByHan、SplitByPunct、SplitNonHan
  count.h/cc   - 词频统计
  main.cc      - CLI 入口
  pip.cc       - pybind11 Python 绑定
dict/          - 维基词表获取与转换
data/          - 语料下载与处理
scripts/       - 训练流程（字频统计、词表过滤、EM）
dict.txt       - 当前默认词频词典
piece.txt      - BPE piece 模型
```

## License

MIT
