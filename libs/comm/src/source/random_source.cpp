// libs/comm/src/source/random_source.cpp
//
// Random data sources: randsrc.
// Future: randerr planned for the same TU (random binary error matrix).

#include <numkit/comm/source/random_source.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/random/matlab_mt19937.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <numeric>

namespace numkit::comm {

namespace {

// Gather alphabet + cumulative-probability arrays from the alphabet
// argument. MATLAB accepts:
//   row vector       -> alphabet, uniform probabilities
//   2-row matrix     -> [alphabet; probabilities]
// Returns the symbol count K via len_out.
void parseAlphabet(const Value &a,
                   std::pmr::memory_resource *mr,
                   double **alpha_out, double **prob_out, size_t *len_out)
{
    const size_t R = a.dims().rows();
    const size_t C = a.dims().cols();

    if (R == 1 && C >= 1) {
        // Plain alphabet row -> uniform probs.
        const size_t K = C;
        auto *al = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        auto *pr = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        for (size_t k = 0; k < K; ++k) {
            al[k] = a.elemAsDouble(k);
            pr[k] = static_cast<double>(k + 1) / static_cast<double>(K);
        }
        *alpha_out = al;
        *prob_out  = pr;
        *len_out   = K;
        return;
    }

    if (R == 2 && C >= 1) {
        const size_t K = C;
        auto *al = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        auto *pr = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        // Column-major: alphabet[k] = a(0, k) at index 0 + k*R = k*2;
        // prob[k] = a(1, k) at index 1 + k*2.
        double sum = 0.0;
        for (size_t k = 0; k < K; ++k) {
            const double sym = a.elemAsDouble(k * R + 0);
            const double pk  = a.elemAsDouble(k * R + 1);
            if (!(pk >= 0.0 && pk <= 1.0))
                throw Error("randsrc: probabilities must be in [0, 1]",
                            0, 0, "randsrc", "",
                            "numkit:randsrc:InvalidProbabilityVal");
            al[k] = sym;
            sum  += pk;
            pr[k] = sum;          // cumulative
        }
        if (std::abs(sum - 1.0) > std::sqrt(2.220446049250313e-16))
            throw Error("randsrc: probabilities must sum to 1",
                        0, 0, "randsrc", "",
                        "numkit:randsrc:InvalidProbabilitySum");
        *alpha_out = al;
        *prob_out  = pr;
        *len_out   = K;
        return;
    }

    if (R == 1 && C == 0)
        throw Error("randsrc: alphabet must be non-empty",
                    0, 0, "randsrc", "", "numkit:randsrc:EmptyAlphabet");

    throw Error("randsrc: alphabet must be a row vector or 2-row matrix",
                0, 0, "randsrc", "", "numkit:randsrc:InvalidAlphabet");
}

} // namespace

Value randsrc(size_t m, size_t n, const Value &alphabet,
              bool have_state, uint32_t state,
              std::pmr::memory_resource *mr)
{
    using ::numkit::builtin::detail::MatlabMT19937;

    // Parse alphabet → cumulative probability table.
    double *alpha = nullptr;
    double *prob  = nullptr;
    size_t  K     = 0;
    parseAlphabet(alphabet, mr, &alpha, &prob, &K);

    Value out = Value::matrix(m, n, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    const size_t N = m * n;

    // Choose generator. With explicit state -> dedicated engine for
    // reproducibility (matches MATLAB's RandStream(...,'Seed',state)).
    if (have_state) {
        MatlabMT19937 local;
        local.seed(state);
        for (size_t i = 0; i < N; ++i) {
            const double r = local.genRes53();
            // idx = 1 + sum(r >= prob(k)) → (1-based) → 0-based = sum(r >= prob).
            size_t idx = 0;
            while (idx < K - 1 && r >= prob[idx]) ++idx;
            o[i] = alpha[idx];
        }
    } else {
        auto &gen = ::numkit::builtin::sharedEngine();
        auto &mtx = ::numkit::builtin::rngMutex();
        std::lock_guard<std::mutex> lk(mtx);
        for (size_t i = 0; i < N; ++i) {
            const double r = gen.genRes53();
            size_t idx = 0;
            while (idx < K - 1 && r >= prob[idx]) ++idx;
            o[i] = alpha[idx];
        }
    }

    return out;
}

// ── randerr ────────────────────────────────────────────────────────
// Random bit-error matrix. Per row i:
//   - draw the error count from the probability CDF;
//   - choose that many random column positions by sorting a vector of
//     random keys and marking the lowest-keyed columns (a standard
//     random-subset selection).
//
// `errspec` is one of:
//   - scalar k                            -> exactly k errors per row
//   - row vector [k1 k2 ...]              -> uniform pick from {k1, k2, ...}
//   - 2-row matrix [counts; probabilities] -> weighted pick
namespace {

void parseErrspec(const Value &e, std::pmr::memory_resource *mr,
                  int **counts_out, double **prob_out, size_t *len_out,
                  size_t n)
{
    const size_t R = e.dims().rows();
    const size_t C = e.dims().cols();

    if (R == 1 && C >= 1) {
        const size_t K = C;
        auto *cs = static_cast<int *>(
            mr->allocate(K * sizeof(int), alignof(int)));
        auto *pr = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        for (size_t k = 0; k < K; ++k) {
            const double v = e.elemAsDouble(k);
            if (!(v >= 0.0) || std::floor(v) != v)
                throw Error("randerr: error counts must be non-negative integers",
                            0, 0, "randerr", "",
                            "numkit:randerr:InvalidErrorsElements");
            if (static_cast<size_t>(v) > n)
                throw Error("randerr: error count exceeds n",
                            0, 0, "randerr", "",
                            "numkit:randerr:InvalidErrorsForm");
            cs[k] = static_cast<int>(v);
            pr[k] = static_cast<double>(k + 1) / static_cast<double>(K);
        }
        *counts_out = cs;
        *prob_out   = pr;
        *len_out    = K;
        return;
    }

    if (R == 2 && C >= 1) {
        const size_t K = C;
        auto *cs = static_cast<int *>(
            mr->allocate(K * sizeof(int), alignof(int)));
        auto *pr = static_cast<double *>(
            mr->allocate(K * sizeof(double), alignof(double)));
        double sum = 0.0;
        for (size_t k = 0; k < K; ++k) {
            const double v = e.elemAsDouble(k * R + 0);
            const double p = e.elemAsDouble(k * R + 1);
            if (!(v >= 0.0) || std::floor(v) != v)
                throw Error("randerr: error counts must be non-negative integers",
                            0, 0, "randerr", "",
                            "numkit:randerr:InvalidErrorsElements");
            if (static_cast<size_t>(v) > n)
                throw Error("randerr: error count exceeds n",
                            0, 0, "randerr", "",
                            "numkit:randerr:InvalidErrorsForm");
            if (!(p >= 0.0 && p <= 1.0))
                throw Error("randerr: probabilities must be in [0, 1]",
                            0, 0, "randerr", "",
                            "numkit:randerr:Invalid2ndRowErrorsVal");
            cs[k] = static_cast<int>(v);
            sum += p;
            pr[k] = sum;
        }
        if (std::abs(sum - 1.0) > std::sqrt(2.220446049250313e-16))
            throw Error("randerr: probabilities must sum to 1",
                        0, 0, "randerr", "",
                        "numkit:randerr:InvalidProbabilitySum");
        *counts_out = cs;
        *prob_out   = pr;
        *len_out    = K;
        return;
    }

    throw Error("randerr: errors must be a row vector or 2-row matrix",
                0, 0, "randerr", "", "numkit:randerr:InvalidErrorsDims");
}

template <typename Rng>
void fillOneRow(double *o_col_major, size_t i, size_t m, size_t n,
                const int *counts, const double *prob, size_t K, Rng &rng,
                ScratchVec<double> &rs, ScratchVec<size_t> &p)
{
    // 1) Pick error count from CDF.
    const double u = rng.genRes53();
    size_t k = 0;
    while (k < K - 1 && u >= prob[k]) ++k;
    const int num = counts[k];

    // 2) n random values + sort permutation.
    for (size_t j = 0; j < n; ++j) rs[j] = rng.genRes53();
    std::iota(p.begin(), p.end(), size_t{0});
    std::sort(p.begin(), p.end(),
              [&](size_t a, size_t b) { return rs[a] < rs[b]; });

    // 3) out(i, j) = (p[j] < num)  (0-based, mirroring p<=num 1-based).
    for (size_t j = 0; j < n; ++j) {
        const bool one = (p[j] < static_cast<size_t>(num));
        o_col_major[i + j * m] = one ? 1.0 : 0.0;
    }
}

} // namespace

Value randerr(size_t m, size_t n, const Value &errspec,
              bool have_state, uint32_t state,
              std::pmr::memory_resource *mr)
{
    using ::numkit::builtin::detail::MatlabMT19937;

    int    *counts = nullptr;
    double *prob   = nullptr;
    size_t  K      = 0;
    parseErrspec(errspec, mr, &counts, &prob, &K, n);

    Value out = Value::matrix(m, n, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    std::fill(o, o + m * n, 0.0);

    ScratchArena scratch(mr);
    ScratchVec<double> rs(n, &scratch);
    ScratchVec<size_t> p(n, &scratch);

    if (have_state) {
        MatlabMT19937 local;
        local.seed(state);
        for (size_t i = 0; i < m; ++i)
            fillOneRow(o, i, m, n, counts, prob, K, local, rs, p);
    } else {
        auto &gen = ::numkit::builtin::sharedEngine();
        auto &mtx = ::numkit::builtin::rngMutex();
        std::lock_guard<std::mutex> lk(mtx);
        for (size_t i = 0; i < m; ++i)
            fillOneRow(o, i, m, n, counts, prob, K, gen, rs, p);
    }

    return out;
}

} // namespace numkit::comm
