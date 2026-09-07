#pragma once
#include <atomic>
#include <vector>
#include <thread>

namespace cultus {

static constexpr int HP_SLOTS   = 3;
static constexpr int MAX_THREADS = 128;
static constexpr int RETIRE_THRESHOLD = 64;

template<typename T>
class HazardTable {
    struct Slot { std::atomic<T*> p{nullptr}; };
    Slot table[MAX_THREADS][HP_SLOTS];
    std::vector<T*> retired[MAX_THREADS];

public:
    T* protect(int slot, std::atomic<T*>& src, int tid) {
        T* p;
        do {
            p = src.load(std::memory_order_relaxed);
            table[tid][slot].p.store(p, std::memory_order_seq_cst);
        } while (p != src.load(std::memory_order_acquire));
        return p;
    }

    void clear(int slot, int tid) {
        table[tid][slot].p.store(nullptr, std::memory_order_release);
    }

    void retire(T* node, int tid) {
        retired[tid].push_back(node);
        if ((int)retired[tid].size() >= RETIRE_THRESHOLD)
            reclaim(tid);
    }

    void reclaim(int tid) {
        std::vector<T*> live;
        live.reserve(MAX_THREADS * HP_SLOTS);
        for (int t = 0; t < MAX_THREADS; t++)
            for (int s = 0; s < HP_SLOTS; s++) {
                T* p = table[t][s].p.load(std::memory_order_acquire);
                if (p) live.push_back(p);
            }

        std::vector<T*> kept;
        for (T* r : retired[tid]) {
            bool hazardous = false;
            for (T* h : live) if (h == r) { hazardous = true; break; }
            if (hazardous) kept.push_back(r);
            else           delete r;
        }
        retired[tid] = std::move(kept);
    }
};

}
