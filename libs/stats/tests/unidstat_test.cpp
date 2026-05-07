// libs/stats/tests/unidstat_test.cpp
// Audit ТЗ closure for unidstat. Closes audit/findings/stats/unidstat.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UnidstatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(UnidstatTest, ScalarMomentsForN5)
{
    eval("[m, v] = unidstat(5);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 3.0);              // (5+1)/2
    EXPECT_DOUBLE_EQ(evalScalar("v"), 2.0);              // (25-1)/12
}

TEST_F(UnidstatTest, VectorInputs)
{
    eval("[m, v] = unidstat([3 5 10]);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 5.5);
}

TEST_F(UnidstatTest, InvalidNReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("unidstat(0)")));
    EXPECT_TRUE(std::isnan(evalScalar("unidstat(-1)")));
    EXPECT_TRUE(std::isnan(evalScalar("unidstat(2.5)")));  // non-integer
}
