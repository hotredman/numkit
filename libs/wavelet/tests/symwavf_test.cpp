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
    StdEngine engine;
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

// Bug fix 2026-05-08 — extended Symlet table from sym2/sym4 to sym2..sym10.

TEST_F(SymwavfTest, Sym3ToSym10Lengths)
{
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(symwavf('sym3'))")),  6u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(symwavf('sym5'))")),  10u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(symwavf('sym6'))")),  12u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(symwavf('sym7'))")),  14u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(symwavf('sym8'))")),  16u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(symwavf('sym9'))")),  18u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(symwavf('sym10'))")), 20u);
}

TEST_F(SymwavfTest, ExtendedSumsToOne)
{
    for (const std::string &name : {"sym3", "sym5", "sym6", "sym7", "sym8", "sym9", "sym10"}) {
        const std::string expr = "sum(symwavf('" + name + "'))";
        EXPECT_NEAR(evalScalar(expr), 1.0, 1e-12) << name;
    }
}
