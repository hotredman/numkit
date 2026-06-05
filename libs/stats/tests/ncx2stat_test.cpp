// libs/stats/tests/ncx2stat_test.cpp
// ncx2stat.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Ncx2statTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Ncx2statTest, ScalarMomentsNCX2_5_2)
{
    eval("[m, v] = ncx2stat(5, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("m"),  7.0);   // k + λ
    EXPECT_DOUBLE_EQ(evalScalar("v"), 18.0);   // 2(k + 2λ)
}

TEST_F(Ncx2statTest, VectorBroadcasting)
{
    eval("[m, v] = ncx2stat([3 5 10], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("m(1)"),  5.0);
    EXPECT_DOUBLE_EQ(evalScalar("m(3)"), 12.0);
}

TEST_F(Ncx2statTest, Lambda0ReducesToCentralChi2)
{
    eval("[m, v] = ncx2stat(5, 0);");
    EXPECT_DOUBLE_EQ(evalScalar("m"),  5.0);   // central χ²(5): m=k, v=2k
    EXPECT_DOUBLE_EQ(evalScalar("v"), 10.0);
}

TEST_F(Ncx2statTest, InvalidParamsReturnNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("ncx2stat( 0, 2)")));   // k <= 0
    EXPECT_TRUE(std::isnan(evalScalar("ncx2stat(-1, 2)")));
    EXPECT_TRUE(std::isnan(evalScalar("ncx2stat( 5, -1)")));  // λ < 0
}
