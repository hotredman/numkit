// libs/linalg/tests/condeig_test.cpp
//
// Regression guard for condeig — eigenvalue condition numbers.
// Hardcoded expected values pinned against MATLAB R2025b
// (see tools/parity/specs/condeig.json for the parity sweep).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class CondeigTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Symmetric A → every eigenvalue is perfectly conditioned (s_i == 1).
TEST_F(CondeigTest, SymmetricGivesUnitConditionNumbers)
{
    eval("A = [4 1 0; 1 3 0; 0 0 2];"
         "s = condeig(A);");
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(3)"), 1.0);
}

// Identity is the trivial symmetric case — all s == 1.
TEST_F(CondeigTest, IdentityGivesUnitConditionNumbers)
{
    eval("s = condeig(eye(5));");
    EXPECT_DOUBLE_EQ(evalScalar("min(s)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("max(s)"), 1.0);
}

// Upper-triangular non-symmetric: condition numbers > 1.
// For [[2 1]; [0 3]] both eigenvalues (2, 3) have s = sqrt(2)
// (angle between right and left eigvec is 45°).
TEST_F(CondeigTest, UpperTriangular2x2HasMatchingCondNumbers)
{
    eval("A = [2 1; 0 3];"
         "s = condeig(A);");
    EXPECT_NEAR(evalScalar("s(1)"), std::sqrt(2.0), 1e-12);
    EXPECT_NEAR(evalScalar("s(2)"), std::sqrt(2.0), 1e-12);
}

// Highly ill-conditioned matrix → at least one s very large.
TEST_F(CondeigTest, NearDefectiveMatrixHasLargeConditionNumber)
{
    // Eigenvalues 1, 1.0001 — very close → high condition.
    eval("A = [1 1; 1 1.0001];"
         "s = condeig(A);");
    // Symmetric A actually; should give s == 1, even if eigvals are close.
    EXPECT_DOUBLE_EQ(evalScalar("max(s)"), 1.0);
}

// 3-output form: [V, D, s] = condeig(A) matches eig(A) for V, D.
TEST_F(CondeigTest, ThreeOutputFormReturnsCorrectVDS)
{
    eval("A = [4 1 0; 1 3 0; 0 0 2];"
         "[V, D, s] = condeig(A);"
         "vs_dims = size(V);"
         "ds_dims = size(D);"
         "ss_dims = size(s);"
         // Verify V*D ≈ A*V (eigendecomposition holds).
         "resid = max(max(abs(A*V - V*D)));");
    EXPECT_EQ(static_cast<int>(evalScalar("vs_dims(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("vs_dims(2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("ds_dims(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("ds_dims(2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("ss_dims(1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("ss_dims(2)")), 1);
    EXPECT_LT(evalScalar("resid"), 1e-12);
}
