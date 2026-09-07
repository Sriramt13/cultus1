#pragma once
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

void* my_malloc(size_t size);
void  my_free(void* ptr);
void* my_calloc(size_t num, size_t size);
void* my_realloc(void* ptr, size_t size);
void* my_memalign(size_t align, size_t size);

#ifdef __cplusplus
}

namespace cultus {
struct Allocator {
    static void* allocate(size_t sz);
    static void  deallocate(void* ptr);
    static void* reallocate(void* ptr, size_t sz);
};
}
#endif
