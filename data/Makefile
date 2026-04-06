all: download process sentences.txt

download: data/zhwiki.jsonl data/zh_wikinews.jsonl

data/zhwiki.jsonl:
	python3 fetch_finewiki.py

data/zh_wikinews.jsonl:
	python3 fetch_wikinews.py

process: data.txt

data.txt: data/zhwiki.jsonl data/zh_wikinews.jsonl
	python3 process.py

sentences.txt: data.txt
	python3 split_sentences.py

clean:
	rm -rf data/ data.txt sentences.txt

.PHONY: all download process clean
