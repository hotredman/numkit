// toolboxes/linalg/tests/schur_complex_test.cpp
//
// Unit tests for complex Schur decomposition and general complex eigenvalue/eigenvector (schur, eig).

#include <gtest/gtest.h>
#include <numkit/linalg/eig.hpp>
#include <numkit/value/value.hpp>

#include <complex>

using namespace numkit;
using namespace numkit::linalg;

TEST(ComplexSchurTest, SchurComplex2x2) {
    // B = [1+1i 2; 3 4-1i]
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);  // B(1,1)
    bd[1] = std::complex<double>(3.0, 0.0);  // B(2,1)
    bd[2] = std::complex<double>(2.0, 0.0);  // B(1,2)
    bd[3] = std::complex<double>(4.0, -1.0); // B(2,2)

    auto [U, T] = schur_general(B);
    EXPECT_EQ(U.dims().rows(), 2);
    EXPECT_EQ(U.dims().cols(), 2);
    EXPECT_EQ(T.dims().rows(), 2);
    EXPECT_EQ(T.dims().cols(), 2);

    auto getU = [&](size_t r, size_t c) -> std::complex<double> {
        return U.isComplex() ? U.complexData()[r + c * 2] : std::complex<double>(U.doubleData()[r + c * 2], 0.0);
    };
    auto getT = [&](size_t r, size_t c) -> std::complex<double> {
        return T.isComplex() ? T.complexData()[r + c * 2] : std::complex<double>(T.doubleData()[r + c * 2], 0.0);
    };

    // Verify upper triangularity of T (T(2,1) == 0)
    EXPECT_NEAR(getT(1, 0).real(), 0.0, 1e-13);
    EXPECT_NEAR(getT(1, 0).imag(), 0.0, 1e-13);

    // Verify U * T * U^H = B
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                for (size_t l = 0; l < 2; ++l) {
                    sum += getU(i, k) * getT(k, l) * std::conj(getU(j, l));
                }
            }
            EXPECT_NEAR(sum.real(), bd[i + j * 2].real(), 1e-13);
            EXPECT_NEAR(sum.imag(), bd[i + j * 2].imag(), 1e-13);
        }
    }

    // Verify unitarity U^H * U = I_2
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += std::conj(getU(k, i)) * getU(k, j);
            }
            double expected_re = (i == j) ? 1.0 : 0.0;
            EXPECT_NEAR(sum.real(), expected_re, 1e-14);
            EXPECT_NEAR(sum.imag(), 0.0, 1e-14);
        }
    }
}

TEST(ComplexEigTest, EigValuesComplex2x2) {
    // B = [1+1i 2; 3 4-1i]
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);
    bd[1] = std::complex<double>(3.0, 0.0);
    bd[2] = std::complex<double>(2.0, 0.0);
    bd[3] = std::complex<double>(4.0, -1.0);

    Value ev = eig_general_values(B);
    EXPECT_EQ(ev.numel(), 2);

    // Sum of eigenvalues == trace(B) = 5
    // Prod of eigenvalues == det(B) = -1 + 3i
    auto getE = [&](size_t i) -> std::complex<double> {
        return ev.isComplex() ? ev.complexData()[i] : std::complex<double>(ev.doubleData()[i], 0.0);
    };

    std::complex<double> sum_ev = getE(0) + getE(1);
    std::complex<double> prod_ev = getE(0) * getE(1);

    EXPECT_NEAR(sum_ev.real(), 5.0, 1e-13);
    EXPECT_NEAR(sum_ev.imag(), 0.0, 1e-13);

    EXPECT_NEAR(prod_ev.real(), -1.0, 1e-13);
    EXPECT_NEAR(prod_ev.imag(), 3.0, 1e-13);
}

TEST(ComplexEigTest, EigVDComplex2x2) {
    // B = [1+1i 2; 3 4-1i]
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);
    bd[1] = std::complex<double>(3.0, 0.0);
    bd[2] = std::complex<double>(2.0, 0.0);
    bd[3] = std::complex<double>(4.0, -1.0);

    auto [V, D] = eig_general_VD(B);
    EXPECT_EQ(V.dims().rows(), 2);
    EXPECT_EQ(V.dims().cols(), 2);
    EXPECT_EQ(D.dims().rows(), 2);
    EXPECT_EQ(D.dims().cols(), 2);

    auto getV = [&](size_t r, size_t c) -> std::complex<double> {
        return V.isComplex() ? V.complexData()[r + c * 2] : std::complex<double>(V.doubleData()[r + c * 2], 0.0);
    };
    auto getD = [&](size_t r, size_t c) -> std::complex<double> {
        return D.isComplex() ? D.complexData()[r + c * 2] : std::complex<double>(D.doubleData()[r + c * 2], 0.0);
    };

    // Verify B * V = V * D
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> lhs(0.0, 0.0);
            std::complex<double> rhs(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                lhs += bd[i + k * 2] * getV(k, j);
                rhs += getV(i, k) * getD(k, j);
            }
            EXPECT_NEAR(lhs.real(), rhs.real(), 1e-13);
            EXPECT_NEAR(lhs.imag(), rhs.imag(), 1e-13);
        }
    }
}

TEST(ComplexSchurTest, SchurComplex3x3) {
    // 3x3 complex matrix
    Value A = Value::complexMatrix(3, 3);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(2.0, 1.0);  ad[3] = std::complex<double>(1.0, -1.0); ad[6] = std::complex<double>(0.0, 2.0);
    ad[1] = std::complex<double>(0.5, 0.0);  ad[4] = std::complex<double>(3.0, 0.0);  ad[7] = std::complex<double>(1.0, 1.0);
    ad[2] = std::complex<double>(-1.0, 2.0); ad[5] = std::complex<double>(2.0, 0.0);  ad[8] = std::complex<double>(4.0, -2.0);

    auto [U, T] = schur_general(A);
    EXPECT_EQ(U.dims().rows(), 3);
    EXPECT_EQ(T.dims().rows(), 3);

    auto getU = [&](size_t r, size_t c) -> std::complex<double> {
        return U.isComplex() ? U.complexData()[r + c * 3] : std::complex<double>(U.doubleData()[r + c * 3], 0.0);
    };
    auto getT = [&](size_t r, size_t c) -> std::complex<double> {
        return T.isComplex() ? T.complexData()[r + c * 3] : std::complex<double>(T.doubleData()[r + c * 3], 0.0);
    };

    // Verify upper triangularity of T (below diagonal == 0)
    for (size_t i = 1; i < 3; ++i) {
        for (size_t j = 0; j < i; ++j) {
            EXPECT_NEAR(getT(i, j).real(), 0.0, 1e-12);
            EXPECT_NEAR(getT(i, j).imag(), 0.0, 1e-12);
        }
    }

    // Verify U * T * U^H = A
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 3; ++k) {
                for (size_t l = 0; l < 3; ++l) {
                    sum += getU(i, k) * getT(k, l) * std::conj(getU(j, l));
                }
            }
            EXPECT_NEAR(sum.real(), ad[i + j * 3].real(), 1e-12);
            EXPECT_NEAR(sum.imag(), ad[i + j * 3].imag(), 1e-12);
        }
    }

    // Verify unitarity U^H * U = I_3
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 3; ++k) {
                sum += std::conj(getU(k, i)) * getU(k, j);
            }
            double expected_re = (i == j) ? 1.0 : 0.0;
            EXPECT_NEAR(sum.real(), expected_re, 1e-12);
            EXPECT_NEAR(sum.imag(), 0.0, 1e-12);
        }
    }
}

TEST(ComplexEigTest, EigVDComplex3x3) {
    // 3x3 complex matrix
    Value A = Value::complexMatrix(3, 3);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(2.0, 1.0);  ad[3] = std::complex<double>(1.0, -1.0); ad[6] = std::complex<double>(0.0, 2.0);
    ad[1] = std::complex<double>(0.5, 0.0);  ad[4] = std::complex<double>(3.0, 0.0);  ad[7] = std::complex<double>(1.0, 1.0);
    ad[2] = std::complex<double>(-1.0, 2.0); ad[5] = std::complex<double>(2.0, 0.0);  ad[8] = std::complex<double>(4.0, -2.0);

    auto [V, D] = eig_general_VD(A);
    EXPECT_EQ(V.dims().rows(), 3);
    EXPECT_EQ(D.dims().rows(), 3);

    auto getV = [&](size_t r, size_t c) -> std::complex<double> {
        return V.isComplex() ? V.complexData()[r + c * 3] : std::complex<double>(V.doubleData()[r + c * 3], 0.0);
    };
    auto getD = [&](size_t r, size_t c) -> std::complex<double> {
        return D.isComplex() ? D.complexData()[r + c * 3] : std::complex<double>(D.doubleData()[r + c * 3], 0.0);
    };

    // Verify A * V = V * D
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            std::complex<double> lhs(0.0, 0.0);
            std::complex<double> rhs(0.0, 0.0);
            for (size_t k = 0; k < 3; ++k) {
                lhs += ad[i + k * 3] * getV(k, j);
                rhs += getV(i, k) * getD(k, j);
            }
            EXPECT_NEAR(lhs.real(), rhs.real(), 1e-12);
            EXPECT_NEAR(lhs.imag(), rhs.imag(), 1e-12);
        }
    }
}

TEST(ComplexSchurTest, HermitianComplex2x2) {
    // H = [2, 1i; -1i, 3] -- Hermitian
    Value H = Value::complexMatrix(2, 2);
    auto *hd = H.complexDataMut();
    hd[0] = std::complex<double>(2.0, 0.0);
    hd[1] = std::complex<double>(0.0, -1.0);
    hd[2] = std::complex<double>(0.0, 1.0);
    hd[3] = std::complex<double>(3.0, 0.0);

    auto [U, T] = schur_general(H);
    auto getT = [&](size_t r, size_t c) -> std::complex<double> {
        return T.isComplex() ? T.complexData()[r + c * 2] : std::complex<double>(T.doubleData()[r + c * 2], 0.0);
    };

    // Hermitian eigenvalues must be real
    EXPECT_NEAR(getT(0, 0).imag(), 0.0, 1e-13);
    EXPECT_NEAR(getT(1, 1).imag(), 0.0, 1e-13);
}
