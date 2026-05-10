// libs/comm/tests/arith_test.cpp
//
// Regression guard for arithenco / arithdeco — arithmetic coding pair.
// Bit-equal with MATLAB R2025b on encoded bit string and round-trip.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ArithTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ArithTest, KnownEncodedBits)
{
    // MATLAB: arithenco([1 2 1 3 4 1 1 2], [10 5 3 2])
    //   = [0 1 0 0 1 1 1 0 0 0 1 0 1 0 0 0 0 0 0 0 0]   (length 21)
    eval("c = arithenco([1 2 1 3 4 1 1 2], [10 5 3 2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("length(c)")), 21);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(4)"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(5)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(13)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(21)"), 0.0);
}

TEST_F(ArithTest, RoundTripSmall)
{
    eval("seq = [1 2 1 3 4 1 1 2];"
         "code = arithenco(seq, [10 5 3 2]);"
         "dec = arithdeco(code, [10 5 3 2], length(seq));"
         "match = isequal(seq, dec);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(ArithTest, RoundTripLarger)
{
    eval("seq = [1 1 1 2 2 2 3 3 4 4 4 4 1 2 3 4 1 2 3 4 1 1 1 1 2];"
         "code = arithenco(seq, [10 5 3 2]);"
         "dec = arithdeco(code, [10 5 3 2], length(seq));"
         "match = isequal(seq, dec);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(ArithTest, EncodedBitsAreBinary)
{
    eval("c = arithenco([1 2 3 4], [10 5 3 2]);"
         "ok = all(c == 0 | c == 1);");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
}

TEST_F(ArithTest, EncodeRowOrientationPreserved)
{
    eval("c = arithenco([1 2 3 4 1 2], [10 5 3 2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 1)")), 1);
}

TEST_F(ArithTest, EncodeColumnOrientationPreserved)
{
    eval("c = arithenco([1; 2; 3; 4; 1; 2], [10 5 3 2]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(c, 2)")), 1);
}

TEST_F(ArithTest, RoundTripUniformDistribution)
{
    // Uniform counts -> stress E1/E2/E3 rescaling.
    eval("seq = [1 2 3 4 5 6 7 8 1 2 3 4 5 6 7 8];"
         "code = arithenco(seq, ones(1, 8));"
         "dec = arithdeco(code, ones(1, 8), length(seq));"
         "match = isequal(seq, dec);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(ArithTest, RejectsBadSymbol)
{
    bool threw = false;
    try {
        eval("arithenco([1 2 5], [10 5 3 2]);");  // 5 > length(counts)=4
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(ArithTest, RejectsNonPositiveCounts)
{
    bool threw = false;
    try {
        eval("arithenco([1 2], [10 0]);");  // 0 not positive integer
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(ArithTest, RoundTripSingleSymbol)
{
    // Edge: K=1 (only one possible symbol). Total range collapses; codec
    // should still round-trip even if encoded length is degenerate.
    eval("code = arithenco([1 1 1 1], [5]);"
         "dec = arithdeco(code, [5], 4);"
         "match = isequal([1 1 1 1], dec);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}
