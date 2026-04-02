ROUNDS = 8
CORPUS = ../data/data.txt
DICT_RAW = ../conv/dict.raw
ISCUT = ../build/iscut
OUT = output

all: ../dict/dict.txt

# Step 1: count character frequencies
count_chars: $(OUT)/count_chars
$(OUT)/count_chars: count_chars.cc
	mkdir -p $(OUT)
	g++ -O3 -std=c++17 $< -o $@

chars.cnt: $(OUT)/count_chars $(CORPUS)
	$(OUT)/count_chars $(CORPUS) $@

# Step 2: filter dict by char frequency
dict.0: chars.cnt $(DICT_RAW)
	python3 filter_dict.py chars.cnt $(DICT_RAW) $@

# Step 3: EM training
em: $(OUT)/dict.$(ROUNDS)
$(OUT)/dict.$(ROUNDS): dict.0 $(CORPUS) $(ISCUT)
	./run_em.sh dict.0 $(CORPUS) $(OUT) $(ROUNDS)

# Step 4: replace dict
replace: ../dict/dict.txt
../dict/dict.txt: $(OUT)/dict.$(ROUNDS)
	cp $< $@
	@echo "=== Installed dict/dict.txt ==="

clean:
	rm -rf $(OUT) chars.cnt dict.0

.PHONY: all count_chars em replace clean
