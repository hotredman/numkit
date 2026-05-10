// libs/comm/tests/quantiz_test.cpp
//
// Regression guard for quantiz() — scalar quantizer applier.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class QuantizTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(QuantizTest, KnownIndices)
{
    eval("partition = [-1 -0.5 0 0.5 1];"
         "codebook  = [-1.5 -0.75 -0.25 0.25 0.75 1.5];"
         "indx = quantiz([-2.4 -0.7 0.1 0.5 1.5 3.0], partition);");
    EXPECT_DOUBLE_EQ(evalScalar("indx(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(4)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(5)"), 5.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(6)"), 5.0);
}

TEST_F(QuantizTest, KnownQuantv)
{
    eval("partition = [-1 -0.5 0 0.5 1];"
         "codebook  = [-1.5 -0.75 -0.25 0.25 0.75 1.5];"
         "[~, qv] = quantiz([-2.4 -0.7 0.1 0.5 1.5 3.0],"
         "                   partition, codebook);");
    EXPECT_DOUBLE_EQ(evalScalar("qv(1)"), -1.5);
    EXPECT_DOUBLE_EQ(evalScalar("qv(2)"), -0.75);
    EXPECT_DOUBLE_EQ(evalScalar("qv(5)"),  1.5);
    EXPECT_DOUBLE_EQ(evalScalar("qv(6)"),  1.5);
}

TEST_F(QuantizTest, DistortionExact)
{
    // distor = mean((sig - quantv).^2)
    // sig = [-2.4 -0.7 0.1 0.5 1.5 3.0],
    // quantv = [-1.5 -0.75 0.25 0.25 1.5 1.5]
    // diff^2 = [0.81 0.0025 0.0225 0.0625 0 2.25]
    // sum = 3.1475 / 6 = 0.524583...
    eval("[~, ~, d] = quantiz([-2.4 -0.7 0.1 0.5 1.5 3.0],"
         "                     [-1 -0.5 0 0.5 1],"
         "                     [-1.5 -0.75 -0.25 0.25 0.75 1.5]);");
    EXPECT_NEAR(evalScalar("d"), 3.1475 / 6.0, 1e-12);
}

TEST_F(QuantizTest, RowOrientationPreserved)
{
    eval("indx = quantiz([1 2 3], [1.5 2.5]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(indx, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(indx, 2)")), 3);
}

TEST_F(QuantizTest, ColumnOrientationPreserved)
{
    eval("indx = quantiz([1; 2; 3], [1.5 2.5]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(indx, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(indx, 2)")), 1);
}

TEST_F(QuantizTest, BoundaryAtPartition)
{
    // sig == partition values are NOT counted (strict <).
    eval("indx = quantiz([0.5 1.0], [0.5 1.0]);");
    EXPECT_DOUBLE_EQ(evalScalar("indx(1)"), 0.0);  // 0.5 not > 0.5
    EXPECT_DOUBLE_EQ(evalScalar("indx(2)"), 1.0);  // 1.0 > 0.5 but not > 1.0
}

TEST_F(QuantizTest, RejectsCodebookSizeMismatch)
{
    bool threw = false;
    try {
        eval("[~, qv] = quantiz([1 2 3], [1 2], [1 2]);");  // length mismatch
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(QuantizTest, MonotonicPartitionInvariant)
{
    // Empty partition -> all bins = 0, indx all zeros.
    eval("indx = quantiz([1 2 3 4 5], []);");
    EXPECT_DOUBLE_EQ(evalScalar("max(indx)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("min(indx)"), 0.0);
}
