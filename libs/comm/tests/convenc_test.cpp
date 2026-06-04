// libs/comm/tests/convenc_test.cpp
//
// Regression guard for convenc (Error Correction Codes). Reference values
// from a direct MATLAB R2025b probe (the parity harness can't capture
// convenc output: MATLAB -batch crashes on shutdown, 0xC0000005 in
// libmwcustom_holes_factory.dll, eating the spec's stdout — a MATLAB
// environment defect, not a numkit issue; convenc is verified correct here
// and via the direct probe).

#include <numkit/comm/coding/convcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ConvencTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Rate 1/2, K=3 [6 7]: convenc([1 1 0 1 1 0 0]) = [1 1 0 0 1 0 1 0 0 0 1 0 0 1].
TEST_F(ConvencTest, Rate12K3)
{
    eval("t = poly2trellis(3, [6 7]); code = convenc([1 1 0 1 1 0 0], t);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(code)")), 14);
    EXPECT_DOUBLE_EQ(evalScalar("code(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("code(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("code(3)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("code(4)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("code(5)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("code(14)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("sum(code)"), 6.0);
}

// Short message: convenc([1 0 0]) = [1 1 1 1 0 1].
TEST_F(ConvencTest, ShortMsg)
{
    eval("t = poly2trellis(3, [6 7]); c = convenc([1 0 0], t);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 6);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(5)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(6)"), 1.0);
}

// Column input -> column output, n*L bits.
TEST_F(ConvencTest, ColumnOrientation)
{
    eval("t = poly2trellis(3, [6 7]); c = convenc([1;0;1;1], t);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(c,1)")), 8);
    EXPECT_EQ(static_cast<int>(evalScalar("size(c,2)")), 1);
}

// Rate 1/3 K=4: 4 message bits -> 12 output bits.
TEST_F(ConvencTest, Rate13Length)
{
    eval("t = poly2trellis(4, [13 15 17]); c = convenc([1 0 1 1], t);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 12);
}

// Direct C++ API: convenc([1 0 0 1 0 1 1 0], t) -> numel 16, sum 11.
TEST_F(ConvencTest, PublicApi)
{
    eval("t = poly2trellis(3, [6 7]); m = [1 0 0 1 0 1 1 0];");
    Value code = comm::convenc(*engine.getVariable("m"),
                               *engine.getVariable("t"), engine.resource());
    ASSERT_EQ(code.numel(), 16u);
    double s = 0.0;
    for (size_t i = 0; i < code.numel(); ++i) s += code.doubleData()[i];
    EXPECT_DOUBLE_EQ(s, 11.0);
}
