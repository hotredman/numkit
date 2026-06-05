// libs/builtin/tests/bitwise_batch_test.cpp
// bitwise family — 7 functions:
//   bitand / bitor / bitxor / bitshift / bitcmp / bitset / bitget
// All  — bit-identical MATLAB R2025b
// on probed inputs (parity tol=0, exact integer match).
// Side observation noted in this commit's review: numkit's bitset
// rejects uint32 first arg ("Not a double array") and uint32 array
// concat fails — separate adapter-level gaps, NOT in scope here.
// Probed only via inputs that work; full type matrix is a follow-up.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class BitwiseBatchTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(BitwiseBatchTest, BitAnd)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitand(uint32(0xF0), uint32(0x0F))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitand(uint32(0xFF), uint32(0x33))"), 0x33);
    EXPECT_DOUBLE_EQ(evalScalar("bitand(uint32(0x55), uint32(0xAA))"), 0.0);
}

TEST_F(BitwiseBatchTest, BitOr)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitor(uint32(0xF0), uint32(0x0F))"), 0xFF);
    EXPECT_DOUBLE_EQ(evalScalar("bitor(uint32(0x55), uint32(0xAA))"), 0xFF);
}

TEST_F(BitwiseBatchTest, BitXor)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitxor(uint32(0xF0), uint32(0xFF))"), 0x0F);
    EXPECT_DOUBLE_EQ(evalScalar("bitxor(uint32(0x55), uint32(0xAA))"), 0xFF);
    EXPECT_DOUBLE_EQ(evalScalar("bitxor(uint32(0x55), uint32(0x55))"), 0.0);
}

TEST_F(BitwiseBatchTest, BitShift)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitshift(uint32(1), 3)"),  8.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitshift(uint32(8), -1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitshift(uint32(16), -2)"), 4.0);
}

TEST_F(BitwiseBatchTest, BitCmp)
{
    // bitcmp(uint8(0)) = 0xFF, bitcmp(uint8(0xF0)) = 0x0F
    EXPECT_DOUBLE_EQ(evalScalar("double(bitcmp(uint8(0)))"),    0xFF);
    EXPECT_DOUBLE_EQ(evalScalar("double(bitcmp(uint8(0xF0)))"), 0x0F);
    EXPECT_DOUBLE_EQ(evalScalar("double(bitcmp(uint8(0x55)))"), 0xAA);
}

TEST_F(BitwiseBatchTest, BitSet)
{
    // bitset(0, 3) sets bit 3 (value 4)
    EXPECT_DOUBLE_EQ(evalScalar("bitset(0, 3)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitset(1, 3)"), 5.0);  // 1 | 4 = 5
    EXPECT_DOUBLE_EQ(evalScalar("bitset(7, 4)"), 15.0); // 7 | 8 = 15
}

TEST_F(BitwiseBatchTest, BitGet)
{
    EXPECT_DOUBLE_EQ(evalScalar("bitget(uint32(1),  1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitget(uint32(2),  2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitget(uint32(4),  3)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitget(uint32(4),  1)"), 0.0);
}

TEST_F(BitwiseBatchTest, RoundTripIdentities)
{
    // bitand + bitor + bitxor: a^a = 0, a|a = a, a&a = a
    EXPECT_DOUBLE_EQ(evalScalar("bitxor(uint32(0xCAFE), uint32(0xCAFE))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("bitor(uint32(0xCAFE),  uint32(0xCAFE))"), 0xCAFE);
    EXPECT_DOUBLE_EQ(evalScalar("bitand(uint32(0xCAFE), uint32(0xCAFE))"), 0xCAFE);
}
