// libs/stats/tests/ecdf_test.cpp
// ecdf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EcdfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("y = [1 2 2 3 5 5 5 7 8]';");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(EcdfTest, DefaultCdf)
{
    eval("[f, x] = ecdf(y);");
    EXPECT_DOUBLE_EQ(evalScalar("f(1)"), 0.0);
    EXPECT_NEAR(evalScalar("f(2)"), 1.0/9.0, 1e-12);
    EXPECT_NEAR(evalScalar("f(3)"), 3.0/9.0, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("f(7)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("x(7)"), 8.0);
}

// 2026-05-08 — gap closure: 'Function' N-V parsed.
TEST_F(EcdfTest, SurvivorMode)
{
    eval("[fs, xs] = ecdf(y, 'Function', 'survivor');");
    EXPECT_DOUBLE_EQ(evalScalar("fs(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("fs(numel(fs))"), 0.0);
    EXPECT_NEAR(evalScalar("fs(3)"), 6.0/9.0, 1e-12);
}

// gap closure: cumulative hazard via Nelson-Aalen estimator.
TEST_F(EcdfTest, CumulativeHazard)
{
    eval("[fh, xh] = ecdf(y, 'Function', 'cumulative hazard');");
    EXPECT_DOUBLE_EQ(evalScalar("fh(1)"), 0.0);
    EXPECT_NEAR(evalScalar("fh(2)"), 1.0/9.0, 1e-12);
    EXPECT_NEAR(evalScalar("fh(3)"), 1.0/9.0 + 2.0/8.0, 1e-12);
    EXPECT_NEAR(evalScalar("fh(numel(fh))"), 2.6277777778, 1e-9);
}

// gap closure: Frequency weighting.
TEST_F(EcdfTest, FrequencyWeighting)
{
    eval("freq = [1 2 1 1 3 1 1 1 1]';");
    eval("[ff, xf] = ecdf(y, 'Frequency', freq);");
    EXPECT_DOUBLE_EQ(evalScalar("ff(1)"), 0.0);
    EXPECT_NEAR(evalScalar("ff(2)"), 1.0/12.0, 1e-12);
    EXPECT_NEAR(evalScalar("ff(3)"), 4.0/12.0, 1e-12);
    EXPECT_DOUBLE_EQ(evalScalar("ff(numel(ff))"), 1.0);
}

// gap closure: 4-output form with Greenwood-style binomial bounds.
TEST_F(EcdfTest, FourOutputBoundsBinomial)
{
    eval("[fc, xc, flo, fup] = ecdf(y);");
    EXPECT_TRUE(std::isnan(evalScalar("flo(1)")));
    EXPECT_TRUE(std::isnan(evalScalar("flo(numel(flo))")));
    EXPECT_NEAR(evalScalar("flo(2)"), 0.0,        1e-9);
    EXPECT_NEAR(evalScalar("fup(2)"), 0.31643,    1e-4);
    EXPECT_NEAR(evalScalar("flo(3)"), 0.0253547,  1e-6);
    EXPECT_NEAR(evalScalar("fup(3)"), 0.641312,   1e-5);
}

// gap closure: Censoring N-V errors with clear message (not silent).
TEST_F(EcdfTest, CensoringRejected)
{
    bool threw = false;
    try { eval("ecdf(y, 'Censoring', [0 0 0 0 0 0 0 1 1]');"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}
