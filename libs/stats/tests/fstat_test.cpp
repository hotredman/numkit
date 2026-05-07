// libs/stats/tests/fstat_test.cpp
//
// Audit ТЗ closure for fstat. Reference values from MATLAB R2025b.
// Closes audit/findings/stats/fstat.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class FstatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(FstatTest, ScalarMomentsBasic)
{
    eval("[m, v] = fstat(5, 10);");
    EXPECT_NEAR(evalScalar("m"), 1.25,                1e-12); // 10/(10-2) = 1.25
    EXPECT_NEAR(evalScalar("v"), 1.3541666666666667,  1e-12);
}

TEST_F(FstatTest, VectorBroadcasting)
{
    // v2=3 → variance NaN (needs v2>4); v2=5,10 → variance defined.
    eval("[m, v] = fstat([5 5 5], [3 5 10]);");
    EXPECT_NEAR(evalScalar("m(1)"), 3.0,                1e-12); // 3/(3-2)
    EXPECT_NEAR(evalScalar("m(2)"), 1.6666666666666667, 1e-12); // 5/(5-2)
    EXPECT_NEAR(evalScalar("m(3)"), 1.25,               1e-12);
    EXPECT_TRUE(std::isnan(evalScalar("v(1)")));        // v2 ≤ 4 → variance NaN
    EXPECT_NEAR(evalScalar("v(2)"), 8.8888888888888893, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"), 1.3541666666666667, 1e-12);
}

TEST_F(FstatTest, V2EqualsTwo_MeanIsNaN)
{
    // Mean defined only for v2 > 2.
    eval("[m, v] = fstat(5, 2);");
    EXPECT_TRUE(std::isnan(evalScalar("m")));
    EXPECT_TRUE(std::isnan(evalScalar("v")));
}

TEST_F(FstatTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("fstat( 0, 10)")));
    EXPECT_TRUE(std::isnan(evalScalar("fstat( 5,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("fstat(-1, 10)")));
}
