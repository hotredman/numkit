// toolboxes/linalg/benchmarks/bench_linalg.cpp
//
// Linear algebra benchmark suite for lu, qr, chol, mldivide, eig, svd.

#include <numkit/linalg/decompositions.hpp>
#include <numkit/linalg/eig.hpp>
#include <numkit/linalg/properties.hpp>
#include <numkit/linalg/solvers.hpp>
#include <numkit/value/value.hpp>

#include <benchmark/benchmark.h>

#include <complex>
#include <random>

namespace {

using namespace numkit;
using namespace numkit::linalg;

Value makeRealMatrix(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value v = Value::matrix(n, n, ValueType::DOUBLE, nullptr);
    double *data = v.doubleDataMut();
    for (size_t i = 0; i < n * n; ++i) {
        data[i] = dist(rng);
    }
    return v;
}

Value makePosDefMatrix(size_t n, uint32_t seed) {
    Value A = makeRealMatrix(n, seed);
    Value A_t = Value::matrix(n, n, ValueType::DOUBLE, nullptr);
    const double *ad = A.doubleData();
    double *atd = A_t.doubleDataMut();
    for (size_t col = 0; col < n; ++col) {
        for (size_t row = 0; row < n; ++row) {
            atd[row + col * n] = ad[col + row * n];
        }
    }
    // A * A' + n * I
    Value pd = Value::matrix(n, n, ValueType::DOUBLE, nullptr);
    double *pdd = pd.doubleDataMut();
    std::fill(pdd, pdd + n * n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (size_t k = 0; k < n; ++k) {
                s += ad[i + k * n] * atd[k + j * n];
            }
            pdd[i + j * n] = s + (i == j ? static_cast<double>(n) : 0.0);
        }
    }
    return pd;
}

Value makeComplexMatrix(size_t n, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    Value v = Value::complexMatrix(n, n, nullptr);
    std::complex<double> *data = v.complexDataMut();
    for (size_t i = 0; i < n * n; ++i) {
        data[i] = std::complex<double>(dist(rng), dist(rng));
    }
    return v;
}

static void BM_Linalg_LU_Real(benchmark::State &state) {
    const size_t n = static_cast<size_t>(state.range(0));
    Value A = makeRealMatrix(n, 42);
    for (auto _ : state) {
        auto res = lu_decompose(A, nullptr);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_Linalg_LU_Real)->Arg(64)->Arg(128)->Arg(256);

static void BM_Linalg_LU_Complex(benchmark::State &state) {
    const size_t n = static_cast<size_t>(state.range(0));
    Value A = makeComplexMatrix(n, 42);
    for (auto _ : state) {
        auto res = lu_decompose(A, nullptr);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_Linalg_LU_Complex)->Arg(64)->Arg(128)->Arg(256);

static void BM_Linalg_Chol_Real(benchmark::State &state) {
    const size_t n = static_cast<size_t>(state.range(0));
    Value A = makePosDefMatrix(n, 42);
    for (auto _ : state) {
        auto res = chol(A, nullptr);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_Linalg_Chol_Real)->Arg(64)->Arg(128)->Arg(256);

static void BM_Linalg_Solve_Real(benchmark::State &state) {
    const size_t n = static_cast<size_t>(state.range(0));
    Value A = makeRealMatrix(n, 42);
    Value B = makeRealMatrix(n, 43);
    for (auto _ : state) {
        auto res = linsolve(A, B, nullptr);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_Linalg_Solve_Real)->Arg(64)->Arg(128)->Arg(256);

static void BM_Linalg_Solve_Complex(benchmark::State &state) {
    const size_t n = static_cast<size_t>(state.range(0));
    Value A = makeComplexMatrix(n, 42);
    Value B = makeComplexMatrix(n, 43);
    for (auto _ : state) {
        auto res = linsolve(A, B, nullptr);
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_Linalg_Solve_Complex)->Arg(64)->Arg(128)->Arg(256);

} // namespace
