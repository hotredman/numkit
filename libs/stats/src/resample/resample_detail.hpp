// libs/.../resample_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by resample.cpp + resample_reg.cpp.
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

// Sample K indices in [0..N-1] given (optional) weights.
std::vector<int> sample_indices(int N, int K, bool with_replacement,
                                const std::vector<double> &weights,
                                numkit::builtin::detail::MatlabMT19937 &gen)
{
    std::vector<int> out;
    out.reserve((size_t)K);

    if (weights.empty()) {
        if (with_replacement) {
            std::uniform_int_distribution<int> ud(0, N - 1);
            for (int i = 0; i < K; ++i) out.push_back(ud(gen));
        } else {
            // Reservoir-style: shuffle 0..N-1 and take first K.
            std::vector<int> all(N);
            std::iota(all.begin(), all.end(), 0);
            std::shuffle(all.begin(), all.end(), gen);
            out.assign(all.begin(), all.begin() + std::min(K, N));
        }
    } else {
        // Weighted sampling.
        std::discrete_distribution<int> dd(weights.begin(), weights.end());
        if (with_replacement) {
            for (int i = 0; i < K; ++i) out.push_back(dd(gen));
        } else {
            // Without replacement: simple rejection. For small K acceptable;
            // for large K relative to N, prefer Walker / Vose alias method.
            std::vector<bool> taken((size_t)N, false);
            int attempts = 0;
            while ((int)out.size() < K && attempts < 10 * K * N) {
                int i = dd(gen);
                if (!taken[(size_t)i]) {
                    taken[(size_t)i] = true;
                    out.push_back(i);
                }
                ++attempts;
            }
        }
    }
    return out;
}

std::vector<double> read_vec(const Value &v) {
    const size_t n = v.numel();
    std::vector<double> out(n);
    for (size_t i = 0; i < n; ++i) out[i] = v.elemAsDouble(i);
    return out;
}

} // anonymous
namespace {
// Shared enumeration: combinations of `items` taken `K` at a time.
Value combnkImpl(const std::vector<double> &items, int K,
                 std::pmr::memory_resource *mr)
{
    const int N = static_cast<int>(items.size());
    if (K < 0)
        throw Error("combnk: K must be non-negative", 0, 0, "combnk", "",
                    "numkit:combnk:badK");
    // MATLAB: choosing K > N elements yields an empty 0xK result (not an
    // error). E.g. combnk(1:4, 5) -> 0x5, combnk(5, 2) -> 0x2 (scalar 5 is
    // the 1-element set {5}, so K=2 > N=1).
    if (K > N)
        return Value::matrix(0, static_cast<size_t>(K), ValueType::DOUBLE, mr);

    // Number of combinations.
    long long C = 1;
    for (int i = 0; i < K; ++i) C = C * (N - i) / (i + 1);

    Value out = Value::matrix((size_t)C, (size_t)K, ValueType::DOUBLE, mr);
    if (C == 0) return out;
    double *od = out.doubleDataMut();
    if (K == 0) return out;

    // Enumerate combinations in lex order by index recursion.
    std::vector<int> idx((size_t)K);
    for (int i = 0; i < K; ++i) idx[(size_t)i] = i;
    long long row = 0;
    while (true) {
        for (int j = 0; j < K; ++j)
            od[(size_t)j * (size_t)C + (size_t)row] =
                items[(size_t)idx[(size_t)j]];
        ++row;
        int j = K - 1;
        while (j >= 0 && idx[(size_t)j] == N - K + j) --j;
        if (j < 0) break;
        ++idx[(size_t)j];
        for (int k = j + 1; k < K; ++k) idx[(size_t)k] = idx[(size_t)k - 1] + 1;
    }
    return out;
}
} // anon

} // namespace numkit::stats
