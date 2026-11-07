// std::initializer_list for the freestanding build (no C++ library): lets the checks loop over braced
// lists (for (DWORD n : {1u, 2u})). Layout as clang expects it: pointer and length.
#pragma once
#include <stddef.h>

namespace std {
template <class E> class initializer_list {
    const E *first_;
    size_t size_;
    constexpr initializer_list(const E *first, size_t size) : first_(first), size_(size) {}

public:
    constexpr initializer_list() : first_(nullptr), size_(0) {}
    constexpr size_t size() const { return size_; }
    constexpr const E *begin() const { return first_; }
    constexpr const E *end() const { return first_ + size_; }
};
}  // namespace std
