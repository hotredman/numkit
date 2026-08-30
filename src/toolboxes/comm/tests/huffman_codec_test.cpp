// toolboxes/comm/tests/huffman_codec_test.cpp
//
// Regression guard for huffmanenco / huffmandeco — Huffman encoder
// and decoder round-trip via a dict produced by huffmandict.
//
// Bit codes are non-unique (Huffman tie-breaking), so we focus on
// round-trip identity rather than literal bit patterns.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class HuffmanCodecTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(HuffmanCodecTest, RoundTripFiveSymbol)
{
    eval("[d, ~] = huffmandict([1 2 3 4 5], [0.4 0.2 0.2 0.1 0.1]);"
         "sig = [1 1 2 3 4 5 1];"
         "enc = huffmanenco(sig, d);"
         "dec = huffmandeco(enc, d);"
         "match = isequal(sig(:), dec(:));");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(HuffmanCodecTest, RoundTripTwoSymbol)
{
    eval("[d, ~] = huffmandict([0 1], [0.7 0.3]);"
         "sig = [0 1 1 0 1 0 0 1 1 1];"
         "enc = huffmanenco(sig, d);"
         "dec = huffmandeco(enc, d);"
         "match = isequal(sig(:), dec(:));");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(HuffmanCodecTest, EncodedBitsAreBinary)
{
    eval("[d, ~] = huffmandict([1 2 3 4 5], [0.4 0.2 0.2 0.1 0.1]);"
         "enc = huffmanenco([1 2 3 4 5 1 2 3 4 5], d);"
         "ok = all(enc == 0 | enc == 1);");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
}

TEST_F(HuffmanCodecTest, EncodeRowOrientationPreserved)
{
    eval("[d, ~] = huffmandict([1 2 3], [0.5 0.3 0.2]);"
         "enc = huffmanenco([1 2 3 1 2 3], d);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(enc, 1)")), 1);
}

TEST_F(HuffmanCodecTest, EncodeColumnOrientationPreserved)
{
    eval("[d, ~] = huffmandict([1 2 3], [0.5 0.3 0.2]);"
         "enc = huffmanenco([1; 2; 3; 1; 2; 3], d);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(enc, 2)")), 1);
}

TEST_F(HuffmanCodecTest, EncodedLengthMatchesAvglenWithProbs)
{
    // Long signal: enc length should equal sum of code lengths used.
    eval("p = [0.4 0.2 0.2 0.1 0.1];"
         "[d, av] = huffmandict([1 2 3 4 5], p);"
         "% deterministic signal hitting each symbol with relative freq p\n"
         "sig = [repmat(1, 1, 400), repmat(2, 1, 200), repmat(3, 1, 200), "
         "       repmat(4, 1, 100), repmat(5, 1, 100)];"
         "enc = huffmanenco(sig, d);"
         "avg_observed = length(enc) / length(sig);");
    EXPECT_NEAR(evalScalar("avg_observed"), 2.2, 1e-12);
}

TEST_F(HuffmanCodecTest, RejectsUnknownSymbol)
{
    bool threw = false;
    try {
        eval("[d, ~] = huffmandict([1 2 3], [0.5 0.3 0.2]);"
             "huffmanenco([1 2 99], d);");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(HuffmanCodecTest, RejectsBadDecodeBits)
{
    bool threw = false;
    try {
        eval("[d, ~] = huffmandict([1 2 3], [0.5 0.3 0.2]);"
             "huffmandeco([0 1 2], d);");   // 2 is not a bit
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(HuffmanCodecTest, RejectsTrailingPartialCode)
{
    // Truncate a valid encoding by 1 bit -> can't complete final code.
    bool threw = false;
    try {
        eval("[d, ~] = huffmandict([1 2 3 4 5], [0.4 0.2 0.2 0.1 0.1]);"
             "enc = huffmanenco([1 2 3 4 5], d);"
             "huffmandeco(enc(1:end-1), d);");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
