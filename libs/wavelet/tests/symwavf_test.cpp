// libs/wavelet/tests/symwavf_test.cpp
//
// Backfill gtest for libs/wavelet/src/filter/families.cpp::symwavf.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SymwavfTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SymwavfTest, Sym2)
{
    eval("h = symwavf('sym2');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(h)")), 4u);
    EXPECT_NEAR(evalScalar("h(1)"),  0.341506, 1e-5);
    EXPECT_NEAR(evalScalar("h(4)"), -0.091506, 1e-5);
}

TEST_F(SymwavfTest, Sym4)
{
    eval("h = symwavf('sym4');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(h)")), 8u);
    EXPECT_NEAR(evalScalar("h(1)"),  0.022785, 1e-5);
    EXPECT_NEAR(evalScalar("h(8)"), -0.053574, 1e-5);
}

TEST_F(SymwavfTest, NormalisedToSumOne)
{
    EXPECT_NEAR(evalScalar("sum(symwavf('sym2'))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("sum(symwavf('sym4'))"), 1.0, 1e-12);
}
