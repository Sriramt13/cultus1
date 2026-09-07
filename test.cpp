#include "skiplist.hpp"
#include <thread>
#include <vector>
#include <cassert>
#include <iostream>
#include <atomic>
#include <random>

void test1() {
    SkipList<int,int> sl;
    for (int i = 0; i < 500; i++) assert(sl.insert(i, i*3));
    for (int i = 0; i < 500; i++) assert(!sl.insert(i, 0));
    for (int i = 0; i < 500; i++) assert(sl.has(i));
    assert(sl.search(200).value() == 600);
    for (int i = 0; i < 250; i++) assert(sl.remove(i));
    for (int i = 0; i < 250; i++) assert(!sl.has(i));
    assert(!sl.remove(9999));
    std::cout << "test1 ok\n";
}

void test2() {
    SkipList<int,int> sl;
    const int T = 8, N = 1000;
    std::atomic<int> cnt{0};
    std::vector<std::thread> ts;
    for (int t = 0; t < T; t++) {
        ts.emplace_back([&, t]() {
            int base = t * N;
            for (int i = 0; i < N; i++)
                if (sl.insert(base + i, base + i)) cnt++;
        });
    }
    for (auto& t : ts) t.join();
    assert(cnt == T * N);
    std::cout << "test2 ok (concurrent insert, " << T << " threads)\n";
}

void test3() {
    SkipList<int,int> sl;
    for (int i = 0; i < 200; i++) sl.insert(i, i);

    const int T = 6;
    std::vector<std::thread> ts;
    for (int t = 0; t < T; t++) {
        ts.emplace_back([&, t]() {
            std::mt19937 rng(t * 31337);
            std::uniform_int_distribution<int> kd(0, 499);
            for (int i = 0; i < 8000; i++) {
                int k = kd(rng);
                if (i % 3 == 0)      sl.insert(k, k);
                else if (i % 3 == 1) sl.remove(k);
                else                 sl.has(k);
            }
        });
    }
    for (auto& t : ts) t.join();
    std::cout << "test3 ok (mixed concurrent ops, " << T << " threads)\n";
}

int main() {
    test1();
    test2();
    test3();
    std::cout << "all passed\n";
}
