// toolboxes/stats/tests/exppdf_test.cpp
// exppdf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ExppdfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ExppdfTest, DefaultMu1)
{
    // exppdf(x) ≡ exppdf(x, 1) → exp(-x).
    EXPECT_NEAR(evalScalar("exppdf(2)"), 0.1353352832366127, 1e-12);
}

TEST_F(ExppdfTest, NonDefaultMu)
{
    EXPECT_NEAR(evalScalar("exppdf(2, 3)"), 0.1711390396775307, 1e-12);
}

TEST_F(ExppdfTest, VectorX)
{
    eval("y = exppdf([0 1 2 5], 2);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.5);                  // 1/mu at x=0
    EXPECT_NEAR(evalScalar("y(2)"), 0.3032653298563167, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 0.0410424993119494, 1e-12);
}

TEST_F(ExppdfTest, NegativeXReturnsZero)
{
    EXPECT_DOUBLE_EQ(evalScalar("exppdf(-1, 2)"), 0.0);
}

TEST_F(ExppdfTest, InvalidMuReturnsNaN)
{
    EXPECT_TRUE(std::isnan(evalScalar("exppdf(2,  0)")));
    EXPECT_TRUE(std::isnan(evalScalar("exppdf(2, -1)")));
}
