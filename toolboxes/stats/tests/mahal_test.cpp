// toolboxes/stats/tests/mahal_test.cpp
// mahal.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MahalTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// D = mahal(X, Y). Mahalanobis distance from each row of X to the
// centroid of Y, scaled by the inverse covariance of Y.

TEST_F(MahalTest, BasicTwoD)
{
    eval("Y = [1 0; 0 1; 1 1; -1 -1; 2 -1; -2 1];");
    eval("d = mahal([0 0; 1 0; 2 2], Y);");
    EXPECT_NEAR(evalScalar("d(1)"), 0.0582750583, 1e-9);
    EXPECT_NEAR(evalScalar("d(2)"), 0.3205128205, 1e-9);
    EXPECT_NEAR(evalScalar("d(3)"), 7.0512820513, 1e-9);
}

TEST_F(MahalTest, CentroidIsZero)
{
    eval("Y = [1 0; 0 1; 1 1; -1 -1; 2 -1; -2 1];");
    eval("d = mahal(mean(Y), Y);");
    EXPECT_NEAR(evalScalar("d"), 0.0, 1e-12);
}

TEST_F(MahalTest, FarPoint)
{
    eval("Y = [1 0; 0 1; 1 1; -1 -1; 2 -1; -2 1];");
    eval("d = mahal([10 10], Y);");
    EXPECT_NEAR(evalScalar("d"), 202.8554778555, 1e-7);
}

TEST_F(MahalTest, ThreeDimensional)
{
    eval("Y3 = [1 0 0; 0 1 0; 0 0 1; 1 1 1; 2 -1 0; -1 0 2; 0 2 -1; 1 -1 1];");
    eval("d = mahal([0 0 0; 1 0 0], Y3);");
    EXPECT_NEAR(evalScalar("d(1)"), 3.2083333333, 1e-9);
    EXPECT_NEAR(evalScalar("d(2)"), 0.5833333333, 1e-9);
}
