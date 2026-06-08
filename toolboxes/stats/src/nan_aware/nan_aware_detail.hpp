// toolboxes/.../nan_aware_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by nan_aware.cpp + nan_aware_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

void validateNormFlag(int w, const char *fn)
{
    if (w != 0 && w != 1)
        throw Error(std::string(fn) + ": normalization flag must be 0 or 1",
                     0, 0, fn, "", std::string("numkit:") + fn + ":badFlag");
}

double medianFromSlice(double *data, size_t n)
{
    if (n == 0) return std::nan("");
    if (n == 1) return data[0];
    const size_t mid = n / 2;
    std::nth_element(data, data + mid, data + n);
    if (n % 2 == 1)
        return data[mid];
    const double upper = data[mid];
    const double lower = *std::max_element(data, data + mid);
    return 0.5 * (lower + upper);
}

} // namespace
namespace {

// Read x as a column-major n×p matrix; vector → n×1.
void readObsMatrix(const Value &x, ScratchVec<double> &out,
                    std::size_t &n, std::size_t &p)
{
    if (x.isEmpty()) { n = p = 0; return; }
    if (x.dims().isVector()) {
        n = x.numel();
        p = 1;
    } else {
        n = x.dims().rows();
        p = x.dims().cols();
    }
    out.resize(n * p);
    if (x.type() == ValueType::DOUBLE) {
        // Column-major copy — same layout as Value storage.
        const double *src = x.doubleData();
        std::memcpy(out.data(), src, n * p * sizeof(double));
    } else {
        for (std::size_t k = 0; k < n * p; ++k)
            out[k] = x.elemAsDouble(k);
    }
}

// Drop rows where any column has NaN. Compacts in place. Returns the
// new row count.
std::size_t dropNanRows(double *data, std::size_t n, std::size_t p)
{
    std::size_t w = 0;
    for (std::size_t r = 0; r < n; ++r) {
        bool keep = true;
        for (std::size_t c = 0; c < p; ++c) {
            if (std::isnan(data[c * n + r])) { keep = false; break; }
        }
        if (keep) {
            // Copy row r to row w (column by column).
            if (w != r)
                for (std::size_t c = 0; c < p; ++c)
                    data[c * n + w] = data[c * n + r];
            ++w;
        }
    }
    // Tail rows [w..n) on each column are now garbage — but the kept
    // count is what we use downstream. Caller treats `w` as new n.
    return w;
}

// In-place column centering for an n_kept × p block stored with stride
// n_orig (NOT compacted). To keep things simple we instead PACK the
// kept rows into a fresh contiguous n_kept × p layout before centering.
void packKeptRows(const double *src, std::size_t n_orig,
                   std::size_t n_kept, std::size_t p, double *dst)
{
    for (std::size_t c = 0; c < p; ++c)
        for (std::size_t r = 0; r < n_kept; ++r)
            dst[c * n_kept + r] = src[c * n_orig + r];
}

void centerColumnsLocal(double *data, std::size_t n, std::size_t p)
{
    for (std::size_t c = 0; c < p; ++c) {
        double s = 0.0;
        for (std::size_t r = 0; r < n; ++r) s += data[c * n + r];
        const double m = s / static_cast<double>(n);
        for (std::size_t r = 0; r < n; ++r) data[c * n + r] -= m;
    }
}

Value covFromCenteredLocal(const double *X, std::size_t n,
                            std::size_t p, double divisor,
                            std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(p, p, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < p; ++j) {
            double s = 0.0;
            for (std::size_t r = 0; r < n; ++r)
                s += X[i * n + r] * X[j * n + r];
            dst[j * p + i] = s / divisor;
        }
    return out;
}

} // namespace

} // namespace numkit::stats
