// src/builtin/src/datafun/random.cpp
//
// Pseudo-random number generators for numkit::builtin.

#include <numkit/builtin/datafun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/ops/rng.hpp>

namespace numkit::builtin {

Value rand(size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    ops::RngContext defaultRng;
    return numkit::ops::rand(defaultRng, rows, cols, 0, mr);
}

Value randn(size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    ops::RngContext defaultRng;
    return numkit::ops::randn(defaultRng, rows, cols, 0, mr);
}

Value randi(int imin, int imax, size_t rows, size_t cols, std::pmr::memory_resource *mr)
{
    ops::RngContext defaultRng;
    return numkit::ops::randi(defaultRng, imin, imax, rows, cols, 0, mr);
}

Value randperm(size_t n, size_t k, std::pmr::memory_resource *mr)
{
    ops::RngContext defaultRng;
    return numkit::ops::randperm(defaultRng, n, k, mr);
}

} // namespace numkit::builtin
