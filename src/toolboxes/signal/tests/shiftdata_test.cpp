// toolboxes/signal/tests/shiftdata_test.cpp
//
// Regression guard for shiftdata + unshiftdata (Phase 4.4).
// Bit-equal MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ShiftDataTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("A = [1 2 3; 4 5 6; 7 8 9];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ShiftDataTest, ExplicitDimPermutes)
{
    // shiftdata(A, 2) → permute with [2, 1] = transpose for 2D.
    eval("[xs, perm, nsh] = shiftdata(A, 2);");
    EXPECT_DOUBLE_EQ(evalScalar("xs(1, 2)"), 4.0);  // xs is A'
    EXPECT_DOUBLE_EQ(evalScalar("perm(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("perm(2)"), 1.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(nsh)")), 0);
}

TEST_F(ShiftDataTest, AutoPathDropsLeadingSingleton)
{
    // shiftdata(1:5, []) on row vec drops leading singleton dim → col vec.
    eval("x = 1:5; [xs, perm, nsh] = shiftdata(x, []);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(xs, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(xs, 2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(perm)")), 0);
    EXPECT_DOUBLE_EQ(evalScalar("nsh"), 1.0);
}

TEST_F(ShiftDataTest, RoundtripExplicit)
{
    eval("[xs, perm, nsh] = shiftdata(A, 2);"
         "y = unshiftdata(xs, perm, nsh);"
         "match = double(isequal(y, A));");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(ShiftDataTest, RoundtripAuto)
{
    eval("x = 1:5; [xs, perm, nsh] = shiftdata(x, []);"
         "y = unshiftdata(xs, perm, nsh);"
         "match = double(isequal(y, x));");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}
