// libs/comm/tests/vitdec_test.cpp
//
// Regression guard for vitdec (Error Correction Codes). Verified vs MATLAB
// R2025b via direct probe (roundtrip_ok = 1). The parity harness can't run
// vitdec for the same reason as convenc (MATLAB -batch shutdown crash on
// Communications Toolbox scripts) — so this gtest is the primary guard.

#include <numkit/comm/coding/convcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class VitdecTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Lossless round-trip: convenc -> vitdec recovers the message exactly.
TEST_F(VitdecTest, RoundTrip)
{
    eval("t = poly2trellis(3, [6 7]); msg = [1 1 0 1 1 0 0 1 0 1 1 0]; "
         "code = convenc(msg, t); dec = vitdec(code, t, 12, 'trunc', 'hard');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(dec, msg)"), 1.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(dec)")), 12);
}

// A single bit error is corrected by the Viterbi decoder.
TEST_F(VitdecTest, CorrectsSingleError)
{
    eval("t = poly2trellis(3, [6 7]); msg = [1 1 0 1 1 0 0 1 0 1 1 0]; "
         "code = convenc(msg, t); code(3) = 1 - code(3); "
         "dec = vitdec(code, t, 12, 'trunc', 'hard');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(dec, msg)"), 1.0);
}

// Standard K=7 code round-trips too.
TEST_F(VitdecTest, K7RoundTrip)
{
    eval("t = poly2trellis(7, [171 133]); msg = [1 0 1 1 0 0 1 0 1 1 1 0 0 1]; "
         "dec = vitdec(convenc(msg, t), t, 30, 'trunc', 'hard');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(dec, msg)"), 1.0);
}

TEST_F(VitdecTest, Errors)
{
    eval("t = poly2trellis(3, [6 7]);");
    EXPECT_ANY_THROW(eval("vitdec([1 0 1 0], t, 5, 'trunc', 'soft');")); // soft deferred
    EXPECT_ANY_THROW(eval("vitdec([1 0 1 0], t, 5, 'cont', 'hard');"));  // cont deferred
    EXPECT_ANY_THROW(eval("vitdec([1 0 1], t, 5, 'trunc', 'hard');"));   // len % n != 0
}

// Direct C++ API.
TEST_F(VitdecTest, PublicApi)
{
    eval("t = poly2trellis(3, [6 7]); msg = [1 0 1 1]; code = convenc(msg, t);");
    Value dec = comm::vitdec(*engine.getVariable("code"),
                             *engine.getVariable("t"), 8, "trunc", "hard",
                             engine.resource());
    ASSERT_EQ(dec.numel(), 4u);
    EXPECT_DOUBLE_EQ(dec.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(dec.doubleData()[1], 0.0);
    EXPECT_DOUBLE_EQ(dec.doubleData()[2], 1.0);
    EXPECT_DOUBLE_EQ(dec.doubleData()[3], 1.0);
}
