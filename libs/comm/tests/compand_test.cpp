// libs/comm/tests/compand_test.cpp
//
// Regression guard for compand() — μ-law / A-law compressor /
// expander. Bit-equality with MATLAB R2025b expected.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CompandTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CompandTest, MuLawCompressKnownValues)
{
    // MATLAB reference: compand([0 0.25 0.5 0.75 1.0], 255, 1, 'mu/compressor')
    eval("y = compand([0 0.25 0.5 0.75 1.0], 255, 1, 'mu/compressor');");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_NEAR(evalScalar("y(2)"), 0.7521010359608192, 1e-14);
    EXPECT_NEAR(evalScalar("y(3)"), 0.8757030686492348, 1e-14);
    EXPECT_NEAR(evalScalar("y(4)"), 0.9483549734952283, 1e-14);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 1.0);
}

TEST_F(CompandTest, MuLawRoundTrip)
{
    eval("x = [0.1 0.3 0.5 0.7 0.9];"
         "y = compand(x, 255, 1, 'mu/compressor');"
         "z = compand(y, 255, 1, 'mu/expander');"
         "err = max(abs(x - z));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(CompandTest, ALawCompressKnownValues)
{
    eval("y = compand([0 0.25 0.5 0.75 1.0], 87.6, 1, 'A/compressor');");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 0.0);
    EXPECT_NEAR(evalScalar("y(2)"), 0.7466928858214664, 1e-14);
    EXPECT_NEAR(evalScalar("y(3)"), 0.8733464429107333, 1e-14);
    EXPECT_NEAR(evalScalar("y(4)"), 0.9474340243909005, 1e-14);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 1.0);
}

TEST_F(CompandTest, ALawRoundTrip)
{
    eval("x = [0.05 0.15 0.4 0.6 0.85];"
         "y = compand(x, 87.6, 1, 'A/compressor');"
         "z = compand(y, 87.6, 1, 'A/expander');"
         "err = max(abs(x - z));");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(CompandTest, NegativePreservesSign)
{
    eval("y = compand(-[0 0.25 0.5 0.75 1.0], 255, 1, 'mu/compressor');");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),  0.0);
    EXPECT_NEAR(evalScalar("y(2)"), -0.7521010359608192, 1e-14);
    EXPECT_NEAR(evalScalar("y(5)"), -1.0,                1e-14);
}

TEST_F(CompandTest, ShapePreserved)
{
    eval("y = compand([0.1 0.2 0.3; 0.4 0.5 0.6], 255, 1, 'mu/compressor');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 3);
}

TEST_F(CompandTest, RejectsBadMethod)
{
    bool threw = false;
    try {
        eval("compand([0.5], 255, 1, 'foobar');");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
