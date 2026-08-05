// toolboxes/linalg/tests/lu_complex_test.cpp
//
// Unit tests for complex LU decomposition, determinant, inverse, and mldivide.

#include <gtest/gtest.h>
#include <numkit/linalg/decompositions.hpp>
#include <numkit/linalg/properties.hpp>
#include <numkit/linalg/solvers.hpp>
#include <numkit/value/value.hpp>

#include <complex>

using namespace numkit;
using namespace numkit::linalg;

TEST(ComplexLinalgTest, DetComplex2x2) {
    // B = [1+1i, 2; 3, 4-1i]
    // det(B) = (1+1i)*(4-1i) - 6 = (4 - 1i + 4i + 1) - 6 = 5 + 3i - 6 = -1 + 3i
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);  // B(1,1)
    bd[1] = std::complex<double>(3.0, 0.0);  // B(2,1)
    bd[2] = std::complex<double>(2.0, 0.0);  // B(1,2)
    bd[3] = std::complex<double>(4.0, -1.0); // B(2,2)

    Value d = det(B);
    ASSERT_TRUE(d.isComplex());
    auto c = d.complexData()[0];
    EXPECT_NEAR(c.real(), -1.0, 1e-14);
    EXPECT_NEAR(c.imag(), 3.0, 1e-14);
}

TEST(ComplexLinalgTest, LuDecomposeComplex) {
    // B = [1+1i, 2; 3, 4-1i]
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);
    bd[1] = std::complex<double>(3.0, 0.0);
    bd[2] = std::complex<double>(2.0, 0.0);
    bd[3] = std::complex<double>(4.0, -1.0);

    auto [L, U, P] = lu_decompose(B);
    EXPECT_EQ(L.dims().rows(), 2);
    EXPECT_EQ(U.dims().rows(), 2);
    EXPECT_EQ(P.dims().rows(), 2);

    // Verify reconstruction P * B = L * U
    // P * B:
    Value PB = Value::complexMatrix(2, 2);
    auto *pbd = PB.complexDataMut();
    const double *pd = P.doubleData();
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += pd[i + k * 2] * bd[k + j * 2];
            }
            pbd[i + j * 2] = sum;
        }
    }

    // L * U:
    Value LU = Value::complexMatrix(2, 2);
    auto *lud = LU.complexDataMut();
    const std::complex<double> *ld = L.isComplex() ? L.complexData() : nullptr;
    const std::complex<double> *ud = U.isComplex() ? U.complexData() : nullptr;

    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                std::complex<double> lv = L.isComplex() ? ld[i + k * 2] : L.doubleData()[i + k * 2];
                std::complex<double> uv = U.isComplex() ? ud[k + j * 2] : U.doubleData()[k + j * 2];
                sum += lv * uv;
            }
            lud[i + j * 2] = sum;
        }
    }

    for (size_t i = 0; i < 4; ++i) {
        EXPECT_NEAR(pbd[i].real(), lud[i].real(), 1e-14);
        EXPECT_NEAR(pbd[i].imag(), lud[i].imag(), 1e-14);
    }
}

TEST(ComplexLinalgTest, InvComplex) {
    // B = [1+1i, 2; 3, 4-1i]
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);
    bd[1] = std::complex<double>(3.0, 0.0);
    bd[2] = std::complex<double>(2.0, 0.0);
    bd[3] = std::complex<double>(4.0, -1.0);

    Value Binv = inv(B);
    ASSERT_TRUE(Binv.isComplex());
    const auto *binvd = Binv.complexData();

    // Verify B * Binv = I_2
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> sum(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                sum += bd[i + k * 2] * binvd[k + j * 2];
            }
            double expected_re = (i == j) ? 1.0 : 0.0;
            EXPECT_NEAR(sum.real(), expected_re, 1e-14);
            EXPECT_NEAR(sum.imag(), 0.0, 1e-14);
        }
    }
}

TEST(ComplexLinalgTest, MldivideComplexSquare) {
    // B = [1+1i, 2; 3, 4-1i]
    // b = [1; 2+1i]
    Value B = Value::complexMatrix(2, 2);
    auto *bd = B.complexDataMut();
    bd[0] = std::complex<double>(1.0, 1.0);
    bd[1] = std::complex<double>(3.0, 0.0);
    bd[2] = std::complex<double>(2.0, 0.0);
    bd[3] = std::complex<double>(4.0, -1.0);

    Value b = Value::complexMatrix(2, 1);
    auto *vd = b.complexDataMut();
    vd[0] = std::complex<double>(1.0, 0.0);
    vd[1] = std::complex<double>(2.0, 1.0);

    Value x = linsolve(B, b);
    ASSERT_TRUE(x.isComplex());
    const auto *xd = x.complexData();

    // Verify B * x = b
    for (size_t i = 0; i < 2; ++i) {
        std::complex<double> sum(0.0, 0.0);
        for (size_t k = 0; k < 2; ++k) {
            sum += bd[i + k * 2] * xd[k];
        }
        EXPECT_NEAR(sum.real(), vd[i].real(), 1e-14);
        EXPECT_NEAR(sum.imag(), vd[i].imag(), 1e-14);
    }
}
