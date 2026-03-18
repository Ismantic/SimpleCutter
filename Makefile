CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall

cut: main.cc cut.cc cut.h trie.h
	$(CXX) $(CXXFLAGS) -o $@ main.cc cut.cc

clean:
	rm -f cut

.PHONY: clean
