// libs/builtin/tests/subspace_test.cpp
//
// Regression guard for builtin::subspace (angle between subspaces).
// Backfilled in tech-debt cycle.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace numkit;

class SubspaceTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SubspaceTest, IdenticalSubspacesIsZero)
{
    eval("A = [1 0; 0 1; 0 0]; t = subspace(A, A);");
    EXPECT_NEAR(evalScalar("t"), 0.0, 1e-12);
}

TEST_F(SubspaceTest, IdenticalReorderedColumns)
{
    // Same column space, different basis.
    eval("A = [1 0; 0 1; 0 0]; B = [0 1; 1 0; 0 0]; t = subspace(A, B);");
    EXPECT_NEAR(evalScalar("t"), 0.0, 1e-12);
}

TEST_F(SubspaceTest, OrthogonalIsHalfPi)
{
    // 1D subspaces along x-axis vs z-axis -> orthogonal -> pi/2.
    eval("A = [1; 0; 0]; B = [0; 0; 1]; t = subspace(A, B);");
    EXPECT_NEAR(evalScalar("t"), 1.5707963267948966, 1e-12);  // pi/2
}

TEST_F(SubspaceTest, RowMismatchThrows)
{
    EXPECT_THROW(eval("subspace([1 2; 3 4], [1; 2; 3]);"), std::exception);
}
