// toolboxes/comm/tests/dpcm_test.cpp
//
// Regression guard for dpcmenco / dpcmdeco — Differential Pulse
// Code Modulation encoder + decoder. Bit-equal with MATLAB R2025b
// on the standard probe (1st-order predictor, 6-bin codebook).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DpcmTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DpcmTest, KnownEncodeIndices)
{
    // MATLAB: dpcmenco([0.1 0.5 0.7 0.6 0.2 -0.3 -0.5], cb, p, [0 1])
    // -> indx = [3 3 3 2 2 1 2]
    eval("predictor = [0 1];"
         "partition = [-1 -0.5 0 0.5 1];"
         "codebook  = [-1.5 -0.75 -0.25 0.25 0.75 1.5];"
         "[indx, qe] = dpcmenco([0.1 0.5 0.7 0.6 0.2 -0.3 -0.5],"
         "                       codebook, partition, predictor);");
    EXPECT_DOUBLE_EQ(evalScalar("indx(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(4)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(5)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(6)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("indx(7)"), 2.0);
}

TEST_F(DpcmTest, KnownQuanterr)
{
    eval("predictor = [0 1];"
         "partition = [-1 -0.5 0 0.5 1];"
         "codebook  = [-1.5 -0.75 -0.25 0.25 0.75 1.5];"
         "[~, qe] = dpcmenco([0.1 0.5 0.7 0.6 0.2 -0.3 -0.5],"
         "                    codebook, partition, predictor);");
    EXPECT_DOUBLE_EQ(evalScalar("qe(1)"),  0.25);
    EXPECT_DOUBLE_EQ(evalScalar("qe(4)"), -0.25);
    EXPECT_DOUBLE_EQ(evalScalar("qe(6)"), -0.75);
}

TEST_F(DpcmTest, DecodeRoundTrip)
{
    eval("predictor = [0 1];"
         "partition = [-1 -0.5 0 0.5 1];"
         "codebook  = [-1.5 -0.75 -0.25 0.25 0.75 1.5];"
         "sig = [0.1 0.5 0.7 0.6 0.2 -0.3 -0.5];"
         "[indx, qe] = dpcmenco(sig, codebook, partition, predictor);"
         "[sigout, qe2] = dpcmdeco(indx, codebook, predictor);");
    // dpcmdeco's reconstructed signal matches MATLAB:
    EXPECT_DOUBLE_EQ(evalScalar("sigout(1)"),  0.25);
    EXPECT_DOUBLE_EQ(evalScalar("sigout(2)"),  0.5);
    EXPECT_DOUBLE_EQ(evalScalar("sigout(7)"), -0.75);
    // quanterr from encoder == codebook(indx+1) == decoder's qe
    EXPECT_DOUBLE_EQ(evalScalar("isequal(qe, qe2)"), 1.0);
}

TEST_F(DpcmTest, ZeroPredictorIsScalarQuantize)
{
    // predictor [0 0] = no prediction; output is just nearest codebook
    eval("predictor = [0 0];"
         "partition = [-1 0 1];"
         "codebook  = [-2 -0.5 0.5 2];"
         "[indx, qe] = dpcmenco([0.7 -0.3 1.5 -1.2],"
         "                       codebook, partition, predictor);");
    EXPECT_DOUBLE_EQ(evalScalar("indx(1)"), 2.0);  // 0.7 in [0, 1]
    EXPECT_DOUBLE_EQ(evalScalar("indx(2)"), 1.0);  // -0.3 in [-1, 0]
    EXPECT_DOUBLE_EQ(evalScalar("indx(3)"), 3.0);  // 1.5 > 1
    EXPECT_DOUBLE_EQ(evalScalar("indx(4)"), 0.0);  // -1.2 < -1
}

TEST_F(DpcmTest, RowOrientationPreserved)
{
    eval("predictor = [0 1]; partition = [-1 0 1]; codebook = [-2 -0.5 0.5 2];"
         "[indx, ~] = dpcmenco([0.5 0.5 0.5], codebook, partition, predictor);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(indx, 1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(indx, 2)")), 3);
}

TEST_F(DpcmTest, ColumnOrientationPreserved)
{
    eval("predictor = [0 1]; partition = [-1 0 1]; codebook = [-2 -0.5 0.5 2];"
         "[indx, ~] = dpcmenco([0.5; 0.5; 0.5], codebook, partition, predictor);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(indx, 1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(indx, 2)")), 1);
}

TEST_F(DpcmTest, RejectsCodebookPartitionMismatch)
{
    bool threw = false;
    try {
        eval("dpcmenco([1 2], [1 2 3], [1 2 3], [0 1]);");
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(DpcmTest, RejectsBadPredictorLength)
{
    bool threw = false;
    try {
        eval("dpcmenco([1 2], [1 2], [1.5], [0]);");  // predictor length 1
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(DpcmTest, RejectsOutOfRangeIndex)
{
    bool threw = false;
    try {
        eval("dpcmdeco([0 99], [1 2 3], [0 1]);");  // 99 >= length(codebook)
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
