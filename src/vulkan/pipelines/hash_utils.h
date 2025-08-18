#pragma once

#include <functional>
#include <cstddef>
#include <vector>

namespace VulkanHash {

class HashCombiner {
public:
    HashCombiner() : hash_(0) {}
    explicit HashCombiner(size_t seed) : hash_(seed) {}

    template<typename T>
    HashCombiner& combine(const T& value) {
        hash_ ^= std::hash<T>{}(value) + 0x9e3779b9 + (hash_ << 6) + (hash_ >> 2);
        return *this;
    }

    template<typename Container>
    HashCombiner& combineContainer(const Container& container) {
        for (const auto& item : container) {
            combine(item);
        }
        return *this;
    }

    template<typename T>
    HashCombiner& combineArray(const T* array, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            combine(array[i]);
        }
        return *this;
    }

    size_t get() const { return hash_; }
    operator size_t() const { return hash_; }

private:
    size_t hash_;
};

template<typename T>
inline size_t hash_value(const T& value) {
    return HashCombiner().combine(value).get();
}

template<typename Container>
inline size_t hash_container(const Container& container) {
    return HashCombiner().combineContainer(container).get();
}

template<typename T1, typename T2>
inline size_t hash_combine(const T1& v1, const T2& v2) {
    return HashCombiner().combine(v1).combine(v2).get();
}

template<typename T1, typename T2, typename T3>
inline size_t hash_combine(const T1& v1, const T2& v2, const T3& v3) {
    return HashCombiner().combine(v1).combine(v2).combine(v3).get();
}

template<typename T1, typename T2, typename T3, typename T4>
inline size_t hash_combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4) {
    return HashCombiner().combine(v1).combine(v2).combine(v3).combine(v4).get();
}

template<typename T1, typename T2, typename T3, typename T4, typename T5>
inline size_t hash_combine(const T1& v1, const T2& v2, const T3& v3, const T4& v4, const T5& v5) {
    return HashCombiner().combine(v1).combine(v2).combine(v3).combine(v4).combine(v5).get();
}

inline size_t hash_shift_combine(size_t h1, size_t h2) {
    return h1 ^ (h2 << 1);
}

inline size_t hash_shift_combine(size_t h1, size_t h2, size_t h3) {
    return h1 ^ (h2 << 1) ^ (h3 << 2);
}

inline size_t hash_shift_combine(size_t h1, size_t h2, size_t h3, size_t h4) {
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
}

}