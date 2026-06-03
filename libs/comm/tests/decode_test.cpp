// libs/comm/tests/decode_test.cpp
//
// Regression guard for decode (Error Correction Codes syndrome decoder).
// Reference values from the MATLAB R2025b probe.

#include <numkit/comm/coding/blockcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DecodeTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Clean codeword -> original message, zero errors.
TEST_F(DecodeTest, HammingClean)
{
    eval("cw = encode([1 0 1 1], 7, 4, 'hamming/binary');"
         " [m, e] = decode(cw, 7, 4, 'hamming/binary');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(m, [1 0 1 1])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("e"), 0.0);
}

// Single-bit error is corrected; err reports one correction.
TEST_F(DecodeTest, HammingOneError)
{
    eval("cw = encode([1 0 1 1], 7, 4, 'hamming/binary');"
         " rc = cw; rc(2) = 1 - rc(2);"
         " [m, e] = decode(rc, 7, 4, 'hamming/binary');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(m, [1 0 1 1])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("e"), 1.0);
}

// Every single-bit error position is correctable for the (7,4) Hamming code.
TEST_F(DecodeTest, HammingAllSingleErrors)
{
    eval("cw = encode([1 0 1 1], 7, 4, 'hamming/binary'); ok = 1;"
         " for p = 1:7, rc = cw; rc(p) = 1 - rc(p);"
         "   m = decode(rc, 7, 4, 'hamming/binary');"
         "   ok = ok && isequal(m, [1 0 1 1]); end");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
}

// Decimal round-trip.
TEST_F(DecodeTest, Decimal)
{
    EXPECT_DOUBLE_EQ(evalScalar("decode(88, 7, 4, 'hamming/decimal')"), 11.0);
}

// Cyclic round-trip with a corrected error.
TEST_F(DecodeTest, CyclicRoundTrip)
{
    eval("cw = encode([1 0 1 1], 7, 4, 'cyclic/binary');"
         " rc = cw; rc(4) = 1 - rc(4);"
         " m = decode(rc, 7, 4, 'cyclic/binary');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(m, [1 0 1 1])"), 1.0);
}

// Direct C++ API.
TEST_F(DecodeTest, PublicApi)
{
    eval("cw = encode([1 0 1 1], 7, 4, 'hamming/binary'); cw(5) = 1 - cw(5);");
    comm::DecodeResult r = comm::decode(*engine.getVariable("cw"), 7, 4,
                                        "hamming/binary", Value::Empty,
                                        engine.resource());
    ASSERT_EQ(r.msg.numel(), 4u);
    EXPECT_DOUBLE_EQ(r.msg.elemAsDouble(0), 1.0);
    EXPECT_DOUBLE_EQ(r.msg.elemAsDouble(1), 0.0);
    EXPECT_DOUBLE_EQ(r.msg.elemAsDouble(2), 1.0);
    EXPECT_DOUBLE_EQ(r.msg.elemAsDouble(3), 1.0);
    EXPECT_DOUBLE_EQ(r.err.elemAsDouble(0), 1.0);  // one error corrected
}
