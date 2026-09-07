#include "../include/skiplist.hpp"
#include "../include/locked_baseline.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <random>
#include <algorithm>

using clk = std::chrono::high_resolution_clock;

struct Run {
    double mops;
    double p50_ns;
    double p99_ns;
};

template<typename SL>
Run measure(int n_threads, int ops_each) {
    SL sl;
    for (int i = 0; i < 10000; i++) sl.insert(i, i);

    std::vector<double> per_thread(n_threads);

    auto worker = [&](int idx) {
        std::mt19937 rng(idx * 0xDEAD + 1);
        std::uniform_int_distribution<int> kd(0, 1 << 18);
        std::uniform_int_distribution<int> od(0, 9);

        std::vector<double> lat;
        lat.reserve(ops_each);

        for (int i = 0; i < ops_each; i++) {
            int k = kd(rng);
            int op = od(rng);
            auto t0 = clk::now();
            if (op < 2)       sl.insert(k, k);
            else if (op < 4)  sl.remove(k);
            else               sl.contains(k);
            auto t1 = clk::now();
            lat.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        }
        std::sort(lat.begin(), lat.end());
        per_thread[idx] = lat[lat.size() / 2];
    };

    auto wall0 = clk::now();
    std::vector<std::thread> pool;
    for (int t = 0; t < n_threads; t++) pool.emplace_back(worker, t);
    for (auto& th : pool) th.join();
    auto wall1 = clk::now();

    double secs = std::chrono::duration<double>(wall1 - wall0).count();
    std::sort(per_thread.begin(), per_thread.end());

    return {
        (double)(n_threads * ops_each) / secs / 1e6,
        per_thread[per_thread.size() / 2],
        per_thread[(size_t)(per_thread.size() * 0.99)]
    };
}

int main() {
    constexpr int OPS = 200000;
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};

    auto header = [](const char* label) {
        std::cout << "\n=== " << label << " ===\n"
                  << std::left
                  << std::setw(10) << "Threads"
                  << std::setw(14) << "Mops/s"
                  << std::setw(14) << "P50(ns)"
                  << std::setw(14) << "P99(ns)"
                  << "\n" << std::string(52, '-') << "\n";
    };

    auto row = [](int t, const Run& r) {
        std::cout << std::left
                  << std::setw(10) << t
                  << std::setw(14) << std::fixed << std::setprecision(2) << r.mops
                  << std::setw(14) << r.p50_ns
                  << std::setw(14) << r.p99_ns
                  << "\n";
    };

    header("Lock-Free Skip List");
    std::vector<Run> lf_runs;
    for (int t : thread_counts) {
        auto r = measure<cultus::SkipList<int,int>>(t, OPS);
        row(t, r);
        lf_runs.push_back(r);
    }

    header("Locked Baseline");
    std::vector<Run> lb_runs;
    for (int t : thread_counts) {
        auto r = measure<cultus::LockedSkipList<int,int>>(t, OPS);
        row(t, r);
        lb_runs.push_back(r);
    }

    std::cout << "\n=== Speedup (lock-free vs locked) ===\n"
              << std::setw(10) << "Threads" << std::setw(14) << "Speedup\n"
              << std::string(24, '-') << "\n";

    for (size_t i = 0; i < thread_counts.size(); i++) {
        double sp = lf_runs[i].mops / lb_runs[i].mops;
        std::cout << std::setw(10) << thread_counts[i]
                  << std::setw(14) << std::fixed << std::setprecision(2) << sp << "x\n";
    }
}
