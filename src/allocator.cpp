#include "../include/allocator.h"
#include "../include/common.h"
#include "arena.h"
#include "thread_cache.h"
#include <cstring>

namespace cultus {

void* Allocator::allocate(size_t sz) {
    return t_thread_cache.allocate(sz);
}

void Allocator::deallocate(void* ptr) {
    if (!ptr) return;
    auto* hdr = reinterpret_cast<SpanHeader*>(
        reinterpret_cast<uintptr_t>(ptr) & ~(SPAN_SIZE - 1));
    if (hdr->magic == MAGIC_LARGE) {
        CentralArena::get().freeLarge(ptr);
    } else {
        t_thread_cache.deallocate(ptr, hdr->block_size);
    }
}

void* Allocator::reallocate(void* ptr, size_t sz) {
    if (!ptr) return allocate(sz);
    if (sz == 0) { deallocate(ptr); return nullptr; }
    void* np = allocate(sz);
    if (!np) return nullptr;
    auto* hdr = reinterpret_cast<SpanHeader*>(
        reinterpret_cast<uintptr_t>(ptr) & ~(SPAN_SIZE - 1));
    size_t copy_sz = hdr->block_size < (uint32_t)sz ? hdr->block_size : (uint32_t)sz;
    memcpy(np, ptr, copy_sz);
    deallocate(ptr);
    return np;
}

}

extern "C" {

void* my_malloc(size_t sz) {
    return cultus::t_thread_cache.allocate(sz);
}

void my_free(void* ptr) {
    cultus::Allocator::deallocate(ptr);
}

void* my_calloc(size_t num, size_t sz) {
    size_t total = num * sz;
    void*  p     = cultus::t_thread_cache.allocate(total);
    if (p) memset(p, 0, total);
    return p;
}

void* my_realloc(void* ptr, size_t sz) {
    return cultus::Allocator::reallocate(ptr, sz);
}

void* my_memalign(size_t align, size_t sz) {
    if (align <= 8) return my_malloc(sz);
    size_t total = sz + align;
    void*  raw   = my_malloc(total);
    if (!raw) return nullptr;
    uintptr_t addr = (reinterpret_cast<uintptr_t>(raw) + align - 1) & ~(align - 1);
    return reinterpret_cast<void*>(addr);
}

}
