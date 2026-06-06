// libs/comm/tests/encode_test.cpp
//
// Regression guard for encode (Error Correction Codes block encoder).
// Reference codewords from the MATLAB R2025b probe.

#include <numkit/comm/coding/blockcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class EncodeTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Hamming(7,4) single word, binary.
TEST_F(EncodeTest, HammingBinaryWord)
{
    eval("c = encode([1 0 1 1], 7, 4, 'hamming/binary');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(c, [1 0 0 1 0 1 1])"), 1.0);
}

// Two words concatenated.
TEST_F(EncodeTest, HammingBinaryTwoWords)
{
    eval("c = encode([1 0 1 1 0 1 0 0], 7, 4, 'hamming/binary');");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 14);
    EXPECT_DOUBLE_EQ(evalScalar(
        "isequal(c, [1 0 0 1 0 1 1 0 1 1 0 1 0 0])"), 1.0);
}

// Cyclic (7,4) — generator polynomial defaults to cyclpoly(7,4).
TEST_F(EncodeTest, CyclicBinary)
{
    eval("c = encode([1 0 1 1], 7, 4, 'cyclic/binary');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(c, [0 0 0 1 0 1 1])"), 1.0);
}

// Linear method with an explicit generator matrix (Hamming G).
TEST_F(EncodeTest, LinearWithGenerator)
{
    eval("[h,g] = hammgen(3); c = encode([1 0 1 1], 7, 4, 'linear/binary', g);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(c, [1 0 0 1 0 1 1])"), 1.0);
}

// Decimal format: integer in, integer out.
TEST_F(EncodeTest, Decimal)
{
    EXPECT_DOUBLE_EQ(evalScalar("encode(11, 7, 4, 'hamming/decimal')"), 88.0);
}

// Zero-padding: 5-bit message, k=4 -> 3 bits added (2 words).
TEST_F(EncodeTest, AddedPadding)
{
    eval("[c, added] = encode([1 0 1 1 0], 7, 4, 'hamming/binary');");
    EXPECT_DOUBLE_EQ(evalScalar("added"), 3.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(c)")), 14);
}

// Direct C++ API.
TEST_F(EncodeTest, PublicApi)
{
    eval("msg = [1 0 1 1];");
    comm::EncodeResult r = comm::encode(*engine.getVariable("msg"), 7, 4,
                                        "hamming/binary", Value::Empty,
                                        engine.resource());
    EXPECT_EQ(r.added, 0);
    ASSERT_EQ(r.code.numel(), 7u);
    // codeword [1 0 0 1 0 1 1]
    EXPECT_DOUBLE_EQ(r.code.elemAsDouble(0), 1.0);
    EXPECT_DOUBLE_EQ(r.code.elemAsDouble(2), 0.0);
    EXPECT_DOUBLE_EQ(r.code.elemAsDouble(6), 1.0);
}
