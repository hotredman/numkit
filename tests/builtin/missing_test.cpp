// toolboxes/builtin/tests/missing_test.cpp
//
// Regression guard for anymissing / ismissing.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MissingTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── anymissing ───────────────────────────────────────────────────

TEST_F(MissingTest, AnyMissingDoubleNoNaN)
{
    EXPECT_EQ(static_cast<int>(evalScalar("double(anymissing([1 2 3]))")), 0);
}

TEST_F(MissingTest, AnyMissingDoubleWithNaN)
{
    EXPECT_EQ(static_cast<int>(evalScalar("double(anymissing([1 NaN 3]))")), 1);
}

TEST_F(MissingTest, AnyMissingMatrix)
{
    EXPECT_EQ(static_cast<int>(evalScalar("double(anymissing([1 NaN; 2 3]))")), 1);
}

TEST_F(MissingTest, AnyMissingUint8)
{
    EXPECT_EQ(static_cast<int>(evalScalar("double(anymissing(uint8([1 2 3])))")), 0);
}

TEST_F(MissingTest, AnyMissingEmpty)
{
    EXPECT_EQ(static_cast<int>(evalScalar("double(anymissing([]))")), 0);
}

TEST_F(MissingTest, AnyMissingLogical)
{
    EXPECT_EQ(static_cast<int>(evalScalar("double(anymissing(logical([1 0 1])))")), 0);
}

TEST_F(MissingTest, AnyMissingSingleNaN)
{
    EXPECT_EQ(static_cast<int>(evalScalar("double(anymissing(single([1 NaN 3])))")), 1);
}

// ── ismissing without indicator ──────────────────────────────────

TEST_F(MissingTest, IsMissingDoubleNaNs)
{
    eval("M = ismissing([1 NaN 3 NaN]);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(3))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(4))")), 1);
}

TEST_F(MissingTest, IsMissingMatrix)
{
    eval("M = ismissing([1 NaN; 2 3]);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(1,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(1,2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(2,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(2,2))")), 0);
}

TEST_F(MissingTest, IsMissingUint8NoMissing)
{
    eval("M = ismissing(uint8([1 2 3]));");
    EXPECT_EQ(eval("class(M)").toString(), "logical");
    EXPECT_EQ(static_cast<int>(evalScalar("double(sum(double(M)))")), 0);
}

// ── ismissing with indicator ─────────────────────────────────────

TEST_F(MissingTest, IsMissingScalarIndicator)
{
    eval("M = ismissing([1 2 -99 4], -99);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(3))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(4))")), 0);
}

TEST_F(MissingTest, IsMissingVectorIndicator)
{
    eval("M = ismissing([1 2 -99 -88 4], [-99 -88]);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(3))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(4))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(5))")), 0);
}

TEST_F(MissingTest, IsMissingIndicatorOverridesNaN)
{
    // Per MATLAB: when indicator is given, NaN is NOT auto-missing.
    eval("M = ismissing([1 2 -99 4 NaN], -99);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(3))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(5))")), 0);
}

TEST_F(MissingTest, IsMissingNaNInIndicator)
{
    // NaN entry in indicator does match NaN entries in x.
    eval("M = ismissing([1 NaN 3], NaN);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(M(3))")), 0);
}

// ── errors ───────────────────────────────────────────────────────

TEST_F(MissingTest, AnyMissingNoArgThrows)
{
    EXPECT_THROW(eval("anymissing();"), std::exception);
}

TEST_F(MissingTest, IsMissingNoArgThrows)
{
    EXPECT_THROW(eval("ismissing();"), std::exception);
}
