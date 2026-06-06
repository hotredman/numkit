// libs/stats/tests/nbinstat_test.cpp
// nbinstat.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class NbinstatTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(NbinstatTest, ScalarMomentsNB5p03)
{
    eval("[m, v] = nbinstat(5, 0.3);");
    EXPECT_NEAR(evalScalar("m"), 5.0 * 0.7 / 0.3,            1e-12);  // ≈11.667
    EXPECT_NEAR(evalScalar("v"), 5.0 * 0.7 / (0.3 * 0.3),    1e-12);  // ≈38.889
}

TEST_F(NbinstatTest, VectorBroadcasting)
{
    eval("[m, v] = nbinstat([3 5 10], 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),  3.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 10.0);
}

TEST_F(NbinstatTest, P1IsZeroVariance)
{
    eval("[m, v] = nbinstat(5, 1);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v"), 0.0);
}

TEST_F(NbinstatTest, NonIntegerRIsValid)
{
    // Pólya generalisation: r need not be integer.
    eval("[m, v] = nbinstat(2.5, 0.5);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("v"), 5.0);
}

TEST_F(NbinstatTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("nbinstat(5,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("nbinstat(0,  0.5)")));
    EXPECT_TRUE(std::isnan(evalScalar("nbinstat(-1, 0.5)")));
    EXPECT_TRUE(std::isnan(evalScalar("nbinstat(5,  1.5)")));
}
