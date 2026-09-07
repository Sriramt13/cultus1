#pragma once
#include <cstddef>
#include <cstdint>

namespace cultus {

constexpr size_t PAGE_SIZE      = 4096;
constexpr size_t SPAN_SIZE      = 65536;
constexpr size_t MAX_SMALL_SIZE = 8192;
constexpr size_t NUM_CLASSES    = 36;
constexpr size_t THREAD_CACHE_MAX_BLOCKS = 64;

struct SizeClass {
    uint32_t block_size;
    uint32_t batch_size;
};

extern const SizeClass SIZE_CLASSES[NUM_CLASSES];
extern uint8_t SIZE_CLASS_LOOKUP[MAX_SMALL_SIZE + 1];

int sizeToClass(size_t sz);
void* osAllocPages(size_t bytes);
void  osFreePages(void* ptr, size_t bytes);

}
