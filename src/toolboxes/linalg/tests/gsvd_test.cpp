// toolboxes/linalg/tests/gsvd_test.cpp
//
// Unit tests for gsvd (generalized SVD).

#include <gtest/gtest.h>
#include <numkit/linalg/gsvd.hpp>
#include <numkit/value/value.hpp>

#include <cmath>

using namespace numkit;
using namespace numkit::linalg;

TEST(GsvdTest, GsvdValuesMatlabRepro) {
    // A = [1 2; 3 4], B = [1 0; 0 1]
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 1.0; ad[1] = 3.0; ad[2] = 2.0; ad[3] = 4.0;

    Value B = Value::matrix(2, 2);
    auto *bd = B.doubleDataMut();
    bd[0] = 1.0; bd[1] = 0.0; bd[2] = 0.0; bd[3] = 1.0;

    Value sigmas = gsvd_values(A, B);
    EXPECT_EQ(sigmas.numel(), 2);

    const double *sd = sigmas.doubleData();
    // MATLAB R2025b generalized singular values: ~0.365966 and ~5.46499
    EXPECT_NEAR(sd[0], 0.365966, 1e-4);
    EXPECT_NEAR(sd[1], 5.46499, 1e-4);
}

TEST(GsvdTest, GsvdFullDecomposition) {
    // A = [1 2; 3 4], B = [1 0; 0 1]
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 1.0; ad[1] = 3.0; ad[2] = 2.0; ad[3] = 4.0;

    Value B = Value::matrix(2, 2);
    auto *bd = B.doubleDataMut();
    bd[0] = 1.0; bd[1] = 0.0; bd[2] = 0.0; bd[3] = 1.0;

    auto [U, V, X, C, S] = gsvd(A, B);

    EXPECT_EQ(U.dims().rows(), 2); EXPECT_EQ(U.dims().cols(), 2);
    EXPECT_EQ(V.dims().rows(), 2); EXPECT_EQ(V.dims().cols(), 2);
    EXPECT_EQ(X.dims().rows(), 2); EXPECT_EQ(X.dims().cols(), 2);
    EXPECT_EQ(C.dims().rows(), 2); EXPECT_EQ(C.dims().cols(), 2);
    EXPECT_EQ(S.dims().rows(), 2); EXPECT_EQ(S.dims().cols(), 2);
}
