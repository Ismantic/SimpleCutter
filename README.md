# Iscut (Is Semantic Cutter) 是语切词

想办法构建一个词表，找一大批中文语料，就能构建一个还不错的分词器。

## 特性

- **DAG + 动态规划分词** — 前缀搜索构建 DAG，后向 DP 求最大概率路径
- **标点预切分** — 先按标点符号切分，再对每段做分词
- **EM 词频自举** — 给定词典和生语料，无需标注数据即可学出词频
- **冷启动分词** — 正向最长匹配，用于 EM 的初始化
- **Double-Array Trie** — XOR 索引，支持精确查找和公共前缀搜索
- **完整可复现** — 从数据获取到词表训练全流程自动化
- **纯 C++17** — 无外部依赖，< 1000 行

## 使用

### 编译

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 交互模式

```bash
./build/iscut --dict dict.txt
```

```
> 南京市长江大桥
南京市/长江/大桥
```

### Pipe 模式

```bash
./build/iscut --dict dict.txt --pipe < input.txt > output.txt
```

### Python

```bash
uv pip install .
```

```python
import iscut

cutter = iscut.Cutter("dict.txt")
result = cutter.cut("南京市长江大桥")
print(result)  # ['南京市', '长江', '大桥']
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
dict/          - 维基词表获取与转换
data/          - 语料下载与处理
scripts/       - 训练流程（字频统计、词表过滤、EM）
dict.txt  - 当前默认词频词典
```

## License

MIT
