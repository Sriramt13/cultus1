#pragma once
#include "../include/common.h"
#include "arena.h"

namespace cultus {

struct CacheBin {
    FreeNode* head;
    int       count;
    int       max_count;
};

class ThreadCache {
public:
    ThreadCache();
    ~ThreadCache();

    void* allocate(size_t sz);
    void  deallocate(void* ptr, size_t sz);

private:
    void refill(int cid);
    void flush(int cid);

    CacheBin bins_[NUM_CLASSES];
    uint64_t alloc_count_;
    uint64_t dealloc_count_;
    uint64_t cache_hits_;
};

extern thread_local ThreadCache t_thread_cache;

}
