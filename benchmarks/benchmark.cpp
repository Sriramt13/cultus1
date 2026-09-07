#include "../include/allocator.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <cstdlib>
#include <numeric>

using namespace std::chrono;

struct Result {
    double ops_per_sec;
    double p50_ns;
    double p99_ns;
};

static Result bench_throughput(int threads, bool use_system) {
    constexpr int OPS = 100000;
    std::vector<double> times(threads);

    auto worker = [&](int idx) {
        std::vector<void*> ptrs(OPS);
        auto t0 = high_resolution_clock::now();
        for (int i = 0; i < OPS; i++) {
            size_t sz = (size_t)(rand() % 1024) + 8;
            ptrs[i] = use_system ? malloc(sz) : my_malloc(sz);
        }
        for (int i = 0; i < OPS; i++) {
            if (use_system) free(ptrs[i]);
            else            my_free(ptrs[i]);
        }
        auto t1  = high_resolution_clock::now();
        times[idx] = duration_cast<nanoseconds>(t1 - t0).count() / (double)OPS;
    };

    std::vector<std::thread> pool;
    for (int i = 0; i < threads; i++) pool.emplace_back(worker, i);
    for (auto& t : pool) t.join();

    std::sort(times.begin(), times.end());
    double avg = std::accumulate(times.begin(), times.end(), 0.0) / threads;
    Result r;
    r.ops_per_sec = 1e9 / avg * threads;
    r.p50_ns      = times[times.size() / 2];
    r.p99_ns      = times[(size_t)(times.size() * 0.99)];
    return r;
}

int main() {
    std::vector<int> thread_counts = {1, 2, 4, 8};

    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(18) << "System(Mops/s)"
              << std::setw(18) << "Custom(Mops/s)"
              << std::setw(12) << "Speedup"
              << "\n";
    std::cout << std::string(58, '-') << "\n";

    for (int tc : thread_counts) {
        auto sys = bench_throughput(tc, true);
        auto cus = bench_throughput(tc, false);
        double speedup = cus.ops_per_sec / sys.ops_per_sec;
        std::cout << std::setw(10) << tc
                  << std::setw(18) << std::fixed << std::setprecision(2)
                  << sys.ops_per_sec / 1e6
                  << std::setw(18) << cus.ops_per_sec / 1e6
                  << std::setw(12) << speedup << "x"
                  << "\n";
    }
}
