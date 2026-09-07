#pragma once
#include "hazard.hpp"
#include <atomic>
#include <optional>
#include <random>
#include <functional>
#include <limits>

namespace cultus {

static constexpr int MAXLVL = 16;

inline int tid() {
    static std::atomic<int> counter{0};
    thread_local int id = counter.fetch_add(1, std::memory_order_relaxed);
    return id;
}

template<typename K, typename V>
struct Node {
    K   key;
    V   val;
    int height;
    std::atomic<uintptr_t> next[MAXLVL];

    Node(K k, V v, int h) : key(std::move(k)), val(std::move(v)), height(h) {
        for (auto& n : next) n.store(0, std::memory_order_relaxed);
    }

    Node* succ(int lvl) const {
        return reinterpret_cast<Node*>(next[lvl].load(std::memory_order_acquire) & ~uintptr_t(1));
    }

    bool marked(int lvl) const {
        return next[lvl].load(std::memory_order_acquire) & 1;
    }

    bool cas(int lvl, Node* exp_ptr, bool exp_mark, Node* new_ptr, bool new_mark) {
        uintptr_t expected = reinterpret_cast<uintptr_t>(exp_ptr) | exp_mark;
        uintptr_t desired  = reinterpret_cast<uintptr_t>(new_ptr) | new_mark;
        return next[lvl].compare_exchange_strong(
            expected, desired, std::memory_order_release, std::memory_order_relaxed);
    }
};

template<typename K, typename V>
class SkipList {
    using N = Node<K, V>;

public:
    SkipList()
        : head_(new N(std::numeric_limits<K>::min(), V{}, MAXLVL - 1)),
          tail_(new N(std::numeric_limits<K>::max(), V{}, MAXLVL - 1)) {
        for (int i = 0; i < MAXLVL; i++)
            head_->next[i].store(reinterpret_cast<uintptr_t>(tail_), std::memory_order_relaxed);
    }

    ~SkipList() {
        N* cur = head_;
        while (cur) {
            N* nxt = cur->succ(0);
            delete cur;
            cur = nxt;
        }
    }

    bool insert(K key, V val) {
        int  me     = tid();
        int  lvl    = pick_level();
        N*   preds[MAXLVL];
        N*   succs[MAXLVL];

        while (true) {
            if (locate(key, preds, succs, me)) {
                hp_.clear(0, me);
                hp_.clear(1, me);
                return false;
            }

            N* node = new N(key, val, lvl);
            for (int i = 0; i <= lvl; i++)
                node->next[i].store(reinterpret_cast<uintptr_t>(succs[i]), std::memory_order_relaxed);

            if (!preds[0]->cas(0, succs[0], false, node, false)) {
                delete node;
                continue;
            }

            for (int i = 1; i <= lvl; i++) {
                while (!preds[i]->cas(i, succs[i], false, node, false))
                    locate(key, preds, succs, me);
            }

            hp_.clear(0, me);
            hp_.clear(1, me);
            return true;
        }
    }

    bool remove(K key) {
        int me = tid();
        N*  preds[MAXLVL];
        N*  succs[MAXLVL];

        if (!locate(key, preds, succs, me)) {
            hp_.clear(0, me);
            hp_.clear(1, me);
            return false;
        }

        N* victim = succs[0];

        for (int i = victim->height; i >= 1; i--) {
            uintptr_t raw;
            do {
                raw = victim->next[i].load(std::memory_order_relaxed);
            } while (!(raw & 1) &&
                     !victim->next[i].compare_exchange_weak(
                         raw, raw | 1,
                         std::memory_order_acq_rel,
                         std::memory_order_relaxed));
        }

        uintptr_t raw;
        bool mine;
        do {
            raw  = victim->next[0].load(std::memory_order_relaxed);
            mine = victim->next[0].compare_exchange_strong(
                raw, raw | 1, std::memory_order_acq_rel, std::memory_order_relaxed);
        } while (!mine && !(raw & 1));

        if (mine) {
            locate(key, preds, succs, me);
            hp_.clear(0, me);
            hp_.clear(1, me);
            hp_.retire(victim, me);
            return true;
        }

        hp_.clear(0, me);
        hp_.clear(1, me);
        return false;
    }

    std::optional<V> find(K key) {
        int me = tid();
        N* pred = head_;
        N* cur  = nullptr;

        for (int i = MAXLVL - 1; i >= 0; i--) {
            cur = hp_.protect(0, *reinterpret_cast<std::atomic<N*>*>(&pred->next[i]), me);
            while (cur != tail_ && cur->key < key) {
                pred = cur;
                cur  = hp_.protect(0, *reinterpret_cast<std::atomic<N*>*>(&pred->next[i]), me);
            }
        }

        hp_.clear(0, me);
        if (cur != tail_ && cur->key == key && !cur->marked(0))
            return cur->val;
        return std::nullopt;
    }

    bool contains(K key) { return find(key).has_value(); }

    void for_each(std::function<void(const K&, const V&)> fn) {
        N* cur = head_->succ(0);
        while (cur != tail_) {
            if (!cur->marked(0)) fn(cur->key, cur->val);
            cur = cur->succ(0);
        }
    }

private:
    bool locate(const K& key, N* preds[], N* succs[], int me) {
        bool found = false;

    retry:
        N* pred = head_;
        for (int i = MAXLVL - 1; i >= 0; i--) {
            uintptr_t raw = pred->next[i].load(std::memory_order_acquire);
            N* cur  = reinterpret_cast<N*>(raw & ~uintptr_t(1));
            hp_.protect(0, *reinterpret_cast<std::atomic<N*>*>(&pred->next[i]), me);

            while (true) {
                uintptr_t cur_raw  = cur->next[i].load(std::memory_order_acquire);
                N*        succ     = reinterpret_cast<N*>(cur_raw & ~uintptr_t(1));
                bool      cur_mark = cur_raw & 1;
                hp_.protect(1, *reinterpret_cast<std::atomic<N*>*>(&cur->next[i]), me);

                if (cur_mark) {
                    if (!pred->cas(i, cur, false, succ, false))
                        goto retry;
                    hp_.retire(cur, me);
                    cur  = succ;
                    hp_.protect(0, *reinterpret_cast<std::atomic<N*>*>(&pred->next[i]), me);
                    continue;
                }

                if (cur->key < key) {
                    pred = cur;
                    cur  = succ;
                    hp_.protect(0, *reinterpret_cast<std::atomic<N*>*>(&pred->next[i]), me);
                } else {
                    break;
                }
            }

            if (cur->key == key) found = true;
            preds[i] = pred;
            succs[i] = cur;
        }
        return found;
    }

    int pick_level() {
        thread_local std::mt19937 rng(std::random_device{}());
        thread_local std::uniform_int_distribution<uint32_t> dist;
        int lvl = 0;
        while (lvl < MAXLVL - 1 && (dist(rng) & 1)) lvl++;
        return lvl;
    }

    N* head_;
    N* tail_;
    HazardTable<N> hp_;
};

}
