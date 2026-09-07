# Concurrent Thread-Safe Memory Allocator

A C++17 memory allocator built around thread-local caching and a sharded central arena, designed to reduce lock contention under heavy multi-threaded workloads.

## Design

**Size Classes**  
36 size classes from 8 to 8192 bytes with progressively coarser granularity. Allocations map to the nearest class in O(1) using a lookup table. Anything above 8 KB goes directly to the OS.

**Thread Cache**  
Each thread has a private `ThreadCache` holding per-class free lists (no locks). Allocation pulls from the local bin; deallocation pushes back. When a bin exceeds its watermark, half the blocks flush to the central arena. When empty, a batch is pulled from the arena.

**Central Arena**  
One `CentralClassArena` per size class backed by a mutex-protected free list. Each class grows by allocating a 64 KB span from the OS and slicing it into blocks. Cross-thread deallocation lands here.

**Large Allocations**  
Requests over 8 KB bypass the cache entirely. The arena calls `VirtualAlloc`/`mmap` directly, prefixes the allocation with a span header, and frees it with `VirtualFree`/`munmap`.

## Build

### CMake
```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Make (Linux/macOS)
```sh
make
```

## Usage

```cpp
#include "allocator.h"

void* p = my_malloc(256);
my_free(p);

void* arr = my_calloc(100, sizeof(int));
arr = my_realloc(arr, 200 * sizeof(int));
my_free(arr);
```

Or through the C++ interface:
```cpp
void* p = cultus::Allocator::allocate(128);
cultus::Allocator::deallocate(p);
```

## Tests
```sh
./test_correctness
```
Covers: basic alloc/free, calloc zeroing, realloc data preservation, 10K single-thread stress, 8-thread concurrent stress, cross-thread producer-consumer free.

## Benchmark
```sh
./benchmark
```
Prints throughput (Mops/s) and speedup versus system `malloc` at 1, 2, 4, and 8 threads.

## File Layout
```
include/
  allocator.h       public C and C++ API
  common.h          size classes, OS page primitives, constants
src/
  common.cpp        size class table, lookup init, OS alloc/free
  arena.h/cpp       central arena, span management
  thread_cache.h/cpp  per-thread cache, refill, flush
  allocator.cpp     API wrappers
tests/
  test_correctness.cpp
benchmarks/
  benchmark.cpp
CMakeLists.txt
Makefile
```

## Platform Support

| Platform | Memory Backend |
|----------|---------------|
| Windows  | VirtualAlloc / VirtualFree |
| Linux / macOS | mmap / munmap |
