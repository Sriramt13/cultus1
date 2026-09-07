#include "../include/allocator.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <cstring>
#include <mutex>
#include <queue>
#include <condition_variable>

static void test_basic() {
    void* p1 = my_malloc(8);
    void* p2 = my_malloc(128);
    void* p3 = my_malloc(4096);
    void* p4 = my_malloc(9000);
    assert(p1 && p2 && p3 && p4);
    my_free(p1); my_free(p2); my_free(p3); my_free(p4);
    std::cout << "[pass] basic alloc/free\n";
}

static void test_calloc() {
    void* p = my_calloc(16, 64);
    assert(p);
    unsigned char* b = (unsigned char*)p;
    for (int i = 0; i < 16 * 64; i++) assert(b[i] == 0);
    my_free(p);
    std::cout << "[pass] calloc zeroed\n";
}

static void test_realloc() {
    void* p = my_malloc(64);
    assert(p);
    memset(p, 0xAB, 64);
    p = my_realloc(p, 256);
    assert(p);
    unsigned char* b = (unsigned char*)p;
    for (int i = 0; i < 64; i++) assert(b[i] == 0xAB);
    my_free(p);
    std::cout << "[pass] realloc data preserved\n";
}

static void test_stress_single() {
    std::vector<void*> ptrs;
    ptrs.reserve(10000);
    for (int i = 0; i < 10000; i++) {
        size_t sz = (size_t)(rand() % 8192) + 1;
        ptrs.push_back(my_malloc(sz));
        assert(ptrs.back());
    }
    for (void* p : ptrs) my_free(p);
    std::cout << "[pass] single-thread stress\n";
}

static void test_concurrent() {
    constexpr int THREADS = 8;
    constexpr int OPS     = 50000;
    std::atomic<int> errors{0};

    auto worker = [&]() {
        std::vector<void*> ptrs;
        ptrs.reserve(OPS);
        for (int i = 0; i < OPS; i++) {
            size_t sz = (size_t)(rand() % 4096) + 1;
            void* p = my_malloc(sz);
            if (!p) { errors++; continue; }
            memset(p, 0x5A, sz);
            ptrs.push_back(p);
        }
        for (void* p : ptrs) my_free(p);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < THREADS; i++) threads.emplace_back(worker);
    for (auto& t : threads) t.join();
    assert(errors.load() == 0);
    std::cout << "[pass] concurrent alloc/free (" << THREADS << " threads)\n";
}

static void test_producer_consumer() {
    std::queue<void*>       q;
    std::mutex              mu;
    std::condition_variable cv;
    std::atomic<bool>       done{false};

    auto producer = [&]() {
        for (int i = 0; i < 20000; i++) {
            void* p = my_malloc(64);
            if (!p) continue;
            std::lock_guard<std::mutex> lock(mu);
            q.push(p);
            cv.notify_one();
        }
        done = true;
        cv.notify_all();
    };

    auto consumer = [&]() {
        while (true) {
            std::unique_lock<std::mutex> lock(mu);
            cv.wait(lock, [&]{ return !q.empty() || done; });
            while (!q.empty()) {
                void* p = q.front(); q.pop();
                lock.unlock();
                my_free(p);
                lock.lock();
            }
            if (done && q.empty()) break;
        }
    };

    std::thread prod(producer), cons(consumer);
    prod.join(); cons.join();
    std::cout << "[pass] producer-consumer cross-thread free\n";
}

int main() {
    test_basic();
    test_calloc();
    test_realloc();
    test_stress_single();
    test_concurrent();
    test_producer_consumer();
    std::cout << "\nAll tests passed.\n";
}
