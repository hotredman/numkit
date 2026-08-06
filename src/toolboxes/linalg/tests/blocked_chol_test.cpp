// toolboxes/linalg/tests/blocked_chol_test.cpp
//
// Unit tests for blocked Cholesky decomposition (Phase 4.3).

#include <gtest/gtest.h>
#include <numkit/linalg/decompositions.hpp>
#include <numkit/value/value.hpp>

#include <cmath>
#include <random>

using namespace numkit;
using namespace numkit::linalg;

TEST(BlockedCholTest, BlockedChol256Real) {
    const size_t n = 256;
    Value B = Value::matrix(n, n);
    double *bd = B.doubleDataMut();

    std::mt19937 gen(43210);
    std::normal_distribution<double> dist(0.0, 1.0);
    for (size_t i = 0; i < n * n; ++i) bd[i] = dist(gen);

    // Form symmetric positive-definite A = B * B' + n*I
    Value A = Value::matrix(n, n);
    double *ad = A.doubleDataMut();
    std::fill(ad, ad + n * n, 0.0);

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (size_t k = 0; k < n; ++k) s += bd[i + k * n] * bd[j + k * n];
            ad[i + j * n] = s + (i == j ? static_cast<double>(n) : 0.0);
        }
    }

    Value R = chol(A);

    EXPECT_EQ(R.dims().rows(), n);
    EXPECT_EQ(R.dims().cols(), n);

    // Verify A == R' * R
    Value RtR = Value::matrix(n, n);
    double *rtrd = RtR.doubleDataMut();
    const double *rd = R.doubleData();

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (size_t k = 0; k < n; ++k) s += rd[k + i * n] * rd[k + j * n];
            rtrd[i + j * n] = s;
        }
    }

    double max_err = 0.0;
    for (size_t i = 0; i < n * n; ++i) {
        max_err = std::max(max_err, std::abs(ad[i] - rtrd[i]));
    }
    EXPECT_LT(max_err, 1e-11);
}

TEST(BlockedCholTest, StrictLowerZerosAndReconstruction_N64_N513) {
    for (size_t n : {size_t(64), size_t(513)}) {
        Value B = Value::matrix(n, n);
        double *bd = B.doubleDataMut();

        std::mt19937 gen(1337 + static_cast<uint32_t>(n));
        std::normal_distribution<double> dist(0.0, 1.0);
        for (size_t i = 0; i < n * n; ++i) bd[i] = dist(gen);

        Value A = Value::matrix(n, n);
        double *ad = A.doubleDataMut();
        std::fill(ad, ad + n * n, 0.0);

        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                double s = 0.0;
                for (size_t k = 0; k < n; ++k) s += bd[i + k * n] * bd[j + k * n];
                ad[i + j * n] = s + (i == j ? static_cast<double>(n) : 0.0);
            }
        }

        Value R = chol(A);
        const double *rd = R.doubleData();

        // Check (a): strictly-lower entries must be exactly 0.0
        size_t non_zero_lower_count = 0;
        for (size_t j = 0; j < n; ++j) {
            for (size_t i = j + 1; i < n; ++i) {
                if (rd[i + j * n] != 0.0) {
                    non_zero_lower_count++;
                }
            }
        }
        EXPECT_EQ(non_zero_lower_count, 0u) << "n=" << n << ": found " << non_zero_lower_count << " nonzero entries in strictly lower triangle of R";

        // Check (b): max |A - R' * R| < 1e-10
        double max_err = 0.0;
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                double s = 0.0;
                for (size_t k = 0; k <= std::min(i, j); ++k) {
                    s += rd[k + i * n] * rd[k + j * n];
                }
                max_err = std::max(max_err, std::abs(ad[i + j * n] - s));
            }
        }
        EXPECT_LT(max_err, 1e-10) << "n=" << n;
    }
}

