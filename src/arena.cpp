#include "arena.h"
#include <cstring>
#include <cassert>

namespace cultus {

void CentralClassArena::init(int cid, uint32_t bsz, uint32_t batch) {
    class_id_   = cid;
    block_size_ = bsz;
    batch_size_ = batch;
    free_list_  = nullptr;
    spans_      = nullptr;
}

void CentralClassArena::grow() {
    size_t span_bytes = SPAN_SIZE;
    void*  raw        = osAllocPages(span_bytes);
    if (!raw) return;

    auto* hdr = reinterpret_cast<SpanHeader*>(raw);
    hdr->magic        = MAGIC_SMALL;
    hdr->class_id     = (uint32_t)class_id_;
    hdr->block_size   = block_size_;
    hdr->next_span    = spans_;
    spans_            = hdr;

    uintptr_t start = reinterpret_cast<uintptr_t>(raw) + 64;
    uintptr_t end   = reinterpret_cast<uintptr_t>(raw) + span_bytes;
    uintptr_t cur   = start;

    while (cur + block_size_ <= end) {
        auto* node = reinterpret_cast<FreeNode*>(cur);
        node->next = free_list_;
        free_list_ = node;
        cur += block_size_;
    }
}

int CentralClassArena::fetch_batch(void** out, int want) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!free_list_) grow();

    int got = 0;
    while (got < want && free_list_) {
        out[got++] = free_list_;
        free_list_ = free_list_->next;
    }
    return got;
}

void CentralClassArena::return_batch(void** in, int count) {
    std::lock_guard<std::mutex> lock(mu_);
    for (int i = 0; i < count; i++) {
        auto* node = reinterpret_cast<FreeNode*>(in[i]);
        node->next = free_list_;
        free_list_ = node;
    }
}

CentralArena::CentralArena() {
    for (size_t i = 0; i < NUM_CLASSES; i++)
        arenas_[i].init((int)i, SIZE_CLASSES[i].block_size, SIZE_CLASSES[i].batch_size);
}

CentralArena& CentralArena::get() {
    static CentralArena inst;
    return inst;
}

CentralClassArena& CentralArena::arena(int cid) {
    return arenas_[cid];
}

void* CentralArena::allocLarge(size_t sz) {
    size_t total = sz + sizeof(SpanHeader);
    total = (total + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    void* raw = osAllocPages(total);
    if (!raw) return nullptr;
    auto* hdr = reinterpret_cast<SpanHeader*>(raw);
    hdr->magic      = MAGIC_LARGE;
    hdr->class_id   = 0;
    hdr->block_size = (uint32_t)total;
    hdr->next_span  = nullptr;
    return reinterpret_cast<char*>(raw) + sizeof(SpanHeader);
}

void CentralArena::freeLarge(void* ptr) {
    auto* hdr = reinterpret_cast<SpanHeader*>(
        reinterpret_cast<char*>(ptr) - sizeof(SpanHeader));
    osFreePages(hdr, hdr->block_size);
}

}
