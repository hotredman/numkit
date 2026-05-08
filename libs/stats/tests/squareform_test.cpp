// libs/stats/tests/squareform_test.cpp
// Audit ТЗ closure for squareform. Closes audit/findings/cluster/squareform.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SquareformTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SquareformTest, VectorToMatrix3pt)
{
    eval("M = squareform([1 2 3]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(M, 1)")), 3u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(M, 2)")), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,1)"), 0);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,2)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,3)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,3)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("M(3,3)"), 0);
}

TEST_F(SquareformTest, MatrixToVector3pt)
{
    eval("v = squareform([0 1 2; 1 0 3; 2 3 0]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(v)")), 3u);
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 3);
}

TEST_F(SquareformTest, VectorToMatrix4pt)
{
    eval("M = squareform([1 2 3 4 5 6]);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(M, 1)")), 4u);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,2)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,3)"), 2);
    EXPECT_DOUBLE_EQ(evalScalar("M(1,4)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,3)"), 4);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,4)"), 5);
    EXPECT_DOUBLE_EQ(evalScalar("M(3,4)"), 6);
}

TEST_F(SquareformTest, ExplicitTomatrix)
{
    eval("M = squareform([1 2 3], 'tomatrix');");
    EXPECT_DOUBLE_EQ(evalScalar("M(1,2)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("M(2,3)"), 3);
}

TEST_F(SquareformTest, ExplicitTovector)
{
    eval("v = squareform([0 1 2; 1 0 3; 2 3 0], 'tovector');");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 1);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 3);
}

TEST_F(SquareformTest, RoundTripIdentity)
{
    eval("d = [10 20 30 40 50 60];");
    eval("M = squareform(d);");
    eval("d_rt = squareform(M);");
    eval("err = max(abs(d - d_rt));");
    EXPECT_DOUBLE_EQ(evalScalar("err"), 0.0);
}

TEST_F(SquareformTest, ScalarZero)
{
    // squareform(0): scalar 0 is treated as a length-1 distance vector,
    // producing a 2x2 zero matrix (the only fit since N(N-1)/2 = 1 → N = 2).
    eval("y = squareform(0);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 1)")), 2u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(y, 2)")), 2u);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1,2)"), 0.0);
}
