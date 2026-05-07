// libs/stats/tests/raylstat_test.cpp
// Audit ТЗ closure for raylstat. Closes audit/findings/stats/raylstat.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RaylstatTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RaylstatTest, ScalarMoments)
{
    eval("[m, v] = raylstat(2);");
    EXPECT_NEAR(evalScalar("m"), 2.5066282746310002, 1e-12);  // 2·sqrt(π/2)
    EXPECT_NEAR(evalScalar("v"), 1.7168146928204138, 1e-12);  // 4·(2 - π/2)
}

TEST_F(RaylstatTest, VectorInputs)
{
    eval("[m, v] = raylstat([1 2 3]);");
    EXPECT_NEAR(evalScalar("m(1)"), 1.2533141373155001, 1e-12);
    EXPECT_NEAR(evalScalar("m(2)"), 2.5066282746310002, 1e-12);
    EXPECT_NEAR(evalScalar("m(3)"), 3.7599424119465003, 1e-12);
}

TEST_F(RaylstatTest, InvalidBReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("raylstat(0)")));
    EXPECT_TRUE(std::isnan(evalScalar("raylstat(-1)")));
}
