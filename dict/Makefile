VERSION=20251220
WEB_SLANG_VERSION=20251223
ZHWIKI_FILENAME=zhwiki-$(VERSION)-all-titles-in-ns0
ZHDICT_FILENAME=zhwiktionary-$(VERSION)-all-titles-in-ns0
ZHSRC_FILENAME=zhwikisource-$(VERSION)-all-titles-in-ns0
ZHWIKI_PAGE=zhwiki-$(VERSION)-page.sql
ZHWIKI_REDIRECT=zhwiki-$(VERSION)-redirect.sql
WEB_SLANG_FILE=web-slang-$(WEB_SLANG_VERSION).txt
WEB_SLANG_SOURCE=web-slang-$(WEB_SLANG_VERSION).wikitext

.DELETE_ON_ERROR:

all: download convert filter

convert: zhwiki.raw zhwiktionary.raw zhwikisource.raw web-slang.raw zhwiki-redirects.txt

filter: convert dict.raw

dict.raw: zhwiki.raw zhwiktionary.raw zhwikisource.raw web-slang.raw zhwiki-redirects.txt
	./filter.py > $@

download: $(ZHWIKI_FILENAME).gz $(ZHDICT_FILENAME).gz $(ZHSRC_FILENAME).gz $(ZHWIKI_PAGE).gz $(ZHWIKI_REDIRECT).gz

$(ZHWIKI_FILENAME).gz:
	wget https://dumps.wikimedia.org/zhwiki/$(VERSION)/$(ZHWIKI_FILENAME).gz

$(ZHDICT_FILENAME).gz:
	wget https://dumps.wikimedia.org/zhwiktionary/$(VERSION)/$(ZHDICT_FILENAME).gz

$(ZHSRC_FILENAME).gz:
	wget https://dumps.wikimedia.org/zhwikisource/$(VERSION)/$(ZHSRC_FILENAME).gz

$(ZHWIKI_PAGE).gz:
	wget https://dumps.wikimedia.org/zhwiki/$(VERSION)/$(ZHWIKI_PAGE).gz

$(ZHWIKI_REDIRECT).gz:
	wget https://dumps.wikimedia.org/zhwiki/$(VERSION)/$(ZHWIKI_REDIRECT).gz

zhwiki-redirects.txt: $(ZHWIKI_PAGE).gz $(ZHWIKI_REDIRECT).gz
	./extract-redirects.py $(ZHWIKI_PAGE).gz $(ZHWIKI_REDIRECT).gz > $@

$(WEB_SLANG_SOURCE):
	./zhwiki-web-slang.py --fetch > $(WEB_SLANG_SOURCE)

$(WEB_SLANG_FILE): $(WEB_SLANG_SOURCE)
	./zhwiki-web-slang.py --process $(WEB_SLANG_SOURCE) > $(WEB_SLANG_FILE)

%: %.gz
	gzip -k -d $<

zhwiki.source: $(ZHWIKI_FILENAME)
	cp $< $@

zhwiktionary.source: $(ZHDICT_FILENAME)
	cp $< $@

zhwikisource.source: $(ZHSRC_FILENAME)
	cp $< $@

web-slang.source: $(WEB_SLANG_FILE)
	cp $< $@

%.raw: %.source
	./convert.py $< > $@.tmp
	sort -u $@.tmp > $@

clean:
	rm -f $(ZHWIKI_FILENAME).gz $(ZHWIKI_FILENAME) zhwiki.source zhwiki.raw zhwiki.raw.tmp
	rm -f $(ZHDICT_FILENAME).gz $(ZHDICT_FILENAME) zhwiktionary.source zhwiktionary.raw zhwiktionary.raw.tmp
	rm -f $(ZHSRC_FILENAME).gz $(ZHSRC_FILENAME) zhwikisource.source zhwikisource.raw zhwikisource.raw.tmp
	rm -f $(WEB_SLANG_SOURCE) $(WEB_SLANG_FILE) web-slang.source web-slang.raw web-slang.raw.tmp
	rm -f $(ZHWIKI_PAGE).gz $(ZHWIKI_REDIRECT).gz zhwiki-redirects.txt
	rm -f dict.raw
