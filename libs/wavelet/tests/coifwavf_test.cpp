// libs/wavelet/tests/coifwavf_test.cpp
//
// Backfill gtest for libs/wavelet/src/filter/families.cpp::coifwavf.
// Reference values from MATLAB R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CoifwavfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
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
