// toolboxes/signal/tests/cell2sos_test.cpp
//
// Regression guard for cell2sos (Phase 4.10). Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Cell2sosTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Cell2sosTest, GainEmbeddedInFirstSection)
{
    // Help example 1: gain in leading first-order numerator/denom.
    eval("c = {{[0.0181 0.0181],[1.0000 -0.5095]}, "
         "     {[1 2 1],     [1 -1.2505 0.5457]}};"
         "s = cell2sos(c);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(s, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(s, 2)")), 6);
    // Row 1: [0.0181, 0.0181, 0, 1, -0.5095, 0]
    EXPECT_NEAR(evalScalar("s(1, 1)"),  0.0181, 1e-9);
    EXPECT_NEAR(evalScalar("s(1, 2)"),  0.0181, 1e-9);
    EXPECT_NEAR(evalScalar("s(1, 3)"),  0.0,    1e-9);
    EXPECT_NEAR(evalScalar("s(1, 5)"), -0.5095, 1e-9);
    EXPECT_NEAR(evalScalar("s(1, 6)"),  0.0,    1e-9);
    // Row 2: [1, 2, 1, 1, -1.2505, 0.5457]
    EXPECT_NEAR(evalScalar("s(2, 6)"),  0.5457, 1e-9);
}

TEST_F(Cell2sosTest, ScalarGainSectionExtracted)
{
    // 2-output form: leading {scalar, scalar} → gain g.
    eval("c = {{0.0181, 1}, {[1 1], [1 -0.5095]}, "
         "     {[1 2 1], [1 -1.2505 0.5457]}};"
         "[s, g] = cell2sos(c);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(s, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(s, 2)")), 6);
    EXPECT_NEAR(evalScalar("g"), 0.0181, 1e-9);
    // Remaining rows should NOT include the scalar gain row.
    EXPECT_NEAR(evalScalar("s(1, 1)"), 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("s(2, 1)"), 1.0, 1e-9);
}

TEST_F(Cell2sosTest, NonScalarFirstNoGainExtraction)
{
    // No leading scalar pair → g defaults to 1.
    eval("c = {{[0.5 0.5], [1 -0.3]}};"
         "[s, g] = cell2sos(c);");
    EXPECT_NEAR(evalScalar("g"), 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("s(1, 1)"), 0.5, 1e-9);
}

TEST_F(Cell2sosTest, RejectsNonCellInput)
{
    bool threw = false;
    try { eval("cell2sos([1 2 3]);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
