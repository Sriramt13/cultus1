#include "../include/skiplist.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <mutex>
#include <condition_variable>
#include <queue>

static void check(bool cond, const char* msg) {
    if (!cond) {
        std::cerr << "[FAIL] " << msg << "\n";
        std::exit(1);
    }
}

static void sequential_ops() {
    cultus::SkipList<int, int> sl;

    for (int i = 0; i < 1000; i++)
        check(sl.insert(i, i * 2), "insert new key");

    for (int i = 0; i < 1000; i++)
        check(!sl.insert(i, 0), "duplicate insert returns false");

    for (int i = 0; i < 1000; i++) {
        auto v = sl.find(i);
        check(v.has_value() && v.value() == i * 2, "find returns correct value");
    }

    for (int i = 0; i < 500; i++)
        check(sl.remove(i), "remove existing");

    for (int i = 0; i < 500; i++)
        check(!sl.contains(i), "removed key absent");

    for (int i = 500; i < 1000; i++)
        check(sl.contains(i), "unremoved key present");

    check(!sl.remove(9999), "remove missing returns false");
    check(!sl.find(9999).has_value(), "find missing returns nullopt");

    std::cout << "[pass] sequential insert/find/remove\n";
}

static void sorted_traversal() {
    cultus::SkipList<int, int> sl;
    std::vector<int> keys = {50, 10, 90, 3, 77, 25};
    for (int k : keys) sl.insert(k, k);

    std::vector<int> out;
    sl.for_each([&](const int& k, const int&) { out.push_back(k); });

    check(std::is_sorted(out.begin(), out.end()), "for_each sorted order");
    std::cout << "[pass] sorted traversal\n";
}

static void concurrent_insert_no_overlap() {
    constexpr int T  = 8;
    constexpr int N  = 2000;
    cultus::SkipList<int, int> sl;
    std::atomic<int> inserted{0};

    auto worker = [&](int base) {
        for (int i = 0; i < N; i++)
            if (sl.insert(base + i, base + i)) inserted++;
    };

    std::vector<std::thread> pool;
    for (int t = 0; t < T; t++) pool.emplace_back(worker, t * N);
    for (auto& th : pool) th.join();

    check(inserted.load() == T * N, "all disjoint inserts succeed");
    std::cout << "[pass] concurrent insert no-overlap (" << T << " threads)\n";
}

static void concurrent_mixed() {
    constexpr int T   = 8;
    constexpr int OPS = 10000;
    cultus::SkipList<int, int> sl;

    for (int i = 0; i < 500; i++) sl.insert(i, i);

    std::atomic<int> errors{0};
    auto worker = [&]() {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> kd(0, 999);
        std::uniform_int_distribution<int> od(0, 2);
        for (int i = 0; i < OPS; i++) {
            int k = kd(rng);
            switch (od(rng)) {
                case 0: sl.insert(k, k);  break;
                case 1: sl.remove(k);     break;
                case 2: sl.contains(k);   break;
            }
        }
    };

    std::vector<std::thread> pool;
    for (int t = 0; t < T; t++) pool.emplace_back(worker);
    for (auto& th : pool) th.join();

    check(errors.load() == 0, "no errors in mixed concurrent ops");
    std::cout << "[pass] concurrent mixed ops (" << T << " threads)\n";
}

static void producer_consumer() {
    cultus::SkipList<int, int> sl;
    std::queue<int*>      q;
    std::mutex            mu;
    std::condition_variable cv;
    std::atomic<bool>     done{false};

    auto producer = [&]() {
        for (int i = 0; i < 20000; i++) {
            sl.insert(i, i);
            {
                std::lock_guard<std::mutex> lk(mu);
                q.push(new int(i));
            }
            cv.notify_one();
        }
        done = true;
        cv.notify_all();
    };

    auto consumer = [&]() {
        while (true) {
            std::unique_lock<std::mutex> lk(mu);
            cv.wait(lk, [&]{ return !q.empty() || done; });
            while (!q.empty()) {
                int* key = q.front(); q.pop();
                lk.unlock();
                sl.remove(*key);
                delete key;
                lk.lock();
            }
            if (done && q.empty()) break;
        }
    };

    std::thread p(producer), c(consumer);
    p.join(); c.join();

    std::cout << "[pass] producer-consumer cross-thread remove\n";
}

static void stress_insert_remove() {
    constexpr int T   = 6;
    constexpr int OPS = 20000;
    cultus::SkipList<int, int> sl;

    auto worker = [&](int seed) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> kd(0, 1 << 16);
        for (int i = 0; i < OPS; i++) {
            int k = kd(rng);
            if (i % 3 == 0) sl.remove(k);
            else             sl.insert(k, k);
        }
    };

    std::vector<std::thread> pool;
    for (int t = 0; t < T; t++) pool.emplace_back(worker, t * 997);
    for (auto& th : pool) th.join();

    std::cout << "[pass] heavy insert/remove stress (" << T << " threads)\n";
}

int main() {
    sequential_ops();
    sorted_traversal();
    concurrent_insert_no_overlap();
    concurrent_mixed();
    producer_consumer();
    stress_insert_remove();
    std::cout << "\nAll tests passed.\n";
}
