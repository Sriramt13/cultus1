#pragma once
#include <map>
#include <mutex>
#include <optional>
#include <functional>

namespace cultus {

template<typename K, typename V>
class LockedSkipList {
public:
    bool insert(K key, V val) {
        std::lock_guard<std::mutex> lk(mu_);
        return data_.emplace(key, val).second;
    }

    bool remove(K key) {
        std::lock_guard<std::mutex> lk(mu_);
        return data_.erase(key) > 0;
    }

    std::optional<V> find(K key) {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = data_.find(key);
        if (it == data_.end()) return std::nullopt;
        return it->second;
    }

    bool contains(K key) { return find(key).has_value(); }

    void for_each(std::function<void(const K&, const V&)> fn) {
        std::lock_guard<std::mutex> lk(mu_);
        for (auto& [k, v] : data_) fn(k, v);
    }

private:
    std::map<K, V> data_;
    std::mutex     mu_;
};

}
