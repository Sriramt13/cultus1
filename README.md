# Concurrent Lock-Free Skip List in C++20

Skip list with lock-free concurrent insert, remove, and search operations. Uses mark bits packed into the `next` pointers for logical deletion (Harris-Herlihy-Shavit style) and hazard pointers for safe memory reclamation without locks or GC.

## Core Algorithms

**Logical Deletion**  
When removing a node, the bottom bit of each `next[i]` pointer is atomically set to 1 (mark bit). Marked nodes are invisible to concurrent searches and are physically unlinked during the next `locate()` traversal that passes through them.

**Hazard Pointers**  
Before dereferencing any shared pointer, a thread publishes it to its hazard slot (`HP_SLOTS = 3` per thread, `MAX_THREADS = 128`). Retired nodes go into a per-thread retire list and are only `delete`d after confirming no other thread has them hazard-protected. Reclamation triggers when the retire list reaches `RETIRE_THRESHOLD`.

**Level Distribution**  
Each node picks a height using a geometric distribution (coin flip per level, up to `MAXLVL = 16`). This gives expected O(log n) search time.

**Baseline**  
`LockedSkipList` wraps `std::map` with a single `std::mutex`. The benchmark compares throughput and latency percentiles between the lock-free version and this coarse-grained locked baseline.

## Build

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

Or with make:
```sh
make
```

## Run

```sh
./test_correctness
./benchmark
```

## Files

```
include/
  hazard.hpp          per-thread hazard pointer table
  skiplist.hpp        lock-free skip list (header-only)
  locked_baseline.hpp coarse-grained locked baseline
tests/
  test_correctness.cpp
benchmarks/
  benchmark.cpp
CMakeLists.txt
Makefile
```

## Memory Model

All CAS operations on `next` pointers use `memory_order_release` for the success case and `memory_order_relaxed` for failure. Loads use `memory_order_acquire`. The linearization point for insert is the successful CAS on `next[0]`; for remove it is the CAS that sets the mark bit on `next[0]`.
