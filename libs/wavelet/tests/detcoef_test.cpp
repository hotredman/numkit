// libs/wavelet/tests/detcoef_test.cpp
// Audit ТЗ closure for detcoef. Closes audit/findings/wavelet/detcoef.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DetcoefTest : public ::testing::Test
{
public:
    Engine engine;
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
    // level numel(L)-2 (deepest), NOT level 1 as auditor's note claimed.
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
