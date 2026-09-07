#pragma once
#include "../include/common.h"
#include <atomic>
#include <mutex>

namespace cultus {

constexpr uint32_t MAGIC_SMALL = 0x414C4C43;
constexpr uint32_t MAGIC_LARGE = 0x4C524741;

struct FreeNode {
    FreeNode* next;
};

struct SpanHeader {
    uint32_t magic;
    uint32_t class_id;
    uint32_t block_size;
    uint32_t total_blocks;
    SpanHeader* next_span;
};

class CentralClassArena {
public:
    void init(int cid, uint32_t bsz, uint32_t batch);
    int  fetch_batch(void** out, int want);
    void return_batch(void** in, int count);

private:
    void grow();

    int       class_id_;
    uint32_t  block_size_;
    uint32_t  batch_size_;
    FreeNode* free_list_;
    SpanHeader* spans_;
    std::mutex mu_;
};

class CentralArena {
public:
    static CentralArena& get();
    CentralClassArena& arena(int cid);
    void*  allocLarge(size_t sz);
    void   freeLarge(void* ptr);

private:
    CentralArena();
    CentralClassArena arenas_[NUM_CLASSES];
};

}
