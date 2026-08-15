// toolboxes/linalg/tests/balance_perm_test.cpp
//
// Unit tests for balance permutation phase.

#include <gtest/gtest.h>
#include <numkit/linalg/balance.hpp>
#include <numkit/value/value.hpp>

using namespace numkit;
using namespace numkit::linalg;

TEST(BalancePermTest, IsolatedTriangularMatrixPermutation) {
    // A = [1 2 3; 0 4 5; 0 0 6] (upper triangular matrix with isolated eigenvalues)
    Value A = Value::matrix(3, 3);
    auto *ad = A.doubleDataMut();
    ad[0] = 1.0; ad[1] = 0.0; ad[2] = 0.0;
    ad[3] = 2.0; ad[4] = 4.0; ad[5] = 0.0;
    ad[6] = 3.0; ad[7] = 5.0; ad[8] = 6.0;

    BalanceResult resPerm = balance_impl(A, /*noperm=*/false);
    EXPECT_EQ(resPerm.B.dims().rows(), 3);
    EXPECT_EQ(resPerm.B.dims().cols(), 3);

    // Permutation column should not be all 1, 2, 3 (some entries swapped)
    const double *pd = resPerm.perm_col.doubleData();
    EXPECT_GT(resPerm.perm_col.numel(), 0);

    BalanceResult resNoperm = balance_impl(A, /*noperm=*/true);
    const double *pd_noperm = resNoperm.perm_col.doubleData();
    EXPECT_DOUBLE_EQ(pd_noperm[0], 1.0);
    EXPECT_DOUBLE_EQ(pd_noperm[1], 2.0);
    EXPECT_DOUBLE_EQ(pd_noperm[2], 3.0);
}
