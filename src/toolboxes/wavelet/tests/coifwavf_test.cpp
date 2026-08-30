// toolboxes/wavelet/tests/coifwavf_test.cpp
//
// Backfill gtest for toolboxes/wavelet/src/filter/families.cpp::coifwavf.
// Reference values from MATLAB R2025b probe.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CoifwavfTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CoifwavfTest, Coif1Length6)
{
    eval("h = coifwavf('coif1');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(h)")), 6u);
    EXPECT_NEAR(evalScalar("h(1)"), -0.051430, 1e-5);
    EXPECT_NEAR(evalScalar("h(2)"),  0.238930, 1e-5);
    EXPECT_NEAR(evalScalar("h(3)"),  0.602859, 1e-5);
    EXPECT_NEAR(evalScalar("h(4)"),  0.272140, 1e-5);
    EXPECT_NEAR(evalScalar("h(5)"), -0.051430, 1e-5);
    EXPECT_NEAR(evalScalar("h(6)"), -0.011070, 1e-5);
}

TEST_F(CoifwavfTest, NormalisedToSumOne)
{
    EXPECT_NEAR(evalScalar("sum(coifwavf('coif1'))"), 1.0, 1e-12);
}

TEST_F(CoifwavfTest, RowOrientation)
{
    eval("h = coifwavf('coif1');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(h, 1)")), 1u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("size(h, 2)")), 6u);
}

// Bug fix 2026-05-08 — extended Coiflet table from coif1 to coif1..coif5.

TEST_F(CoifwavfTest, Coif2ToCoif5Lengths)
{
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(coifwavf('coif2'))")), 12u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(coifwavf('coif3'))")), 18u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(coifwavf('coif4'))")), 24u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(coifwavf('coif5'))")), 30u);
}

TEST_F(CoifwavfTest, ExtendedSumsToOne)
{
    // MATLAB's published Coiflet coefficients are decimal-truncated, so
    // sums are within ~1e-10 of unity rather than full IEEE precision.
    // (sum(coifwavf('coif5')) = 1.0000000001 in MATLAB itself.)
    for (const std::string &name : {"coif2", "coif3", "coif4", "coif5"}) {
        const std::string expr = "sum(coifwavf('" + name + "'))";
        EXPECT_NEAR(evalScalar(expr), 1.0, 1e-9) << name;
    }
}

TEST_F(CoifwavfTest, Coif2HighPrecision)
{
    eval("h = coifwavf('coif2');");
    EXPECT_NEAR(evalScalar("h(1)"),   0.011587596739, 1e-9);
    EXPECT_NEAR(evalScalar("h(12)"), -0.000509505400, 1e-9);
}
