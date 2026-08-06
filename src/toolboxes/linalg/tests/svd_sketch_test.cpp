// toolboxes/linalg/tests/svd_sketch_test.cpp
//
// Unit tests for svdsketch and svdappend.

#include <gtest/gtest.h>
#include <numkit/linalg/svd_sketch.hpp>
#include <numkit/linalg/decompositions.hpp>
#include <numkit/value/value.hpp>

#include <cmath>

using namespace numkit;
using namespace numkit::linalg;

TEST(SvdSketchTest, SvdSketchLowRankApproximation) {
    // Rank 2 matrix A = u1*v1' + u2*v2' (4x4)
    Value A = Value::matrix(4, 4);
    auto *ad = A.doubleDataMut();
    std::fill(ad, ad + 16, 0.0);
    // Rank 2 rank components
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            ad[i + j * 4] = (i + 1.0) * (j + 1.0) + (i == j ? 10.0 : 0.0);
        }
    }

    auto [U, S, V] = svdsketch(A, 1e-3);
    EXPECT_GT(U.dims().cols(), 0);
    EXPECT_EQ(U.dims().rows(), 4);
    EXPECT_EQ(V.dims().rows(), 4);
}

TEST(SvdSketchTest, SvdAppendIncrementalColumnUpdate) {
    // A = [1 0; 0 2]
    Value A = Value::matrix(2, 2);
    auto *ad = A.doubleDataMut();
    ad[0] = 1.0; ad[1] = 0.0; ad[2] = 0.0; ad[3] = 2.0;

    auto [U, S, V] = svd_decompose(A);

    // Append column A_new = [3; 4]
    Value A_new = Value::matrix(2, 1);
    auto *andata = A_new.doubleDataMut();
    andata[0] = 3.0; andata[1] = 4.0;

    auto [U_new, S_new, V_new] = svdappend(U, S, V, A_new);

    EXPECT_EQ(U_new.dims().rows(), 2);
    EXPECT_EQ(V_new.dims().rows(), 3); // 2 + 1 columns total

    // Reconstruct A_full = U_new * S_new * V_new'
    // Should equal [1 0 3; 0 2 4]
    double a_full[2][3] = {{1.0, 0.0, 3.0}, {0.0, 2.0, 4.0}};

    const double *ud = U_new.doubleData();
    const double *sd = S_new.doubleData();
    const double *vd = V_new.doubleData();
    size_t r = S_new.dims().rows();

    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            double val = 0.0;
            for (size_t k = 0; k < r; ++k) {
                val += ud[i + k * 2] * sd[k + k * r] * vd[j + k * 3];
            }
            EXPECT_NEAR(val, a_full[i][j], 1e-4);
        }
    }
}
