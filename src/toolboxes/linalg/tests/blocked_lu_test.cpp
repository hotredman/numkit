// toolboxes/linalg/tests/blocked_lu_test.cpp
//
// Unit tests for blocked LU decomposition (Phase 4.1).

#include <gtest/gtest.h>
#include <numkit/linalg/decompositions.hpp>
#include <numkit/value/value.hpp>

#include <cmath>
#include <complex>
#include <random>

using namespace numkit;
using namespace numkit::linalg;

TEST(BlockedLuTest, BlockedLu256Real) {
    const size_t n = 256;
    Value A = Value::matrix(n, n);
    double *ad = A.doubleDataMut();

    std::mt19937 gen(12345);
    std::normal_distribution<double> dist(0.0, 1.0);
    for (size_t i = 0; i < n * n; ++i) ad[i] = dist(gen);

    auto [L, U, P] = lu_decompose(A);

    EXPECT_EQ(L.dims().rows(), n); EXPECT_EQ(L.dims().cols(), n);
    EXPECT_EQ(U.dims().rows(), n); EXPECT_EQ(U.dims().cols(), n);
    EXPECT_EQ(P.dims().rows(), n); EXPECT_EQ(P.dims().cols(), n);

    // Verify P * A == L * U
    // First PA = P * A
    Value PA = Value::matrix(n, n);
    double *pad = PA.doubleDataMut();
    const double *pd = P.doubleData();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (size_t k = 0; k < n; ++k) s += pd[i + k * n] * ad[k + j * n];
            pad[i + j * n] = s;
        }
    }

    // LU_prod = L * U
    Value LU_prod = Value::matrix(n, n);
    double *lud = LU_prod.doubleDataMut();
    const double *ld = L.doubleData();
    const double *ud = U.doubleData();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (size_t k = 0; k < n; ++k) s += ld[i + k * n] * ud[k + j * n];
            lud[i + j * n] = s;
        }
    }

    double max_err = 0.0;
    for (size_t i = 0; i < n * n; ++i) {
        max_err = std::max(max_err, std::abs(pad[i] - lud[i]));
    }
    EXPECT_LT(max_err, 1e-11);
}

TEST(BlockedLuTest, BlockedLu128Complex) {
    const size_t n = 128;
    Value A = Value::complexMatrix(n, n);
    std::complex<double> *ad = A.complexDataMut();

    std::mt19937 gen(54321);
    std::normal_distribution<double> dist(0.0, 1.0);
    for (size_t i = 0; i < n * n; ++i) ad[i] = std::complex<double>(dist(gen), dist(gen));

    auto [L, U, P] = lu_decompose(A);

    // Verify P * A == L * U
    Value PA = Value::complexMatrix(n, n);
    std::complex<double> *pad = PA.complexDataMut();
    const double *pd = P.doubleData();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            std::complex<double> s(0.0, 0.0);
            for (size_t k = 0; k < n; ++k) s += pd[i + k * n] * ad[k + j * n];
            pad[i + j * n] = s;
        }
    }

    Value LU_prod = Value::complexMatrix(n, n);
    std::complex<double> *lud = LU_prod.complexDataMut();
    const std::complex<double> *ld = L.complexData();
    const std::complex<double> *ud = U.complexData();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            std::complex<double> s(0.0, 0.0);
            for (size_t k = 0; k < n; ++k) s += ld[i + k * n] * ud[k + j * n];
            lud[i + j * n] = s;
        }
    }

    double max_err = 0.0;
    for (size_t i = 0; i < n * n; ++i) {
        max_err = std::max(max_err, std::abs(pad[i] - lud[i]));
    }
    EXPECT_LT(max_err, 1e-11);
}
