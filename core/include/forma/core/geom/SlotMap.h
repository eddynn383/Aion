#pragma once

#include <cassert>
#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace forma::geom {

// Colecție de entități cu ID-uri stabile: stocare densă (iterare rapidă) plus index ID → poziție.
// ID-urile nu se refolosesc; undo poate reinsera o entitate cu ID-ul ei vechi.
template <class Key, class T>
class SlotMap {
public:
    Key insert(T value)
    {
        Key key { ++m_lastId };
        insertWithId(key, std::move(value));
        return key;
    }

    void insertWithId(Key key, T value)
    {
        assert(key.valid() && !contains(key));
        m_index.emplace(key.value, m_values.size());
        m_keys.push_back(key);
        m_values.push_back(std::move(value));
        if (key.value > m_lastId) {
            m_lastId = key.value;
        }
    }

    bool erase(Key key)
    {
        const auto it = m_index.find(key.value);
        if (it == m_index.end()) {
            return false;
        }
        const std::size_t pos = it->second;
        const std::size_t last = m_values.size() - 1;
        if (pos != last) {
            m_values[pos] = std::move(m_values[last]);
            m_keys[pos] = m_keys[last];
            m_index[m_keys[pos].value] = pos;
        }
        m_values.pop_back();
        m_keys.pop_back();
        m_index.erase(it);
        return true;
    }

    [[nodiscard]] bool contains(Key key) const { return m_index.contains(key.value); }

    [[nodiscard]] T* find(Key key)
    {
        const auto it = m_index.find(key.value);
        return it == m_index.end() ? nullptr : &m_values[it->second];
    }
    [[nodiscard]] const T* find(Key key) const
    {
        const auto it = m_index.find(key.value);
        return it == m_index.end() ? nullptr : &m_values[it->second];
    }

    [[nodiscard]] T& at(Key key)
    {
        T* v = find(key);
        assert(v != nullptr);
        return *v;
    }
    [[nodiscard]] const T& at(Key key) const
    {
        const T* v = find(key);
        assert(v != nullptr);
        return *v;
    }

    [[nodiscard]] std::size_t size() const { return m_values.size(); }
    [[nodiscard]] bool empty() const { return m_values.empty(); }

    // Cheile, în ordinea stocării (se schimbă la ștergere).
    [[nodiscard]] const std::vector<Key>& keys() const { return m_keys; }
    [[nodiscard]] const std::vector<T>& values() const { return m_values; }

private:
    std::vector<T> m_values;
    std::vector<Key> m_keys;
    std::unordered_map<std::uint64_t, std::size_t> m_index;
    std::uint64_t m_lastId = 0;
};

} // namespace forma::geom
