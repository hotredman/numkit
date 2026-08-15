// toolboxes/linalg/tests/chol_complex_test.cpp
//
// Unit tests for complex Hermitian Cholesky factorization (chol).

#include <gtest/gtest.h>
#include <numkit/linalg/decompositions.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

#include <complex>

using namespace numkit;
using namespace numkit::linalg;

TEST(CholComplexTest, CholHermitian2x2) {
    // A = [2 1i; -1i 2] (2x2 Hermitian positive definite)
    Value A = Value::complexMatrix(2, 2);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(2.0, 0.0);  // A(1,1)
    ad[1] = std::complex<double>(0.0, -1.0); // A(2,1) = -1i
    ad[2] = std::complex<double>(0.0, 1.0);  // A(1,2) = 1i
    ad[3] = std::complex<double>(2.0, 0.0);  // A(2,2)

    Value R = chol(A);
    EXPECT_EQ(R.dims().rows(), 2);
    EXPECT_EQ(R.dims().cols(), 2);
    ASSERT_TRUE(R.isComplex());

    const auto *rd = R.complexData();
    // R(1,1) = sqrt(2)
    EXPECT_NEAR(rd[0].real(), std::sqrt(2.0), 1e-14);
    EXPECT_NEAR(rd[0].imag(), 0.0, 1e-14);

    // R(2,1) = 0
    EXPECT_NEAR(rd[1].real(), 0.0, 1e-14);
    EXPECT_NEAR(rd[1].imag(), 0.0, 1e-14);

    // R(1,2) = 1i / sqrt(2) = 0.7071067811865475i
    EXPECT_NEAR(rd[2].real(), 0.0, 1e-14);
    EXPECT_NEAR(rd[2].imag(), 1.0 / std::sqrt(2.0), 1e-14);

    // R(2,2) = sqrt(1.5)
    EXPECT_NEAR(rd[3].real(), std::sqrt(1.5), 1e-14);
    EXPECT_NEAR(rd[3].imag(), 0.0, 1e-14);

    // Verify reconstruction R^H * R = A
    std::complex<double> A_rec[4];
    A_rec[0] = std::conj(rd[0]) * rd[0] + std::conj(rd[1]) * rd[1];
    A_rec[1] = std::conj(rd[2]) * rd[0] + std::conj(rd[3]) * rd[1];
    A_rec[2] = std::conj(rd[0]) * rd[2] + std::conj(rd[1]) * rd[3];
    A_rec[3] = std::conj(rd[2]) * rd[2] + std::conj(rd[3]) * rd[3];

    for (size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(A_rec[i].real(), ad[i].real(), 1e-14);
        EXPECT_NEAR(A_rec[i].imag(), ad[i].imag(), 1e-14);
    }
}

TEST(CholComplexTest, CholNonHermitianThrows) {
    // A = [2 1i; 1i 2] (symmetric but NOT Hermitian)
    Value A = Value::complexMatrix(2, 2);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(2.0, 0.0);
    ad[1] = std::complex<double>(0.0, 1.0);  // Non-Hermitian pair (1i != conj(1i))
    ad[2] = std::complex<double>(0.0, 1.0);
    ad[3] = std::complex<double>(2.0, 0.0);

    EXPECT_THROW(chol(A), Error);
}

TEST(CholComplexTest, CholHermitian3x3) {
    // A (3x3 Hermitian positive definite matrix)
    // A = [ 4,        1-1i,     0.5i ]
    //     [ 1+1i,     3,        1-2i ]
    //     [ -0.5i,    1+2i,     5    ]
    Value A = Value::complexMatrix(3, 3);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(4.0, 0.0);   ad[3] = std::complex<double>(1.0, -1.0);  ad[6] = std::complex<double>(0.0, 0.5);
    ad[1] = std::complex<double>(1.0, 1.0);   ad[4] = std::complex<double>(3.0, 0.0);   ad[7] = std::complex<double>(1.0, -2.0);
    ad[2] = std::complex<double>(0.0, -0.5);  ad[5] = std::complex<double>(1.0, 2.0);   ad[8] = std::complex<double>(5.0, 0.0);

    Value R = chol(A);
    EXPECT_EQ(R.dims().rows(), 3);
    EXPECT_EQ(R.dims().cols(), 3);
    ASSERT_TRUE(R.isComplex());

    const auto *rd = R.complexData();
    // Verify R is upper triangular (rd[1] == 0, rd[2] == 0, rd[5] == 0)
    EXPECT_NEAR(rd[1].real(), 0.0, 1e-14);
    EXPECT_NEAR(rd[1].imag(), 0.0, 1e-14);
    EXPECT_NEAR(rd[2].real(), 0.0, 1e-14);
    EXPECT_NEAR(rd[2].imag(), 0.0, 1e-14);
    EXPECT_NEAR(rd[5].real(), 0.0, 1e-14);
    EXPECT_NEAR(rd[5].imag(), 0.0, 1e-14);

    // Verify reconstruction R^H * R = A
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 3; ++k) {
                sum += std::conj(rd[k + i * 3]) * rd[k + j * 3];
            }
            EXPECT_NEAR(sum.real(), ad[i + j * 3].real(), 1e-14);
            EXPECT_NEAR(sum.imag(), ad[i + j * 3].imag(), 1e-14);
        }
    }
}

TEST(CholComplexTest, CholComplexBlocked_N96_N513) {
    std::mt19937 rng(707);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    for (size_t n : {size_t(96), size_t(513)}) {
        Value X = Value::complexMatrix(n, n);
        auto *xd = X.complexDataMut();
        for (size_t i = 0; i < n * n; ++i) {
            xd[i] = std::complex<double>(dist(rng), dist(rng));
        }

        // Form HPD matrix A = X * X^H + n * I
        Value A = Value::complexMatrix(n, n);
        auto *ad = A.complexDataMut();
        for (size_t j = 0; j < n; ++j) {
            for (size_t i = 0; i < n; ++i) {
                std::complex<double> sum(0.0, 0.0);
                for (size_t k = 0; k < n; ++k) {
                    sum += xd[i + k * n] * std::conj(xd[j + k * n]);
                }
                if (i == j) sum += static_cast<double>(n);
                ad[i + j * n] = sum;
            }
        }

        Value R;
        EXPECT_NO_THROW(R = chol(A));
        ASSERT_TRUE(R.isComplex());
        const auto *rd = R.complexData();

        // (b) Verify strictly-lower entries of R are exactly zero
        for (size_t j = 0; j < n; ++j) {
            for (size_t i = j + 1; i < n; ++i) {
                EXPECT_EQ(rd[i + j * n], std::complex<double>(0.0, 0.0)) << "n=" << n << " at (" << i << "," << j << ")";
            }
        }

        // (c) Verify reconstruction R^H * R = A within 1e-9
        double max_err = 0.0;
        for (size_t j = 0; j < n; ++j) {
            for (size_t i = 0; i < n; ++i) {
                std::complex<double> sum(0.0, 0.0);
                size_t k_end = std::min(i, j) + 1;
                for (size_t k = 0; k < k_end; ++k) {
                    sum += std::conj(rd[k + i * n]) * rd[k + j * n];
                }
                max_err = std::max(max_err, std::abs(sum - ad[i + j * n]));
            }
        }
        EXPECT_LT(max_err, 1e-9) << "n=" << n;
    }
}

