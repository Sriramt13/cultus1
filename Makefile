CXX      ?= g++
CXXFLAGS ?= -std=c++20 -O3 -Wall -Wextra -pthread -Iinclude

all: test_correctness benchmark

test_correctness: tests/test_correctness.cpp include/skiplist.hpp include/hazard.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

benchmark: benchmarks/benchmark.cpp include/skiplist.hpp include/locked_baseline.hpp include/hazard.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f test_correctness benchmark

.PHONY: all clean
