// toolboxes/linalg/tests/matrix_functions_general_test.cpp
//
// Unit tests for general sqrtm, sylvester, and logm.

#include <gtest/gtest.h>
#include <numkit/linalg/matrix_functions.hpp>
#include <numkit/value/value.hpp>

#include <complex>

using namespace numkit;
using namespace numkit::linalg;

TEST(MatrixFunctionsGeneralTest, SqrtmGeneralSquare) {
    // A = [1 2; 3 4]
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 1.0; ad[1] = 3.0; ad[2] = 2.0; ad[3] = 4.0;

    Value R = sqrtm(A);
    EXPECT_EQ(R.dims().rows(), 2);
    EXPECT_EQ(R.dims().cols(), 2);

    auto getR = [&](size_t r, size_t c) -> std::complex<double> {
        return R.isComplex() ? R.complexData()[r + c * 2] : std::complex<double>(R.doubleData()[r + c * 2], 0.0);
    };

    // Verify R^2 == A
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += getR(i, k) * getR(k, j);
            }
            EXPECT_NEAR(sum.real(), ad[i + j * 2], 1e-12);
            EXPECT_NEAR(sum.imag(), 0.0, 1e-12);
        }
    }
}

TEST(MatrixFunctionsGeneralTest, SqrtmComplexMatrix) {
    // A = [1+1i 2; 3 4-1i]
    Value A = Value::complexMatrix(2, 2);
    auto *ad = A.complexDataMut();
    ad[0] = std::complex<double>(1.0, 1.0);
    ad[1] = std::complex<double>(3.0, 0.0);
    ad[2] = std::complex<double>(2.0, 0.0);
    ad[3] = std::complex<double>(4.0, -1.0);

    Value R = sqrtm(A);
    EXPECT_EQ(R.dims().rows(), 2);
    EXPECT_EQ(R.dims().cols(), 2);

    auto getR = [&](size_t r, size_t c) -> std::complex<double> {
        return R.isComplex() ? R.complexData()[r + c * 2] : std::complex<double>(R.doubleData()[r + c * 2], 0.0);
    };

    // Verify R^2 == A
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += getR(i, k) * getR(k, j);
            }
            EXPECT_NEAR(sum.real(), ad[i + j * 2].real(), 1e-12);
            EXPECT_NEAR(sum.imag(), ad[i + j * 2].imag(), 1e-12);
        }
    }
}

TEST(MatrixFunctionsGeneralTest, SylvesterGeneral) {
    // A = [1 2; 3 4], B = [5 6; 7 8], C = [1 0; 0 1]
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 1.0; ad[1] = 3.0; ad[2] = 2.0; ad[3] = 4.0;

    Value B = Value::matrix(2, 2);
    auto *bd = B.doubleDataMut();
    bd[0] = 5.0; bd[1] = 7.0; bd[2] = 6.0; bd[3] = 8.0;

    Value C = Value::matrix(2, 2);
    auto *cd = C.doubleDataMut();
    cd[0] = 1.0; cd[1] = 0.0; cd[2] = 0.0; cd[3] = 1.0;

    Value X = sylvester(A, B, C);
    EXPECT_EQ(X.dims().rows(), 2);
    EXPECT_EQ(X.dims().cols(), 2);

    auto getX = [&](size_t r, size_t c) -> std::complex<double> {
        return X.isComplex() ? X.complexData()[r + c * 2] : std::complex<double>(X.doubleData()[r + c * 2], 0.0);
    };

    // Verify A * X + X * B == C
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> ax(0.0, 0.0);
            std::complex<double> xb(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                ax += std::complex<double>(ad[i + k * 2], 0.0) * getX(k, j);
                xb += getX(i, k) * std::complex<double>(bd[k + j * 2], 0.0);
            }
            std::complex<double> sum = ax + xb;
            EXPECT_NEAR(sum.real(), cd[i + j * 2], 1e-12);
            EXPECT_NEAR(sum.imag(), 0.0, 1e-12);
        }
    }
}

TEST(MatrixFunctionsGeneralTest, LogmGeneralSquare) {
    // A = [2 1; 0 3]
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 2.0; ad[1] = 0.0; ad[2] = 1.0; ad[3] = 3.0;

    Value L = logm(A);
    Value E = expm(L);

    const double *ed = E.doubleData();
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(ed[i], ad[i], 1e-8);
    }
}
