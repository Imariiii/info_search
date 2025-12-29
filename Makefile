.PHONY: all tokenizer stemmer search_engine clean

all: tokenizer stemmer search_engine

tokenizer:
	$(MAKE) -C tokenizer

stemmer:
	$(MAKE) -C stemmer

search_engine:
	$(MAKE) -C search_engine

clean:
	$(MAKE) -C tokenizer clean
	$(MAKE) -C stemmer clean
	$(MAKE) -C search_engine clean

cleanbin:
	rm -f *.bin
	rm -f search_engine/*.bin