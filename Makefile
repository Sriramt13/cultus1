CXX      ?= g++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -pthread -Iinclude -Isrc

SRCS = src/common.cpp src/arena.cpp src/thread_cache.cpp src/allocator.cpp
OBJS = $(SRCS:.cpp=.o)

all: test_correctness benchmark

liballocator.a: $(OBJS)
	ar rcs $@ $^

test_correctness: tests/test_correctness.cpp liballocator.a
	$(CXX) $(CXXFLAGS) $^ -o $@

benchmark: benchmarks/benchmark.cpp liballocator.a
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f $(OBJS) liballocator.a test_correctness benchmark

.PHONY: all clean
