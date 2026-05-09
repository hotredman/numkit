// libs/builtin/tests/type_conv_batch_test.cpp
//
// Audit ТЗ batch closure for type-conversion family — 12 functions:
//   int8 / int16 / int32 / int64
//   uint8 / uint16 / uint32 / uint64
//   double / single / logical / char
//
// All flagged "no major gap detected". Bit-identical MATLAB R2025b
// on probed inputs.
//
// Known sub-gap: numkit's double("string") rejects with error;
// MATLAB returns NaN, Octave returns ASCII codes. Documented in
// audit/closed/builtin/double.md — only numeric→double paths
// pinned here.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class TypeConvBatchTest : public ::testing::Test
{
public:
    Engine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(TypeConvBatchTest, IntTypes)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(int8(127))"),         127.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(int8(-128))"),       -128.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(int16(32767))"),      32767.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(int32(2147483647))"), 2147483647.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(int64(100))"),        100.0);
}

TEST_F(TypeConvBatchTest, UintTypes)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(uint8(255))"),         255.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(uint8(0))"),           0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(uint16(65535))"),     65535.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(uint32(4294967295))"), 4294967295.0);
}

TEST_F(TypeConvBatchTest, IntSaturation)
{
    // MATLAB saturates on overflow (NOT wrap)
    EXPECT_DOUBLE_EQ(evalScalar("double(int8(200))"),  127.0);   // clamped to MAX
    EXPECT_DOUBLE_EQ(evalScalar("double(int8(-200))"), -128.0);  // clamped to MIN
    EXPECT_DOUBLE_EQ(evalScalar("double(uint8(300))"), 255.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(uint8(-5))"),  0.0);
}

TEST_F(TypeConvBatchTest, DoubleAndSingle)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(int8(50))"), 50.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(true)"),     1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(3.14)"),     3.14);
    EXPECT_NEAR(evalScalar("double(single(3.14))"),  3.14, 1e-6);
}

TEST_F(TypeConvBatchTest, Logical)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(logical(0))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(logical(1))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(logical(5))"), 1.0);   // any non-zero → true
}

TEST_F(TypeConvBatchTest, Char)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(char(65))"), 65.0);  // 'A'
    EXPECT_DOUBLE_EQ(evalScalar("double(char(97))"), 97.0);  // 'a'
}
