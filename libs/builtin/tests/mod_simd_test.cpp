// libs/builtin/tests/mod_simd_test.cpp
//
// Regression guard for the SIMD mod() fast path (real-double arrays:
// same-shape, array-by-scalar, scalar-by-array). The kernel is the same
// formula as the scalar reference, r = (b!=0) ? a - floor(a/b)*b : a, so
// results are bit-identical and MATLAB-equal. Runs on both engines.
// (Scalar mod(s,s) goes through the VM/TW fast path, tested elsewhere.)

#include "dual_engine_fixture.hpp"

using namespace m_test;

class ModSimdTest : public DualEngineTest {};

TEST_P(ModSimdTest, SameShapeVector)
{
    eval("v = mod([5 -5 7 -7 8 -8 9 -9 100 -100], "
         "        [3  3 3  3 3  3 3  3   3    3]);"); // 10 elems: SIMD body + tail
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"),  2.0); // mod(5,3)
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"),  1.0); // mod(-5,3)
    EXPECT_DOUBLE_EQ(evalScalar("v(5)"),  2.0); // mod(8,3), SIMD lane
    EXPECT_DOUBLE_EQ(evalScalar("v(9)"),  1.0); // mod(100,3), tail
    EXPECT_DOUBLE_EQ(evalScalar("v(10)"), 2.0); // mod(-100,3), tail
}

TEST_P(ModSimdTest, ArrayByScalar)
{
    eval("w = mod(1:10, 3);"); // VS, SIMD body + tail
    EXPECT_DOUBLE_EQ(evalScalar("w(1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(3)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("w(10)"), 1.0);
    eval("f = mod([5.5 -5.5 7.25 -7.25], 3);"); // fractional, exact in double
    EXPECT_DOUBLE_EQ(evalScalar("f(1)"), 2.5);
    EXPECT_DOUBLE_EQ(evalScalar("f(2)"), 0.5);
    EXPECT_DOUBLE_EQ(evalScalar("f(3)"), 1.25);
}

TEST_P(ModSimdTest, ScalarByArray)
{
    eval("s = mod(10, [3 4 7 6]);"); // SV
    EXPECT_DOUBLE_EQ(evalScalar("s(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("s(4)"), 4.0);
}

TEST_P(ModSimdTest, SignFollowsDivisor)
{
    eval("p = mod([5 -5 5 -5], [3 3 -3 -3]);"); // 4 elems = one SIMD vector
    EXPECT_DOUBLE_EQ(evalScalar("p(1)"),  2.0);  // mod(5,3)
    EXPECT_DOUBLE_EQ(evalScalar("p(2)"),  1.0);  // mod(-5,3)
    EXPECT_DOUBLE_EQ(evalScalar("p(3)"), -1.0);  // mod(5,-3)
    EXPECT_DOUBLE_EQ(evalScalar("p(4)"), -2.0);  // mod(-5,-3)
}

TEST_P(ModSimdTest, ZeroDivisorReturnsDividend)
{
    eval("z = mod([5 6 7 8], [0 3 0 4]);"); // b==0 -> a
    EXPECT_DOUBLE_EQ(evalScalar("z(1)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(3)"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(4)"), 0.0);
}

INSTANTIATE_DUAL(ModSimdTest);
