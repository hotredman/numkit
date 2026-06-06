// libs/builtin/tests/symrcm_test.cpp
//
// Regression guard for symrcm.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SymRcmTest : public ::testing::Test
{
public:
    StdEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(SymRcmTest, ClassicBandwidthExample)
{
    eval("A = [1 1 0 0 1 0; 1 1 1 0 0 0; 0 1 1 1 0 0; "
         "0 0 1 1 1 1; 1 0 0 1 1 0; 0 0 0 1 0 1];"
         "p = symrcm(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("p(1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("p(2)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("p(3)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("p(4)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("p(5)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("p(6)")), 6);
}

TEST_F(SymRcmTest, TridiagonalReverses)
{
    eval("A = [1 1 0 0 0; 1 1 1 0 0; 0 1 1 1 0; 0 0 1 1 1; 0 0 0 1 1];"
         "p = symrcm(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("p(1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("p(2)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("p(3)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("p(4)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("p(5)")), 1);
}

TEST_F(SymRcmTest, DisconnectedComponents)
{
    eval("A = [1 1 0 0; 1 1 0 0; 0 0 1 1; 0 0 1 1];"
         "p = symrcm(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("p(1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("p(2)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("p(3)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("p(4)")), 3);
}

TEST_F(SymRcmTest, OutputIsRowVectorOfDoubles)
{
    eval("p = symrcm(eye(5));");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,2)")), 5);
    EXPECT_EQ(eval("class(p)").toString(), "double");
}

TEST_F(SymRcmTest, EmptyInput)
{
    eval("p = symrcm([]);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(p,2)")), 0);
}

TEST_F(SymRcmTest, NonSquareThrows)
{
    EXPECT_THROW(eval("symrcm([1 0 1; 0 1 0]);"), std::exception);
}

TEST_F(SymRcmTest, NoArgsThrows)
{
    EXPECT_THROW(eval("symrcm();"), std::exception);
}
