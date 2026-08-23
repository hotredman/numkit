// src/builtin/src/specfun/combinatorics.cpp

#include <numkit/builtin/specfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>
#include <numkit/ops/helpers.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "discrete_detail.hpp"

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Number theory
// ════════════════════════════════════════════════════════════════════════


Value primes(double n, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(n) || n < 2)
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);
    const std::uint64_t N = static_cast<std::uint64_t>(std::floor(n));
    ScratchArena scratch(mr);
    // Sieve mask — uint8_t rather than bool to avoid std::pmr::vector<bool>'s
    // bit-packed proxy reference (MSVC's specialisation has caused subtle
    // initialisation bugs here in the past). One byte per slot is also
    // friendlier on the cache for the inner mark loop.
    auto composite = ScratchVec<std::uint8_t>(N + 1, &scratch);
    for (std::uint64_t i = 2; i * i <= N; ++i)
        if (!composite[i])
            for (std::uint64_t j = i * i; j <= N; j += i)
                composite[j] = 1;

    auto primesVec = ScratchVec<double>(&scratch);
    primesVec.reserve(static_cast<size_t>(N / std::log(static_cast<double>(N) + 1.0)) + 1);
    for (std::uint64_t i = 2; i <= N; ++i)
        if (!composite[i])
            primesVec.push_back(static_cast<double>(i));

    auto out = Value::matrix(1, primesVec.size(), ValueType::DOUBLE, mr);
    if (!primesVec.empty())
        std::memcpy(out.doubleDataMut(), primesVec.data(),
                    primesVec.size() * sizeof(double));
    return out;
}

Value isprime(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::COMPLEX)
        throw Error("isprime: complex inputs are not supported",
                     0, 0, "isprime", "", "numkit:isprime:complex");
    auto out = createLike(x, ValueType::LOGICAL, mr);
    uint8_t *dst = out.logicalDataMut();
    const size_t N = x.numel();
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        dst[i] = isPrimeDouble(v) ? 1 : 0;
    }
    return out;
}

Value factor(double n, std::pmr::memory_resource *mr)
{
    std::uint64_t u;
    if (!isExactNonnegInt(n, u))
        throw Error("factor: argument must be a non-negative integer scalar",
                     0, 0, "factor", "", "numkit:factor:badArg");
    if (u == 0 || u == 1) {
        auto r = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        r.doubleDataMut()[0] = static_cast<double>(u);
        return r;
    }
    ScratchArena scratch(mr);
    auto factors = ScratchVec<double>(&scratch);
    std::uint64_t m = u;
    while (m % 2 == 0) { factors.push_back(2.0); m /= 2; }
    for (std::uint64_t p = 3; p * p <= m; p += 2) {
        while (m % p == 0) {
            factors.push_back(static_cast<double>(p));
            m /= p;
        }
    }
    if (m > 1)
        factors.push_back(static_cast<double>(m));

    auto out = Value::matrix(1, factors.size(), ValueType::DOUBLE, mr);
    if (!factors.empty())
        std::memcpy(out.doubleDataMut(), factors.data(),
                    factors.size() * sizeof(double));
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Combinatorics
// ════════════════════════════════════════════════════════════════════════


Value perms(const Value &v, std::pmr::memory_resource *mr)
{
    if (v.type() == ValueType::COMPLEX)
        throw Error("perms: complex inputs are not supported",
                     0, 0, "perms", "", "numkit:perms:complex");
    if (v.isEmpty()) {
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);
    }
    if (!v.dims().isVector())
        throw Error("perms: argument must be a vector",
                     0, 0, "perms", "", "numkit:perms:notVector");

    const size_t n = v.numel();
    if (n > static_cast<size_t>(kPermMaxN))
        throw Error("perms: numel(v) > 11 is not supported (n! is too large)",
                     0, 0, "perms", "", "numkit:perms:tooLarge");

    ScratchArena scratch(mr);
    auto vals = ScratchVec<double>(n, &scratch);
    for (size_t i = 0; i < n; ++i)
        vals[i] = v.elemAsDouble(i);

    ScratchVec<double> cur(vals, &scratch);
    std::sort(cur.begin(), cur.end(), std::greater<double>());

    const size_t totalRows = static_cast<size_t>(permFactorial(static_cast<int>(n)));
    auto out = Value::matrix(totalRows, n, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();

    size_t row = 0;
    do {
        for (size_t c = 0; c < n; ++c)
            dst[c * totalRows + row] = cur[c];
        ++row;
    } while (std::prev_permutation(cur.begin(), cur.end()));

    return out;
}

Value factorial(const Value &n, std::pmr::memory_resource *mr)
{
    if (n.type() == ValueType::COMPLEX)
        throw Error("factorial: complex inputs are not supported",
                     0, 0, "factorial", "", "numkit:factorial:complex");
    auto out = createLike(n, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t N = n.numel();
    for (size_t i = 0; i < N; ++i)
        dst[i] = factorialDouble(n.elemAsDouble(i), "factorial");
    return out;
}

Value nchoosek(double n, double k, std::pmr::memory_resource *mr)
{
    if (!std::isfinite(n) || !std::isfinite(k))
        throw Error("nchoosek: arguments must be finite",
                     0, 0, "nchoosek", "", "numkit:nchoosek:badArg");
    if (n < 0 || k < 0 || n != std::floor(n) || k != std::floor(k))
        throw Error("nchoosek: arguments must be non-negative integers",
                     0, 0, "nchoosek", "", "numkit:nchoosek:badArg");
    if (k > n)
        throw Error("nchoosek: k must satisfy 0 ≤ k ≤ n",
                     0, 0, "nchoosek", "", "numkit:nchoosek:kTooLarge");

    double kk = (k > n - k) ? n - k : k;
    if (kk == 0.0)
        return Value::scalar(1.0, mr);

    double r = 1.0;
    const int kInt = static_cast<int>(kk);
    for (int i = 0; i < kInt; ++i) {
        r = r * (n - static_cast<double>(i)) / static_cast<double>(i + 1);
    }
    return Value::scalar(std::round(r), mr);
}

// nchoosek(v, k) where v is a vector: all k-combinations of the elements of v,
// one per ROW, in lexicographic order of element indices (MATLAB R2025b):
// nchoosek([1 2 3 4],2) = [1 2;1 3;1 4;2 3;2 4;3 4]. k==0 -> 1x0; k==numel ->
// a single row of all elements. Result is DOUBLE (numeric input flattened).
Value nchoosekCombinations(const Value &v, double kd, std::pmr::memory_resource *mr)
{
    const size_t n = v.numel();
    if (!std::isfinite(kd) || kd < 0 || kd != std::floor(kd))
        throw Error("nchoosek: K must be a non-negative integer",
                     0, 0, "nchoosek", "", "numkit:nchoosek:badArg");
    const size_t k = static_cast<size_t>(kd);
    if (k > n)
        throw Error("nchoosek: K must satisfy 0 <= K <= numel(V)",
                     0, 0, "nchoosek", "", "numkit:nchoosek:kTooLarge");
    if (k == 0)
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    auto vd = ScratchVec<double>(n, &scratch);
    for (size_t i = 0; i < n; ++i) vd[i] = v.elemAsDouble(i);

    // Row count R = C(n,k) (computed with the symmetric product to limit error).
    const size_t kk = (k > n - k) ? n - k : k;
    double rd = 1.0;
    for (size_t i = 0; i < kk; ++i)
        rd = rd * static_cast<double>(n - i) / static_cast<double>(i + 1);
    const size_t R = static_cast<size_t>(std::round(rd));

    auto out = Value::matrix(R, k, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    auto idx = ScratchVec<size_t>(k, &scratch);
    for (size_t i = 0; i < k; ++i) idx[i] = i;
    size_t row = 0;
    while (true) {
        for (size_t j = 0; j < k; ++j) od[j * R + row] = vd[idx[j]];
        ++row;
        // Advance to the next lexicographic combination of indices.
        size_t i = k;
        while (i-- > 0) {
            if (idx[i] != n - k + i) {
                ++idx[i];
                for (size_t j = i + 1; j < k; ++j) idx[j] = idx[j - 1] + 1;
                break;
            }
            if (i == 0) { row = R; }   // exhausted (sentinel: stop outer loop)
        }
        if (row >= R) break;
    }
    return out;
}


Value colperm(const Value &s, std::pmr::memory_resource *mr)
{
    const auto &d = s.dims();
    if (d.ndim() > 2)
        throw Error("colperm: input must be 2-D", 0, 0, "colperm", "", "numkit:colperm:rank");
    const size_t rows = d.rows();
    const size_t cols = d.cols();
    if (cols == 0)
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    struct ColCount {
        size_t colIndex;
        size_t nnz;
    };
    auto counts = ScratchVec<ColCount>(&scratch);
    counts.reserve(cols);

    for (size_t c = 0; c < cols; ++c) {
        size_t nonzeros = 0;
        for (size_t r = 0; r < rows; ++r) {
            const size_t idx = c * rows + r;
            if (s.type() == ValueType::COMPLEX) {
                const auto val = s.complexData()[idx];
                if (val.real() != 0.0 || val.imag() != 0.0) ++nonzeros;
            } else {
                if (s.elemAsDouble(idx) != 0.0) ++nonzeros;
            }
        }
        counts.push_back({ c + 1, nonzeros });
    }

    std::stable_sort(counts.begin(), counts.end(), [](const ColCount &a, const ColCount &b) {
        return a.nnz < b.nnz;
    });

    auto out = Value::matrix(1, cols, ValueType::DOUBLE, mr);
    double *p = out.doubleDataMut();
    for (size_t i = 0; i < cols; ++i)
        p[i] = static_cast<double>(counts[i].colIndex);
    return out;
}

Value symrcm(const Value &s, std::pmr::memory_resource *mr)
{
    const auto &d = s.dims();
    if (d.ndim() != 2 || d.rows() != d.cols())
        throw Error("symrcm: S must be a square 2-D matrix", 0, 0, "symrcm", "", "numkit:symrcm:notSquare");
    const size_t n = d.rows();
    if (n == 0)
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);

    ScratchArena scratch(mr);
    std::vector<std::vector<size_t>> adj(n);

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (i == j) continue;
            bool nonz = false;
            if (s.type() == ValueType::COMPLEX) {
                auto v1 = s.complexData()[j * n + i];
                auto v2 = s.complexData()[i * n + j];
                nonz = (v1.real() != 0.0 || v1.imag() != 0.0 || v2.real() != 0.0 || v2.imag() != 0.0);
            } else {
                nonz = (s.elemAsDouble(j * n + i) != 0.0 || s.elemAsDouble(i * n + j) != 0.0);
            }
            if (nonz)
                adj[i].push_back(j);
        }
    }

    // Sort adj[i] by degree (ascending), tie-break by node index
    for (size_t i = 0; i < n; ++i) {
        std::sort(adj[i].begin(), adj[i].end(), [&](size_t u, size_t v) {
            if (adj[u].size() != adj[v].size())
                return adj[u].size() < adj[v].size();
            return u < v;
        });
    }

    auto visited = ScratchVec<uint8_t>(n, 0, &scratch);
    auto order = ScratchVec<size_t>(&scratch);
    order.reserve(n);

    for (size_t startCand = 0; startCand < n; ++startCand) {
        if (visited[startCand]) continue;

        size_t start = startCand;
        for (size_t i = startCand + 1; i < n; ++i) {
            if (!visited[i] && adj[i].size() < adj[start].size()) {
                start = i;
            }
        }

        auto comp = ScratchVec<size_t>(&scratch);
        auto queue = ScratchVec<size_t>(&scratch);
        size_t head = 0;

        visited[start] = 1;
        queue.push_back(start);
        comp.push_back(start);

        while (head < queue.size()) {
            size_t u = queue[head++];
            for (size_t v : adj[u]) {
                if (!visited[v]) {
                    visited[v] = 1;
                    queue.push_back(v);
                    comp.push_back(v);
                }
            }
        }

        // Reverse the component BFS order
        std::reverse(comp.begin(), comp.end());
        for (size_t node : comp)
            order.push_back(node);
    }

    auto out = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *p = out.doubleDataMut();
    for (size_t i = 0; i < n; ++i)
        p[i] = static_cast<double>(order[i] + 1);
    return out;
}

} // namespace numkit::builtin
