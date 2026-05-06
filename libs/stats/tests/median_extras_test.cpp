// libs/stats/tests/median_extras_test.cpp
//
// Closes audit/closed/stats/median.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MedianExtrasTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("v = [2 5 3 7 4 6 NaN 8 1 9]';");
        engine.eval("A = [1 4 7; 2 5 8; 3 6 9; 4 7 10; 5 8 11];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MedianExtrasTest, DefaultPoisonsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("median(v)")));
}

TEST_F(MedianExtrasTest, OmitnanExplicit)
{
    EXPECT_DOUBLE_EQ(evalScalar("median(v, 'omitnan')"), 5);
}

TEST_F(MedianExtrasTest, AllString)
{
    EXPECT_DOUBLE_EQ(evalScalar("median(A, 'all')"), 6);
}

TEST_F(MedianExtrasTest, Vecdim)
{
    EXPECT_DOUBLE_EQ(evalScalar("median(A, [1 2])"), 6);
}

TEST_F(MedianExtrasTest, BasicValuesUnchanged)
{
    EXPECT_DOUBLE_EQ(evalScalar("median([1 2 3 4 5])"), 3);
    eval("y = median(A);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 6);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 9);
}
