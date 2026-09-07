#include "thread_cache.h"
#include <cstdlib>

namespace cultus {

thread_local ThreadCache t_thread_cache;

ThreadCache::ThreadCache() : alloc_count_(0), dealloc_count_(0), cache_hits_(0) {
    for (size_t i = 0; i < NUM_CLASSES; i++) {
        bins_[i].head      = nullptr;
        bins_[i].count     = 0;
        bins_[i].max_count = (int)SIZE_CLASSES[i].batch_size * 2;
    }
}

ThreadCache::~ThreadCache() {
    void* tmp[64];
    for (size_t i = 0; i < NUM_CLASSES; i++) {
        int n = 0;
        FreeNode* node = bins_[i].head;
        while (node) {
            tmp[n++] = node;
            node = node->next;
            if (n == 64) {
                CentralArena::get().arena((int)i).return_batch(tmp, n);
                n = 0;
            }
        }
        if (n > 0)
            CentralArena::get().arena((int)i).return_batch(tmp, n);
        bins_[i].head  = nullptr;
        bins_[i].count = 0;
    }
}

void ThreadCache::refill(int cid) {
    void* buf[64];
    int   want = SIZE_CLASSES[cid].batch_size;
    int   got  = CentralArena::get().arena(cid).fetch_batch(buf, want);
    for (int i = 0; i < got; i++) {
        auto* node = reinterpret_cast<FreeNode*>(buf[i]);
        node->next    = bins_[cid].head;
        bins_[cid].head = node;
        bins_[cid].count++;
    }
}

void ThreadCache::flush(int cid) {
    int flush_count = bins_[cid].count / 2;
    void* buf[64];
    int n = 0;
    while (n < flush_count && bins_[cid].head) {
        buf[n++] = bins_[cid].head;
        bins_[cid].head = bins_[cid].head->next;
        bins_[cid].count--;
    }
    if (n > 0)
        CentralArena::get().arena(cid).return_batch(buf, n);
}

void* ThreadCache::allocate(size_t sz) {
    if (sz == 0) sz = 1;
    if (sz > MAX_SMALL_SIZE)
        return CentralArena::get().allocLarge(sz);

    alloc_count_++;
    int cid = sizeToClass(sz);
    if (!bins_[cid].head)
        refill(cid);

    if (!bins_[cid].head)
        return nullptr;

    cache_hits_++;
    void* p = bins_[cid].head;
    bins_[cid].head = bins_[cid].head->next;
    bins_[cid].count--;
    return p;
}

void ThreadCache::deallocate(void* ptr, size_t sz) {
    if (!ptr) return;
    if (sz > MAX_SMALL_SIZE) {
        CentralArena::get().freeLarge(ptr);
        return;
    }
    dealloc_count_++;
    int cid = sizeToClass(sz);
    auto* node = reinterpret_cast<FreeNode*>(ptr);
    node->next      = bins_[cid].head;
    bins_[cid].head = node;
    bins_[cid].count++;
    if (bins_[cid].count > bins_[cid].max_count)
        flush(cid);
}

}
