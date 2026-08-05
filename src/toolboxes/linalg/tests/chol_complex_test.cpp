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

TEST(CholComplexTest, CholNonPositiveDefiniteThrows) {
    // A = [1 2i; -2i 1] (Hermitian but det = -3 < 0, indef)
    Value A = Value::complexMatrix(2, 2);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(1.0, 0.0);
    ad[1] = std::complex<double>(0.0, -2.0);
    ad[2] = std::complex<double>(0.0, 2.0);
    ad[3] = std::complex<double>(1.0, 0.0);

    EXPECT_THROW(chol(A), Error);
}
