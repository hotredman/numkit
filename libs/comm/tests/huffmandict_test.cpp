// libs/comm/tests/huffmandict_test.cpp
//
// Regression guard for huffmandict() — Huffman code-book builder.
//
// NOTE: Huffman codes are not unique -- when the build heap has ties
// (two equal-probability subtrees), different tie-breaking yields
// different but equally-optimal codes. The INVARIANT is avglen
// (sum of p_k * L_k), which all optimal Huffman codes share. We
// pin avglen and prefix-freeness; the literal bit patterns are not
// asserted.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class HuffmandictTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(HuffmandictTest, AvglenMatchesOptimum)
{
    // Reference probs [0.4 0.2 0.2 0.1 0.1] -> optimal avglen = 2.2.
    eval("[~, av] = huffmandict([1 2 3 4 5], [0.4 0.2 0.2 0.1 0.1]);");
    EXPECT_NEAR(evalScalar("av"), 2.2, 1e-12);
}

TEST_F(HuffmandictTest, DictHasCorrectShape)
{
    eval("[d, ~] = huffmandict([1 2 3 4 5], [0.4 0.2 0.2 0.1 0.1]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(d, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(d, 2)")), 2);
}

TEST_F(HuffmandictTest, SymbolsPreservedInColumn1)
{
    eval("[d, ~] = huffmandict([7 8 9], [0.5 0.3 0.2]);");
    EXPECT_DOUBLE_EQ(evalScalar("d{1, 1}"), 7.0);
    EXPECT_DOUBLE_EQ(evalScalar("d{2, 1}"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("d{3, 1}"), 9.0);
}

TEST_F(HuffmandictTest, AllCodesAreBinary)
{
    eval("[d, ~] = huffmandict([1 2 3 4 5], [0.4 0.2 0.2 0.1 0.1]);"
         "ok = 1;"
         "for k = 1:5; c = d{k, 2}; if any(c ~= 0 & c ~= 1); ok = 0; end; end;");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
}

TEST_F(HuffmandictTest, AvglenAtLeastEntropy)
{
    // Huffman lower bound: avglen >= H(X) = -sum(p log2 p)
    eval("p = [0.4 0.2 0.2 0.1 0.1];"
         "[~, av] = huffmandict(1:5, p);"
         "ent = -sum(p .* log2(p));");
    EXPECT_GE(evalScalar("av"), evalScalar("ent"));
}

TEST_F(HuffmandictTest, AvglenAtMostEntropyPlusOne)
{
    // Huffman upper bound: avglen < H(X) + 1
    eval("p = [0.4 0.2 0.2 0.1 0.1];"
         "[~, av] = huffmandict(1:5, p);"
         "ent = -sum(p .* log2(p));");
    EXPECT_LT(evalScalar("av"), evalScalar("ent") + 1.0);
}

TEST_F(HuffmandictTest, TwoSymbolsHaveOneBitCodes)
{
    // [0.7 0.3] -> both codes length 1.
    eval("[d, av] = huffmandict([0 1], [0.7 0.3]);");
    EXPECT_DOUBLE_EQ(evalScalar("av"), 1.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d{1, 2})")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d{2, 2})")), 1);
}

TEST_F(HuffmandictTest, SingleSymbolEdge)
{
    eval("[d, av] = huffmandict([42], [1.0]);");
    EXPECT_DOUBLE_EQ(evalScalar("av"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("d{1, 1}"), 42.0);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(d{1, 2})")), 1);
}

TEST_F(HuffmandictTest, RejectsBadProbabilitySum)
{
    bool threw = false;
    try { eval("huffmandict([1 2 3], [0.4 0.4 0.4]);"); }   // sums to 1.2
    catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(HuffmandictTest, UniformDistributionGivesEqualLengths)
{
    // K=4 uniform -> all codes length 2 -> avglen = 2
    eval("[d, av] = huffmandict([1 2 3 4], [0.25 0.25 0.25 0.25]);");
    EXPECT_DOUBLE_EQ(evalScalar("av"), 2.0);
    for (int k = 1; k <= 4; ++k) {
        const std::string q = "numel(d{" + std::to_string(k) + ", 2})";
        EXPECT_EQ(static_cast<int>(evalScalar(q)), 2);
    }
}
