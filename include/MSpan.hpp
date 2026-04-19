// include/MSpan.hpp
#pragma once

#include <cstddef>
#include <vector>

namespace numkit {

template<typename T>
class Span
{
public:
    Span()
        : data_(nullptr)
        , size_(0)
    {}
    Span(T *d, size_t n)
        : data_(d)
        , size_(n)
    {}

    // Construct from vector (const or non-const depending on T)
    template<typename U = T, typename std::enable_if<std::is_const<U>::value, int>::type = 0>
    Span(const std::vector<typename std::remove_const<T>::type> &v)
        : data_(v.data())
        , size_(v.size())
    {}

    template<typename U = T, typename std::enable_if<!std::is_const<U>::value, int>::type = 0>
    Span(std::vector<T> &v)
        : data_(v.data())
        , size_(v.size())
    {}

    T &operator[](size_t i) const { return data_[i]; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    T *data() const { return data_; }
    T *begin() const { return data_; }
    T *end() const { return data_ + size_; }

    Span subspan(size_t offset) const { return {data_ + offset, size_ - offset}; }
    Span subspan(size_t offset, size_t count) const { return {data_ + offset, count}; }

private:
    T *data_;
    size_t size_;
};

} // namespace numkit