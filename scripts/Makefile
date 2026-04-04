VOCAB_SIZE = 256000
SUB_ITERS = 2
MIN_COUNT = 10
CORPUS = ../data/data.txt
DICT_RAW = ../dict/dict.raw
ISCUT = ../build/iscut
OUT = output

all: ../dict.txt

# Step 1: count character frequencies
count_chars: $(OUT)/count_chars
$(OUT)/count_chars: count_chars.cc
	mkdir -p $(OUT)
	g++ -O3 -std=c++17 $< -o $@

chars.cnt: $(OUT)/count_chars $(CORPUS)
	$(OUT)/count_chars $(CORPUS) $@

# Step 2: filter dict by char frequency
dict.0: chars.cnt $(DICT_RAW)
	python3 filter_dict.py chars.cnt $(DICT_RAW) $@ --min-count $(MIN_COUNT)

# Step 3: EM training + pruning
em: $(OUT)/dict.txt
$(OUT)/dict.txt: dict.0 $(CORPUS) $(ISCUT)
	./run_em.sh dict.0 $(CORPUS) $(OUT) $(VOCAB_SIZE) $(SUB_ITERS) $(MIN_COUNT)

# Step 4: replace dict
replace: ../dict.txt
../dict.txt: $(OUT)/dict.txt
	cp $< $@
	@echo "=== Installed dict.txt ==="

clean:
	rm -rf $(OUT) chars.cnt dict.0

.PHONY: all count_chars em replace clean
