// toolboxes/linalg/tests/ordschur_test.cpp
//
// Unit tests for ordschur.

#include <gtest/gtest.h>
#include <numkit/linalg/ordschur.hpp>
#include <numkit/value/value.hpp>

#include <complex>

using namespace numkit;
using namespace numkit::linalg;

TEST(OrdschurTest, ReorderSchur2x2Logical) {
    // T = [1 2; 0 3], U = [1 0; 0 1]
    Value U = Value::matrix(2, 2);
    auto *ud = U.doubleDataMut();
    ud[0] = 1.0; ud[1] = 0.0; ud[2] = 0.0; ud[3] = 1.0;

    Value T = Value::matrix(2, 2);
    auto *td = T.doubleDataMut();
    td[0] = 1.0; td[1] = 0.0; td[2] = 2.0; td[3] = 3.0;

    // Select second eigenvalue (3.0) to move to top-left
    Value select = Value::matrix(1, 2, ValueType::LOGICAL);
    auto *sd = select.logicalDataMut();
    sd[0] = 0; sd[1] = 1;

    auto [Us, Ts] = ordschur(U, T, select);

    EXPECT_EQ(Ts.dims().rows(), 2);
    EXPECT_EQ(Ts.dims().cols(), 2);

    auto getTs = [&](size_t r, size_t c) -> std::complex<double> {
        return Ts.isComplex() ? Ts.complexData()[r + c * 2] : std::complex<double>(Ts.doubleData()[r + c * 2], 0.0);
    };
    auto getUs = [&](size_t r, size_t c) -> std::complex<double> {
        return Us.isComplex() ? Us.complexData()[r + c * 2] : std::complex<double>(Us.doubleData()[r + c * 2], 0.0);
    };

    // Diagonal elements of Ts should be 3 and 1
    EXPECT_NEAR(getTs(0, 0).real(), 3.0, 1e-12);
    EXPECT_NEAR(getTs(1, 1).real(), 1.0, 1e-12);
    EXPECT_NEAR(getTs(1, 0).real(), 0.0, 1e-12); // Subdiagonal is 0

    // Reconstruction invariant: Us * Ts * Us^H == U * T * U^H == T
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            std::complex<double> recon(0.0, 0.0);
            for (size_t k = 0; k < 2; ++k) {
                for (size_t l = 0; l < 2; ++l) {
                    recon += getUs(i, k) * getTs(k, l) * std::conj(getUs(j, l));
                }
            }
            EXPECT_NEAR(recon.real(), td[i + j * 2], 1e-12);
            EXPECT_NEAR(recon.imag(), 0.0, 1e-12);
        }
    }
}

TEST(OrdschurTest, ReorderSchurDomainLHP) {
    // T = [-2 1 0; 0 1 2; 0 0 3]
    Value U = Value::matrix(3, 3);
    auto *ud = U.doubleDataMut();
    std::fill(ud, ud + 9, 0.0);
    ud[0] = 1.0; ud[4] = 1.0; ud[8] = 1.0;

    Value T = Value::matrix(3, 3);
    auto *td = T.doubleDataMut();
    std::fill(td, td + 9, 0.0);
    td[0 + 0*3] = -2.0; td[0 + 1*3] = 1.0;
    td[1 + 1*3] =  1.0; td[1 + 2*3] = 2.0;
    td[2 + 2*3] =  3.0;

    auto [Us, Ts] = ordschur(U, T, "lhp");

    auto getTs = [&](size_t r, size_t c) -> std::complex<double> {
        return Ts.isComplex() ? Ts.complexData()[r + c * 3] : std::complex<double>(Ts.doubleData()[r + c * 3], 0.0);
    };

    // First eigenvalue should be -2.0 (lhp)
    EXPECT_NEAR(getTs(0, 0).real(), -2.0, 1e-12);
}

TEST(OrdschurTest, ReorderSchurComplexUnitDisk) {
    // T = [0.5+0.5i 1; 0 2.0]
    Value U = Value::matrix(2, 2);
    auto *ud = U.doubleDataMut();
    ud[0] = 1.0; ud[4-1] = 1.0;

    Value T = Value::complexMatrix(2, 2);
    auto *td = T.complexDataMut();
    td[0] = std::complex<double>(0.5, 0.5);
    td[1] = std::complex<double>(0.0, 0.0);
    td[2] = std::complex<double>(1.0, 0.0);
    td[3] = std::complex<double>(2.0, 0.0);

    auto [Us, Ts] = ordschur(U, T, "uip"); // |0.5+0.5i| = sqrt(0.5) < 1

    auto getTs = [&](size_t r, size_t c) -> std::complex<double> {
        return Ts.isComplex() ? Ts.complexData()[r + c * 2] : std::complex<double>(Ts.doubleData()[r + c * 2], 0.0);
    };

    // Top-left entry should be 0.5+0.5i
    EXPECT_NEAR(getTs(0, 0).real(), 0.5, 1e-12);
    EXPECT_NEAR(getTs(0, 0).imag(), 0.5, 1e-12);
}
