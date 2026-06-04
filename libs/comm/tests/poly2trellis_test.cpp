// libs/comm/tests/poly2trellis_test.cpp
//
// Regression guard for poly2trellis (Error Correction Codes). Reference
// values from the MATLAB R2025b probe. (Struct field-then-index in one
// expression is a core gap, so matrix fields are pulled into intermediate
// variables before indexing.)

#include <numkit/comm/coding/convcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Poly2trellisTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Classic rate-1/2, K=3 code, generators [6 7] (octal).
TEST_F(Poly2trellisTest, Rate12K3)
{
    eval("t = poly2trellis(3, [6 7]); ns = t.nextStates; ou = t.outputs;");
    EXPECT_DOUBLE_EQ(evalScalar("t.numInputSymbols"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("t.numOutputSymbols"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("t.numStates"), 4.0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(ns,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(ns,2)")), 2);
    // nextStates = [0 2; 0 2; 1 3; 1 3]
    EXPECT_DOUBLE_EQ(evalScalar("ns(1,2)"), 2.0); // state 0, input 1
    EXPECT_DOUBLE_EQ(evalScalar("ns(3,1)"), 1.0); // state 2, input 0
    EXPECT_DOUBLE_EQ(evalScalar("ns(4,2)"), 3.0); // state 3, input 1
    // outputs = [0 3; 1 2; 3 0; 2 1]
    EXPECT_DOUBLE_EQ(evalScalar("ou(1,2)"), 3.0); // state 0, input 1
    EXPECT_DOUBLE_EQ(evalScalar("ou(2,1)"), 1.0); // state 1, input 0
    EXPECT_DOUBLE_EQ(evalScalar("ou(3,1)"), 3.0); // state 2, input 0
    EXPECT_DOUBLE_EQ(evalScalar("ou(3,2)"), 0.0); // state 2, input 1
}

// Rate 1/3, K=4, generators [13 15 17] (octal).
TEST_F(Poly2trellisTest, Rate13K4)
{
    eval("t = poly2trellis(4, [13 15 17]); ou = t.outputs;");
    EXPECT_DOUBLE_EQ(evalScalar("t.numOutputSymbols"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("t.numStates"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("ou(1,2)"), 7.0); // state 0, input 1 -> 111b
    EXPECT_DOUBLE_EQ(evalScalar("ou(2,1)"), 7.0); // state 1, input 0
}

// Standard K=7 code (e.g. 802.11): 64 states, rate 1/2.
TEST_F(Poly2trellisTest, K7Standard)
{
    eval("t = poly2trellis(7, [171 133]);");
    EXPECT_DOUBLE_EQ(evalScalar("t.numStates"), 64.0);
    EXPECT_DOUBLE_EQ(evalScalar("t.numOutputSymbols"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("t.numInputSymbols"), 2.0);
}

// K=1 degenerate (single state, no memory).
TEST_F(Poly2trellisTest, K1NoMemory)
{
    eval("t = poly2trellis(1, [1 1]); ou = t.outputs;");
    EXPECT_DOUBLE_EQ(evalScalar("t.numStates"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ou(1,1)"), 0.0); // input 0 -> 00
    EXPECT_DOUBLE_EQ(evalScalar("ou(1,2)"), 3.0); // input 1 -> 11
}

TEST_F(Poly2trellisTest, Errors)
{
    // rate k/n (vector ConstraintLength) deferred
    EXPECT_ANY_THROW(eval("poly2trellis([2 2], [5 7; 7 5]);"));
    // octal digit out of range
    EXPECT_ANY_THROW(eval("poly2trellis(3, [6 9]);"));
}

// Direct C++ API — inspect the returned struct map directly.
TEST_F(Poly2trellisTest, PublicApi)
{
    eval("K = 3; g = [6 7];");
    Value t = comm::poly2trellis(*engine.getVariable("K"),
                                 *engine.getVariable("g"), engine.resource());
    ASSERT_TRUE(t.isStruct());
    ASSERT_EQ(t.numel(), 1u);
    const auto &el = t.structArrayElem(0);
    EXPECT_DOUBLE_EQ(el.at("numInputSymbols").toScalar(), 2.0);
    EXPECT_DOUBLE_EQ(el.at("numStates").toScalar(), 4.0);
    const Value &ns = el.at("nextStates");
    EXPECT_EQ(ns.dims().rows(), 4u);
    EXPECT_EQ(ns.dims().cols(), 2u);
    // ns(4,2) 1-based = (row 3, col 1) col-major -> index 1*4 + 3 = 7
    EXPECT_DOUBLE_EQ(ns.doubleData()[1 * 4 + 3], 3.0);
}
