// libs/builtin/tests/math_reductions_batch_test.cpp
//
// Audit ТЗ batch closure for math primitives + reductions — 11 functions:
//   cospi / sinpi
//   deg2rad / rad2deg
//   eps
//   cumsum / cumprod / diff
//   diag
//   prod / sum
//
// All flagged "no major gap detected" — bit-identical MATLAB R2025b
// on probed inputs (parity tol=1e-12).
//
// eps has 3 known sub-gaps documented in audit/closed/builtin/eps.md
// (no-arg form, fractional input, vector input) — only the working
// scalar-positive path is pinned here.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MathReductionsBatchTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MathReductionsBatchTest, CospiSinpi)
{
    EXPECT_DOUBLE_EQ(evalScalar("cospi(0)"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("cospi(0.5)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("cospi(1)"),   -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(0)"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(0.5)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sinpi(1)"),    0.0);
}

TEST_F(MathReductionsBatchTest, Deg2RadRad2Deg)
{
    EXPECT_NEAR(evalScalar("deg2rad(180)"),  3.141592653589793, 1e-12);
    EXPECT_NEAR(evalScalar("deg2rad(90)"),   1.570796326794897, 1e-12);
    EXPECT_NEAR(evalScalar("rad2deg(pi)"),   180.0,             1e-12);
    EXPECT_NEAR(evalScalar("rad2deg(pi/2)"), 90.0,              1e-12);
    // Round-trip
    EXPECT_NEAR(evalScalar("rad2deg(deg2rad(73))"), 73.0, 1e-12);
}

TEST_F(MathReductionsBatchTest, Eps)
{
    EXPECT_DOUBLE_EQ(evalScalar("eps(1)"), 2.220446049250313e-16);
}

TEST_F(MathReductionsBatchTest, Cumsum)
{
    eval("y = cumsum([1, 2, 3, 4, 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"),  6.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 15.0);
}

TEST_F(MathReductionsBatchTest, Cumprod)
{
    eval("y = cumprod([1, 2, 3, 4, 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"),   1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"),   6.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 120.0);
}

TEST_F(MathReductionsBatchTest, Diff)
{
    eval("y = diff([1, 4, 9, 16, 25]);");
    EXPECT_DOUBLE_EQ(evalScalar("numel(y)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 9.0);
}

TEST_F(MathReductionsBatchTest, DiagFromVector)
{
    // diag(vec) → diagonal matrix
    eval("D = diag([1, 2, 3]);");
    EXPECT_DOUBLE_EQ(evalScalar("D(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(2,2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(3,3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("D(1,2)"), 0.0);
}

TEST_F(MathReductionsBatchTest, ProdSum)
{
    EXPECT_DOUBLE_EQ(evalScalar("sum([1, 2, 3, 4, 5])"),  15.0);
    EXPECT_DOUBLE_EQ(evalScalar("prod([1, 2, 3, 4, 5])"), 120.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum([2.5, 3.5])"),       6.0);
    EXPECT_DOUBLE_EQ(evalScalar("prod([0.5, 4])"),        2.0);
}
