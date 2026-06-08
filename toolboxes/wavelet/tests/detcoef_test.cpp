// toolboxes/wavelet/tests/detcoef_test.cpp
// detcoef.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DetcoefTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// MATLAB R2025b convention:
//   detcoef(C, L)            → level = numel(L) - 2 (deepest detail)
//   detcoef(C, L, n)         → detail at level n
//   detcoef(C, L, v, 'cells')→ cell array of details for each level in v

TEST_F(DetcoefTest, DefaultLevelIsDeepest)
{
    // Bug fix 2026-05-08: 2-arg form was throwing. MATLAB default returns
    // level numel(L)-2 (deepest), NOT level 1 as an early note claimed.
    eval("[c, l] = wavedec(1:16, 3, 'db1');");
    eval("d = detcoef(c, l);");
    // Deepest detail (level 3) for db1 of (1:16): two values ~ -5.6569.
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(d)")), 2u);
    EXPECT_NEAR(evalScalar("d(1)"), -5.6568542495, 1e-9);
    EXPECT_NEAR(evalScalar("d(2)"), -5.6568542495, 1e-9);
}

TEST_F(DetcoefTest, ExplicitLevel1)
{
    eval("[c, l] = wavedec(1:16, 3, 'db1'); d = detcoef(c, l, 1);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(d)")), 8u);
    EXPECT_NEAR(evalScalar("d(1)"), -0.7071067812, 1e-9);
}

TEST_F(DetcoefTest, ExplicitLevel3)
{
    eval("[c, l] = wavedec(1:16, 3, 'db1'); d = detcoef(c, l, 3);");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(d)")), 2u);
    EXPECT_NEAR(evalScalar("d(1)"), -5.6568542495, 1e-9);
}

TEST_F(DetcoefTest, CellsForm)
{
    // Bug fix 2026-05-08: 'cells' form was throwing.
    eval("[c, l] = wavedec(1:16, 3, 'db1'); D = detcoef(c, l, [1 2], 'cells');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(D)")), 2u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(D{1})")), 8u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(D{2})")), 4u);
    EXPECT_NEAR(evalScalar("D{1}(1)"), -0.7071067812, 1e-9);
    EXPECT_NEAR(evalScalar("D{2}(1)"), -2.0, 1e-12);
}

TEST_F(DetcoefTest, CellsFormMixedLevels)
{
    // Cells with non-sequential levels.
    eval("[c, l] = wavedec(1:16, 3, 'db1'); D = detcoef(c, l, [3 1], 'cells');");
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(D{1})")), 2u);  // level 3
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(D{2})")), 8u);  // level 1
}

// DEEP-PROBE c175: a vector of levels with a single output returns a CELL
// array of the per-level details (no 'cells' flag needed). vs MATLAB R2025b.
TEST_F(DetcoefTest, VectorLevelsReturnsCell)
{
    eval("[c, l] = wavedec(1:16, 3, 'db1'); cv = detcoef(c, l, [1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(cv))"), 1.0);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(cv)")), 3u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(cv{1})")), 8u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(cv{2})")), 4u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(cv{3})")), 2u);
    EXPECT_NEAR(evalScalar("cv{1}(1)"), -0.7071067812, 1e-9);
    EXPECT_NEAR(evalScalar("cv{3}(1)"), -5.6568542495, 1e-9);
}

// detcoef(C, L, 'cells') returns a cell of ALL levels 1..nMax.
TEST_F(DetcoefTest, CellsStringAllLevels)
{
    eval("[c, l] = wavedec(1:16, 3, 'db1'); ca = detcoef(c, l, 'cells');");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(ca))"), 1.0);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(ca)")), 3u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(ca{1})")), 8u);  // level 1
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(ca{3})")), 2u);  // level 3
}

// [d1, d2, d3] = detcoef(C, L, [1 2 3]) deals one detail per output.
TEST_F(DetcoefTest, VectorLevelsMultiOutput)
{
    eval("[c, l] = wavedec(1:16, 3, 'db1'); [e1, e2, e3] = detcoef(c, l, [1 2 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(iscell(e1))"), 0.0);   // each is a vector
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(e1)")), 8u);
    EXPECT_EQ(static_cast<size_t>(evalScalar("numel(e3)")), 2u);
    EXPECT_NEAR(evalScalar("e1(1)"), -0.7071067812, 1e-9);
    EXPECT_NEAR(evalScalar("e3(1)"), -5.6568542495, 1e-9);
}
