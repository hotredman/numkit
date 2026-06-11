// toolboxes/image/tests/reducepoly_test.cpp
//
// Regression guard for image/reducepoly (Ramer-Douglas-Peucker).
// Fingerprints from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ReducepolyTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("P = [0 0; 1 0.05; 2 0.0; 3 1; 4 2; 5 2.05; 6 2];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ReducepolyTest, DefaultDropsCollinearVertex)
{
    eval("R = reducepoly(P);");          // default tolerance 0.001
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 6);
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,2)")), 2);
    // Endpoints retained exactly.
    EXPECT_DOUBLE_EQ(evalScalar("R(1,1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(end,1)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(end,2)"), 2.0);
    // The [3 1] vertex (collinear with [2 0]-[4 2]) is dropped; the 4th
    // retained vertex is therefore [4 2].
    EXPECT_DOUBLE_EQ(evalScalar("R(4,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(4,2)"), 2.0);
}

TEST_F(ReducepolyTest, HighToleranceKeepsEndpointsOnly)
{
    EXPECT_EQ(static_cast<int>(evalScalar("size(reducepoly(P,0.1),1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(reducepoly(P,1),1)")), 2);
}

TEST_F(ReducepolyTest, ZeroToleranceMinimalReduction)
{
    // tol 0 -> eps: same minimal reduction as the small default here.
    EXPECT_EQ(static_cast<int>(evalScalar("size(reducepoly(P,0),1)")), 6);
}

TEST_F(ReducepolyTest, CollinearCollapsesToEndpoints)
{
    eval("C = [0 0; 1 1; 2 2; 3 3; 4 4];");
    EXPECT_EQ(static_cast<int>(evalScalar("size(reducepoly(C,0.01),1)")), 2);
}

TEST_F(ReducepolyTest, TriangleWaveKeepsAllPeaks)
{
    eval("T = [0 0; 1 1; 2 0; 3 1; 4 0];");
    eval("R = reducepoly(T, 0.1);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(R,1)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("R(3,2)"), 0.0);
}

TEST_F(ReducepolyTest, SmallInputsReturnedAsIs)
{
    EXPECT_EQ(static_cast<int>(evalScalar("size(reducepoly([0 0; 5 5],0.5),1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(reducepoly([3 4],0.5),1)")), 1);
}

TEST_F(ReducepolyTest, ClassPreservedForInteger)
{
    eval("Pi = int32([0 0; 1 0; 2 5; 3 0]);");
    eval("Ri = reducepoly(Pi, 0.1);");
    EXPECT_TRUE(eval("Ri").type() == ValueType::INT32);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Ri,1)")), 4);
}

TEST_F(ReducepolyTest, ToleranceOutOfRangeThrows)
{
    bool tlo = false, thi = false;
    try { eval("reducepoly(P, -0.1);"); } catch (...) { tlo = true; }
    try { eval("reducepoly(P, 2);"); }    catch (...) { thi = true; }
    EXPECT_TRUE(tlo);
    EXPECT_TRUE(thi);
}
