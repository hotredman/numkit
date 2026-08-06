// toolboxes/linalg/tests/blocked_qr_test.cpp
//
// Unit tests for blocked QR decomposition (Phase 4.2).

#include <gtest/gtest.h>
#include <numkit/linalg/decompositions.hpp>
#include <numkit/value/value.hpp>

#include <cmath>
#include <complex>
#include <random>

using namespace numkit;
using namespace numkit::linalg;

TEST(BlockedQrTest, BlockedQr256Real) {
    const size_t m = 256;
    const size_t n = 128;
    Value A = Value::matrix(m, n);
    double *ad = A.doubleDataMut();

    std::mt19937 gen(67890);
    std::normal_distribution<double> dist(0.0, 1.0);
    for (size_t i = 0; i < m * n; ++i) ad[i] = dist(gen);

    auto [Q, R] = qr_decompose(A);

    EXPECT_EQ(Q.dims().rows(), m); EXPECT_EQ(Q.dims().cols(), m);
    EXPECT_EQ(R.dims().rows(), m); EXPECT_EQ(R.dims().cols(), n);

    // Verify A == Q * R
    Value QR = Value::matrix(m, n);
    double *qrd = QR.doubleDataMut();
    const double *qd = Q.doubleData();
    const double *rd = R.doubleData();

    for (size_t i = 0; i < m; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (size_t k = 0; k < m; ++k) s += qd[i + k * m] * rd[k + j * m];
            qrd[i + j * m] = s;
        }
    }

    double max_err = 0.0;
    for (size_t i = 0; i < m * n; ++i) {
        max_err = std::max(max_err, std::abs(ad[i] - qrd[i]));
    }
    EXPECT_LT(max_err, 1e-11);

    // Verify Q' * Q == I
    double max_ortho_err = 0.0;
    for (size_t i = 0; i < m; ++i) {
        for (size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (size_t k = 0; k < m; ++k) s += qd[k + i * m] * qd[k + j * m];
            double expected = (i == j) ? 1.0 : 0.0;
            max_ortho_err = std::max(max_ortho_err, std::abs(s - expected));
        }
    }
    EXPECT_LT(max_ortho_err, 1e-11);
}
