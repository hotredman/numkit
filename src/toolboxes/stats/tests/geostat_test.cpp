// toolboxes/stats/tests/geostat_test.cpp
// geostat.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GeostatTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GeostatTest, ScalarMomentsForP03)
{
    // Number-of-failures form: m = (1-p)/p = 0.7/0.3, v = 0.7/0.09.
    eval("[m, v] = geostat(0.3);");
    EXPECT_NEAR(evalScalar("m"), 7.0/3.0, 1e-12);
    EXPECT_NEAR(evalScalar("v"), 7.0/0.9, 1e-12);
}

TEST_F(GeostatTest, VectorInputs)
{
    eval("[m, v] = geostat([0.1 0.5 0.9]);");
    EXPECT_NEAR(evalScalar("m(1)"), 9.0,        1e-12);
    EXPECT_NEAR(evalScalar("m(2)"), 1.0,        1e-12);
    EXPECT_NEAR(evalScalar("m(3)"), 1.0/9.0,    1e-12);
}

TEST_F(GeostatTest, P1IsZeroVariance)
{
    eval("[m, v] = geostat(1);");
    EXPECT_DOUBLE_EQ(evalScalar("m"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v"), 0.0);
}

TEST_F(GeostatTest, OutOfRangePReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("geostat(0)")));
    EXPECT_TRUE(std::isnan(evalScalar("geostat(-0.1)")));
    EXPECT_TRUE(std::isnan(evalScalar("geostat(1.5)")));
}
