#pragma once
#include "hazard.hpp"
#include <atomic>
#include <optional>
#include <random>
#include <functional>
#include <limits>
#include <thread>

static int get_tid() {
    static std::atomic<int> ctr{0};
    thread_local int id = ctr.fetch_add(1, std::memory_order_relaxed);
    return id;
}

template<typename K, typename V>
struct SLNode {
    K key;
    V val;
    int lvl;
    std::atomic<uintptr_t> fwd[20];

    SLNode(K k, V v, int l) : key(k), val(v), lvl(l) {
        for (auto& f : fwd) f.store(0, std::memory_order_relaxed);
    }

    SLNode* next(int i) const {
        return reinterpret_cast<SLNode*>(fwd[i].load(std::memory_order_acquire) & ~uintptr_t(1));
    }

    bool del(int i) const {
        return fwd[i].load(std::memory_order_acquire) & 1;
    }

    bool cas(int i, SLNode* ep, bool em, SLNode* np, bool nm) {
        uintptr_t e = reinterpret_cast<uintptr_t>(ep) | em;
        uintptr_t n = reinterpret_cast<uintptr_t>(np) | nm;
        return fwd[i].compare_exchange_strong(e, n,
            std::memory_order_release, std::memory_order_relaxed);
    }
};

template<typename K, typename V>
class SkipList {
    using N = SLNode<K, V>;
    static const int LEVELS = 18;

    N* head;
    N* tail;
    HPTable<N> hp;

    int randlvl() {
        thread_local std::mt19937 rng{std::random_device{}()};
        int l = 0;
        while (l < LEVELS-1 && (rng() & 1)) l++;
        return l;
    }

    bool locate(const K& key, N* preds[], N* succs[], int tid) {
        bool found = false;
    retry:
        N* pred = head;
        for (int i = LEVELS-1; i >= 0; i--) {
            N* cur = pred->next(i);
            hp.acquire(tid, 0, *reinterpret_cast<std::atomic<N*>*>(&pred->fwd[i]));
            while (true) {
                N* succ = cur->next(i);
                bool mark = cur->del(i);
                hp.acquire(tid, 1, *reinterpret_cast<std::atomic<N*>*>(&cur->fwd[i]));
                if (mark) {
                    N* old = cur;
                    if (!pred->cas(i, cur, false, succ, false))
                        goto retry;
                    hp.retire(tid, old);
                    cur = pred->next(i);
                    hp.acquire(tid, 0, *reinterpret_cast<std::atomic<N*>*>(&pred->fwd[i]));
                    continue;
                }
                if (cur->key < key) {
                    pred = cur;
                    cur  = succ;
                    hp.acquire(tid, 0, *reinterpret_cast<std::atomic<N*>*>(&pred->fwd[i]));
                } else break;
            }
            if (cur->key == key) found = true;
            preds[i] = pred;
            succs[i] = cur;
        }
        hp.drop(tid, 0);
        hp.drop(tid, 1);
        return found;
    }

public:
    SkipList() {
        head = new N(std::numeric_limits<K>::min(), V{}, LEVELS-1);
        tail = new N(std::numeric_limits<K>::max(), V{}, LEVELS-1);
        for (int i = 0; i < LEVELS; i++)
            head->fwd[i].store(reinterpret_cast<uintptr_t>(tail), std::memory_order_relaxed);
    }

    ~SkipList() {
        N* cur = head;
        while (cur) { N* nx = cur->next(0); delete cur; cur = nx; }
    }

    bool insert(K key, V val) {
        int tid = get_tid();
        int lvl = randlvl();
        N* preds[LEVELS], *succs[LEVELS];

        while (true) {
            if (locate(key, preds, succs, tid)) return false;

            N* nd = new N(key, val, lvl);
            for (int i = 0; i <= lvl; i++)
                nd->fwd[i].store(reinterpret_cast<uintptr_t>(succs[i]), std::memory_order_relaxed);

            if (!preds[0]->cas(0, succs[0], false, nd, false)) {
                delete nd; continue;
            }
            for (int i = 1; i <= lvl; i++) {
                while (!preds[i]->cas(i, succs[i], false, nd, false))
                    locate(key, preds, succs, tid);
            }
            return true;
        }
    }

    bool remove(K key) {
        int tid = get_tid();
        N* preds[LEVELS], *succs[LEVELS];
        if (!locate(key, preds, succs, tid)) return false;

        N* vic = succs[0];
        for (int i = vic->lvl; i >= 1; i--) {
            uintptr_t r;
            do { r = vic->fwd[i].load(std::memory_order_relaxed); }
            while (!(r & 1) && !vic->fwd[i].compare_exchange_weak(
                r, r|1, std::memory_order_acq_rel, std::memory_order_relaxed));
        }
        uintptr_t r;
        bool mine;
        do {
            r = vic->fwd[0].load(std::memory_order_relaxed);
            mine = vic->fwd[0].compare_exchange_strong(
                r, r|1, std::memory_order_acq_rel, std::memory_order_relaxed);
        } while (!mine && !(r & 1));

        if (!mine) return false;
        locate(key, preds, succs, tid);
        hp.retire(tid, vic);
        return true;
    }

    std::optional<V> search(K key) {
        int tid = get_tid();
        N* pred = head;
        for (int i = LEVELS-1; i >= 0; i--) {
            N* cur = pred->next(i);
            while (cur != tail && cur->key < key) {
                pred = cur;
                cur  = pred->next(i);
            }
            if (cur->key == key && !cur->del(0)) return cur->val;
        }
        return std::nullopt;
    }

    bool has(K key) { return search(key).has_value(); }

    void scan(std::function<void(K,V)> fn) {
        N* c = head->next(0);
        while (c != tail) {
            if (!c->del(0)) fn(c->key, c->val);
            c = c->next(0);
        }
    }
};
