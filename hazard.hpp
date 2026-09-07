#pragma once
#include <atomic>
#include <vector>

template<typename T>
struct HazardRec {
    std::atomic<T*> slot[3] = {};
    std::vector<T*> retired;
    char pad[64 - sizeof(std::atomic<T*>)*3 - sizeof(std::vector<T*>)];
};

template<typename T, int MAXT = 64>
struct HPTable {
    HazardRec<T> recs[MAXT];

    T* acquire(int tid, int s, std::atomic<T*>& src) {
        T* p;
        do {
            p = src.load(std::memory_order_relaxed);
            recs[tid].slot[s].store(p, std::memory_order_seq_cst);
        } while (p != src.load(std::memory_order_acquire));
        return p;
    }

    void drop(int tid, int s) {
        recs[tid].slot[s].store(nullptr, std::memory_order_release);
    }

    void retire(int tid, T* p) {
        recs[tid].retired.push_back(p);
        if (recs[tid].retired.size() < 2 * MAXT * 3)
            return;
        std::vector<T*> live;
        for (int t = 0; t < MAXT; t++)
            for (int s = 0; s < 3; s++) {
                T* h = recs[t].slot[s].load(std::memory_order_acquire);
                if (h) live.push_back(h);
            }
        auto& ret = recs[tid].retired;
        std::vector<T*> keep;
        for (T* r : ret) {
            bool safe = true;
            for (T* h : live) if (h == r) { safe = false; break; }
            if (safe) delete r;
            else keep.push_back(r);
        }
        ret = std::move(keep);
    }
};
