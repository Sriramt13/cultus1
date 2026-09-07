#include "skiplist.hpp"
#include "locked.hpp"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <random>

template<typename SL>
long long run(int nthreads, int ops) {
    SL sl;
    for (int i = 0; i < 5000; i++) sl.insert(i, i);

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> ts;
    for (int t = 0; t < nthreads; t++) {
        ts.emplace_back([&, t]() {
            std::mt19937 rng(t * 7919);
            std::uniform_int_distribution<int> kd(0, 1<<17);
            std::uniform_int_distribution<int> od(0, 9);
            for (int i = 0; i < ops; i++) {
                int k = kd(rng), o = od(rng);
                if (o < 2)      sl.insert(k, k);
                else if (o < 4) sl.remove(k);
                else            sl.has(k);
            }
        });
    }
    for (auto& t : ts) t.join();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
}

int main() {
    std::vector<int> threads = {1, 2, 4, 8};
    int ops = 150000;

    std::cout << "threads  lock-free(ms)  locked(ms)\n";
    for (int t : threads) {
        long long lf = run<SkipList<int,int>>(t, ops);
        long long lk = run<LockedSL<int,int>>(t, ops);
        std::cout << t << "        " << lf << "           " << lk << "\n";
    }
}
