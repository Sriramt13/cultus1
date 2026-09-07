#pragma once
#include <map>
#include <mutex>
#include <optional>

template<typename K, typename V>
struct LockedSL {
    std::map<K,V> m;
    std::mutex mu;

    bool insert(K k, V v) {
        std::lock_guard lk(mu);
        return m.emplace(k, v).second;
    }
    bool remove(K k) {
        std::lock_guard lk(mu);
        return m.erase(k) > 0;
    }
    std::optional<V> search(K k) {
        std::lock_guard lk(mu);
        auto it = m.find(k);
        if (it == m.end()) return std::nullopt;
        return it->second;
    }
    bool has(K k) { return search(k).has_value(); }
};
