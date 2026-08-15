// toolboxes/linalg/tests/svd_complex_test.cpp
//
// Unit tests for complex SVD (svd), complex rank (rank), and complex pseudo-inverse (pinv).

#include <gtest/gtest.h>
#include <numkit/linalg/decompositions.hpp>
#include <numkit/linalg/properties.hpp>
#include <numkit/linalg/pseudo_subspace.hpp>
#include <numkit/value/value.hpp>

#include <complex>

using namespace numkit;
using namespace numkit::linalg;

TEST(ComplexSvdTest, SvdComplex2x2) {
    // B = [1+1i 2; 3 4-1i]
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);  // B(1,1)
    bd[1] = std::complex<double>(3.0, 0.0);  // B(2,1)
    bd[2] = std::complex<double>(2.0, 0.0);  // B(1,2)
    bd[3] = std::complex<double>(4.0, -1.0); // B(2,2)

    auto [U, S, V] = svd_decompose(B);
    EXPECT_EQ(U.dims().rows(), 2);
    EXPECT_EQ(U.dims().cols(), 2);
    EXPECT_EQ(S.dims().rows(), 2);
    EXPECT_EQ(S.dims().cols(), 2);
    EXPECT_EQ(V.dims().rows(), 2);
    EXPECT_EQ(V.dims().cols(), 2);

    auto getU = [&](size_t r, size_t c) -> std::complex<double> {
        return U.isComplex() ? U.complexData()[r + c * 2] : std::complex<double>(U.doubleData()[r + c * 2], 0.0);
    };
    auto getS = [&](size_t r, size_t c) -> std::complex<double> {
        return S.isComplex() ? S.complexData()[r + c * 2] : std::complex<double>(S.doubleData()[r + c * 2], 0.0);
    };
    auto getV = [&](size_t r, size_t c) -> std::complex<double> {
        return V.isComplex() ? V.complexData()[r + c * 2] : std::complex<double>(V.doubleData()[r + c * 2], 0.0);
    };

    // Verify reconstruction U * S * V^H = B
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += getU(i, k) * getS(k, k) * std::conj(getV(j, k));
            }
            EXPECT_NEAR(sum.real(), bd[i + j * 2].real(), 1e-13);
            EXPECT_NEAR(sum.imag(), bd[i + j * 2].imag(), 1e-13);
        }
    }

    // Verify U^H * U = I_2
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += std::conj(getU(k, i)) * getU(k, j);
            }
            double expected_re = (i == j) ? 1.0 : 0.0;
            EXPECT_NEAR(sum.real(), expected_re, 1e-13);
            EXPECT_NEAR(sum.imag(), 0.0, 1e-13);
        }
    }

    // Verify V^H * V = I_2
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += std::conj(getV(k, i)) * getV(k, j);
            }
            double expected_re = (i == j) ? 1.0 : 0.0;
            EXPECT_NEAR(sum.real(), expected_re, 1e-13);
            EXPECT_NEAR(sum.imag(), 0.0, 1e-13);
        }
    }
}

TEST(ComplexSvdTest, SvdComplex3x2) {
    // B = [1+1i 2; 3 4-1i; 0 1+2i] (3x2)
    Value B = Value::complexMatrix(3, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);  // B(1,1)
    bd[1] = std::complex<double>(3.0, 0.0);  // B(2,1)
    bd[2] = std::complex<double>(0.0, 0.0);  // B(3,1)
    bd[3] = std::complex<double>(2.0, 0.0);  // B(1,2)
    bd[4] = std::complex<double>(4.0, -1.0); // B(2,2)
    bd[5] = std::complex<double>(1.0, 2.0);  // B(3,2)

    auto [U, S, V] = svd_decompose(B);
    EXPECT_EQ(U.dims().rows(), 3);
    EXPECT_EQ(U.dims().cols(), 3);
    EXPECT_EQ(S.dims().rows(), 3);
    EXPECT_EQ(S.dims().cols(), 2);
    EXPECT_EQ(V.dims().rows(), 2);
    EXPECT_EQ(V.dims().cols(), 2);

    auto getU = [&](size_t r, size_t c) -> std::complex<double> {
        return U.isComplex() ? U.complexData()[r + c * 3] : std::complex<double>(U.doubleData()[r + c * 3], 0.0);
    };
    auto getS = [&](size_t r, size_t c) -> std::complex<double> {
        return S.isComplex() ? S.complexData()[r + c * 3] : std::complex<double>(S.doubleData()[r + c * 3], 0.0);
    };
    auto getV = [&](size_t r, size_t c) -> std::complex<double> {
        return V.isComplex() ? V.complexData()[r + c * 2] : std::complex<double>(V.doubleData()[r + c * 2], 0.0);
    };

    // Verify U * S * V^H = B
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += getU(i, k) * getS(k, k) * std::conj(getV(j, k));
            }
            EXPECT_NEAR(sum.real(), bd[i + j * 3].real(), 1e-13);
            EXPECT_NEAR(sum.imag(), bd[i + j * 3].imag(), 1e-13);
        }
    }
}

TEST(ComplexSvdTest, RankComplex2x2) {
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);
    bd[1] = std::complex<double>(3.0, 0.0);
    bd[2] = std::complex<double>(2.0, 0.0);
    bd[3] = std::complex<double>(4.0, -1.0);

    Value r = rank_of(B);
    EXPECT_EQ(r.elemAsDouble(0), 2.0);
}

TEST(ComplexPinvTest, PinvComplex2x2) {
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);
    bd[1] = std::complex<double>(3.0, 0.0);
    bd[2] = std::complex<double>(2.0, 0.0);
    bd[3] = std::complex<double>(4.0, -1.0);

    Value P = pinv(B);
    EXPECT_EQ(P.dims().rows(), 2);
    EXPECT_EQ(P.dims().cols(), 2);

    auto getP = [&](size_t r, size_t c) -> std::complex<double> {
        return P.isComplex() ? P.complexData()[r + c * 2] : std::complex<double>(P.doubleData()[r + c * 2], 0.0);
    };

    // Verify B * P * B = B
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                for (size_t l = 0; l < 2; ++l) {
                    sum += bd[i + k * 2] * getP(k, l) * bd[l + j * 2];
                }
            }
            EXPECT_NEAR(sum.real(), bd[i + j * 2].real(), 1e-13);
            EXPECT_NEAR(sum.imag(), bd[i + j * 2].imag(), 1e-13);
        }
    }
}

TEST(ComplexSvdTest, SvdComplex2x3) {
    // B = [1+1i 3 0; 2 4-1i 1+2i] (2x3 wide matrix)
    Value B = Value::complexMatrix(2, 3);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);  // B(1,1)
    bd[1] = std::complex<double>(2.0, 0.0);  // B(2,1)
    bd[2] = std::complex<double>(3.0, 0.0);  // B(1,2)
    bd[3] = std::complex<double>(4.0, -1.0); // B(2,2)
    bd[4] = std::complex<double>(0.0, 0.0);  // B(1,3)
    bd[5] = std::complex<double>(1.0, 2.0);  // B(2,3)

    auto [U, S, V] = svd_decompose(B);
    EXPECT_EQ(U.dims().rows(), 2);
    EXPECT_EQ(U.dims().cols(), 2);
    EXPECT_EQ(S.dims().rows(), 2);
    EXPECT_EQ(S.dims().cols(), 3);
    EXPECT_EQ(V.dims().rows(), 3);
    EXPECT_EQ(V.dims().cols(), 3);

    auto getU = [&](size_t r, size_t c) -> std::complex<double> {
        return U.isComplex() ? U.complexData()[r + c * 2] : std::complex<double>(U.doubleData()[r + c * 2], 0.0);
    };
    auto getS = [&](size_t r, size_t c) -> std::complex<double> {
        return S.isComplex() ? S.complexData()[r + c * 2] : std::complex<double>(S.doubleData()[r + c * 2], 0.0);
    };
    auto getV = [&](size_t r, size_t c) -> std::complex<double> {
        return V.isComplex() ? V.complexData()[r + c * 3] : std::complex<double>(V.doubleData()[r + c * 3], 0.0);
    };

    // Verify U * S * V^H = B
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += getU(i, k) * getS(k, k) * std::conj(getV(j, k));
            }
            EXPECT_NEAR(sum.real(), bd[i + j * 2].real(), 1e-13);
            EXPECT_NEAR(sum.imag(), bd[i + j * 2].imag(), 1e-13);
        }
    }
}

TEST(ComplexPinvTest, PinvComplex3x2) {
    // B = [1+1i 2; 3 4-1i; 0 1+2i] (3x2)
    Value B = Value::complexMatrix(3, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);  // B(1,1)
    bd[1] = std::complex<double>(3.0, 0.0);  // B(2,1)
    bd[2] = std::complex<double>(0.0, 0.0);  // B(3,1)
    bd[3] = std::complex<double>(2.0, 0.0);  // B(1,2)
    bd[4] = std::complex<double>(4.0, -1.0); // B(2,2)
    bd[5] = std::complex<double>(1.0, 2.0);  // B(3,2)

    Value P = pinv(B);
    EXPECT_EQ(P.dims().rows(), 2);
    EXPECT_EQ(P.dims().cols(), 3);

    auto getP = [&](size_t r, size_t c) -> std::complex<double> {
        return P.isComplex() ? P.complexData()[r + c * 2] : std::complex<double>(P.doubleData()[r + c * 2], 0.0);
    };

    // Verify B * P * B = B
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                for (size_t l = 0; l < 3; ++l) {
                    sum += bd[i + k * 3] * getP(k, l) * bd[l + j * 3];
                }
            }
            EXPECT_NEAR(sum.real(), bd[i + j * 3].real(), 1e-13);
            EXPECT_NEAR(sum.imag(), bd[i + j * 3].imag(), 1e-13);
        }
    }
}
