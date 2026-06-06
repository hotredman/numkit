// libs/builtin/tests/standardize_missing_test.cpp
//
// Regression guard for standardizeMissing.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class StandardizeMissingTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── double inputs replace matched values with NaN ────────────────

TEST_F(StandardizeMissingTest, DoubleScalarIndicator)
{
    eval("B = standardizeMissing([1 2 -99 4], -99);");
    EXPECT_EQ(eval("class(B)").toString(), "double");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")),  1);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2)")),  2);
    EXPECT_TRUE(std::isnan(evalScalar("B(3)")));
    EXPECT_EQ(static_cast<int>(evalScalar("B(4)")),  4);
}

TEST_F(StandardizeMissingTest, DoubleVectorIndicator)
{
    eval("B = standardizeMissing([1 -99 -88 4], [-99 -88]);");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")),  1);
    EXPECT_TRUE(std::isnan(evalScalar("B(2)")));
    EXPECT_TRUE(std::isnan(evalScalar("B(3)")));
    EXPECT_EQ(static_cast<int>(evalScalar("B(4)")),  4);
}

TEST_F(StandardizeMissingTest, MatrixInput)
{
    eval("B = standardizeMissing([1 -99; -99 4], -99);");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1,1)")), 1);
    EXPECT_TRUE(std::isnan(evalScalar("B(1,2)")));
    EXPECT_TRUE(std::isnan(evalScalar("B(2,1)")));
    EXPECT_EQ(static_cast<int>(evalScalar("B(2,2)")), 4);
}

// ── NaN in indicator does NOT match NaN in x (NaN != NaN) ───────

TEST_F(StandardizeMissingTest, NaNInIndicatorNoOp)
{
    eval("B = standardizeMissing([1 2 NaN 4], NaN);");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")),  1);
    EXPECT_EQ(static_cast<int>(evalScalar("B(2)")),  2);
    EXPECT_TRUE(std::isnan(evalScalar("B(3)")));   // unchanged
    EXPECT_EQ(static_cast<int>(evalScalar("B(4)")),  4);
}

TEST_F(StandardizeMissingTest, MixedNaNAndScalarIndicator)
{
    // Only -99 entries are replaced; NaN passes through unchanged.
    eval("B = standardizeMissing([1 NaN -99 4], [NaN -99]);");
    EXPECT_EQ(static_cast<int>(evalScalar("B(1)")),  1);
    EXPECT_TRUE(std::isnan(evalScalar("B(2)")));
    EXPECT_TRUE(std::isnan(evalScalar("B(3)")));
    EXPECT_EQ(static_cast<int>(evalScalar("B(4)")),  4);
}

// ── single preserved ─────────────────────────────────────────────

TEST_F(StandardizeMissingTest, SingleClass)
{
    eval("B = standardizeMissing(single([1.5 2 -99 4]), single(-99));");
    EXPECT_EQ(eval("class(B)").toString(), "single");
    EXPECT_NEAR(evalScalar("double(B(1))"),  1.5, 1e-6);
    EXPECT_TRUE(std::isnan(evalScalar("double(B(3))")));
}

// ── integer / logical types pass through (no missing concept) ────

TEST_F(StandardizeMissingTest, Uint8NoOp)
{
    eval("B = standardizeMissing(uint8([1 2 3 5]), 3);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2))")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3))")), 3);  // NOT replaced
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(4))")), 5);
}

TEST_F(StandardizeMissingTest, Int16NoOp)
{
    eval("B = standardizeMissing(int16([1 -99 4]), int16(-99));");
    EXPECT_EQ(eval("class(B)").toString(), "int16");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2))")), -99);  // NOT replaced
}

TEST_F(StandardizeMissingTest, LogicalNoOp)
{
    eval("B = standardizeMissing(logical([1 0 1 0]), false);");
    EXPECT_EQ(eval("class(B)").toString(), "logical");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2))")), 0);
}

// ── errors ───────────────────────────────────────────────────────

TEST_F(StandardizeMissingTest, NoArgsThrows)
{
    EXPECT_THROW(eval("standardizeMissing();"), std::exception);
}

TEST_F(StandardizeMissingTest, NoIndicatorThrows)
{
    EXPECT_THROW(eval("standardizeMissing([1 2 3]);"), std::exception);
}
