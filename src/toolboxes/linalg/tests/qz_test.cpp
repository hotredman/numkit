// toolboxes/linalg/tests/qz_test.cpp
//
// Unit tests for qz (generalized Schur decomposition).

#include <gtest/gtest.h>
#include <numkit/linalg/qz.hpp>
#include <numkit/value/value.hpp>

#include <complex>

using namespace numkit;
using namespace numkit::linalg;

TEST(QzTest, QzDecomposition2x2) {
    // A = [1 2; 3 4], B = [5 6; 7 8]
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 1.0; ad[1] = 3.0; ad[2] = 2.0; ad[3] = 4.0;

    Value B = Value::matrix(2, 2);
    auto *bd = B.doubleDataMut();
    bd[0] = 5.0; bd[1] = 7.0; bd[2] = 6.0; bd[3] = 8.0;

    auto [AA, BB, Q, Z] = qz(A, B);

    EXPECT_EQ(AA.dims().rows(), 2); EXPECT_EQ(AA.dims().cols(), 2);
    EXPECT_EQ(BB.dims().rows(), 2); EXPECT_EQ(BB.dims().cols(), 2);
    EXPECT_EQ(Q.dims().rows(), 2);  EXPECT_EQ(Q.dims().cols(), 2);
    EXPECT_EQ(Z.dims().rows(), 2);  EXPECT_EQ(Z.dims().cols(), 2);

    auto getVal = [](const Value &M, size_t r, size_t c) -> std::complex<double> {
        return M.isComplex() ? M.complexData()[r + c * 2] : std::complex<double>(M.doubleData()[r + c * 2], 0.0);
    };

    // Subdiagonals of AA and BB should be 0
    EXPECT_NEAR(getVal(AA, 1, 0).real(), 0.0, 1e-12);
    EXPECT_NEAR(getVal(AA, 1, 0).imag(), 0.0, 1e-12);
    EXPECT_NEAR(getVal(BB, 1, 0).real(), 0.0, 1e-12);
    EXPECT_NEAR(getVal(BB, 1, 0).imag(), 0.0, 1e-12);

    // Verify Q * A * Z == AA
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> qaz(0.0, 0.0);
            std::complex<double> qbz(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                for (size_t l = 0; l < 2; ++l) {
                    qaz += getVal(Q, i, k) * std::complex<double>(ad[k + l * 2], 0.0) * getVal(Z, l, j);
                    qbz += getVal(Q, i, k) * std::complex<double>(bd[k + l * 2], 0.0) * getVal(Z, l, j);
                }
            }
            EXPECT_NEAR(qaz.real(), getVal(AA, i, j).real(), 1e-12);
            EXPECT_NEAR(qaz.imag(), getVal(AA, i, j).imag(), 1e-12);
            EXPECT_NEAR(qbz.real(), getVal(BB, i, j).real(), 1e-12);
            EXPECT_NEAR(qbz.imag(), getVal(BB, i, j).imag(), 1e-12);
        }
    }
}

TEST(QzTest, QzDecompositionComplex) {
    // A = [1+1i 2; 3 4], B = [1 0; 0 1]
    Value A = Value::complexMatrix(2, 2);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(1.0, 1.0); ad[1] = std::complex<double>(3.0, 0.0);
    ad[2] = std::complex<double>(2.0, 0.0); ad[3] = std::complex<double>(4.0, 0.0);

    Value B = Value::matrix(2, 2);
    auto *bd = B.doubleDataMut();
    bd[0] = 1.0; bd[1] = 0.0; bd[2] = 0.0; bd[3] = 1.0;

    auto [AA, BB, Q, Z] = qz(A, B);

    auto getVal = [](const Value &M, size_t r, size_t c) -> std::complex<double> {
        return M.isComplex() ? M.complexData()[r + c * 2] : std::complex<double>(M.doubleData()[r + c * 2], 0.0);
    };

    // Subdiagonal of AA should be 0
    EXPECT_NEAR(getVal(AA, 1, 0).real(), 0.0, 1e-12);
    EXPECT_NEAR(getVal(AA, 1, 0).imag(), 0.0, 1e-12);
}

TEST(QzTest, QzNearSingularB) {
    // A = [1 2; 3 4], B = [1e-12 0; 0 1]
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 1.0; ad[1] = 3.0; ad[2] = 2.0; ad[3] = 4.0;

    Value B = Value::matrix(2, 2);
    auto *bd = B.doubleDataMut();
    bd[0] = 1e-12; bd[1] = 0.0; bd[2] = 0.0; bd[3] = 1.0;

    auto [AA, BB, Q, Z] = qz(A, B);

    EXPECT_EQ(AA.dims().rows(), 2);
    EXPECT_EQ(BB.dims().rows(), 2);
}
