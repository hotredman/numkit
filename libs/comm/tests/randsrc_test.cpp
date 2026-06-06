// libs/comm/tests/randsrc_test.cpp
//
// Regression guard for randsrc() — random matrix from finite alphabet.
// Bit-equal with MATLAB R2025b when an explicit seed is supplied
// (uses the MatlabMT19937 RNG = MATLAB's mt19937ar).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RandsrcTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(RandsrcTest, BinaryAlphabetSeededMatchesMATLAB)
{
    // MATLAB: randsrc(2, 3, [-1 1], 42) =
    //   -1  1 -1
    //    1  1 -1
    eval("y = randsrc(2, 3, [-1 1], 42);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 2)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 3)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2, 1)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2, 2)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2, 3)"), -1.0);
}

TEST_F(RandsrcTest, FourLevelAlphabetSeededMatchesMATLAB)
{
    // MATLAB: randsrc(3, 4, [-3 -1 1 3], 42) =
    //   -1  1 -3  1
    //    3 -3  3 -3
    //    1 -3  1  3
    eval("y = randsrc(3, 4, [-3 -1 1 3], 42);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1, 1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2, 2)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3, 4)"),  3.0);
}

TEST_F(RandsrcTest, ProbabilityWeights)
{
    // P(symbol=1) = 0.7 -> ~70% of 10000 samples should be 1.
    eval("y = randsrc(1, 10000, [1 2 3; 0.7 0.2 0.1], 42);"
         "p1 = sum(y == 1) / 10000;");
    EXPECT_NEAR(evalScalar("p1"), 0.7, 0.05);
}

TEST_F(RandsrcTest, ShapePreserved)
{
    eval("y = randsrc(5, 8, [0 1], 42);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 8);
}

TEST_F(RandsrcTest, DeterministicOnSameSeed)
{
    eval("a = randsrc(10, 10, [-1 1], 7);"
         "b = randsrc(10, 10, [-1 1], 7);"
         "match = isequal(a, b);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(RandsrcTest, DefaultAlphabetIsBinary)
{
    // Without alphabet arg, MATLAB defaults to [-1, 1].
    eval("y = randsrc(1, 1000, [], 42);"
         "uvals = unique(y);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(uvals)")), 2);
    EXPECT_DOUBLE_EQ(evalScalar("uvals(1)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("uvals(2)"),  1.0);
}

TEST_F(RandsrcTest, RejectsBadProbabilitySum)
{
    bool threw = false;
    try {
        eval("randsrc(1, 10, [1 2 3; 0.5 0.4 0.4]);");  // sums to 1.3
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(RandsrcTest, RejectsNegativeProbability)
{
    bool threw = false;
    try {
        eval("randsrc(1, 10, [1 2; 0.5 -0.5]);");  // negative prob, also sums to 0
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
