// toolboxes/comm/tests/istrellis_test.cpp
//
// Regression guard for istrellis (Error Correction Codes). Validates that a
// poly2trellis output is recognised and non-trellis values are rejected.

#include <numkit/comm/coding/convcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class IstrellisTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(IstrellisTest, ValidTrellis)
{
    eval("t = poly2trellis(3, [6 7]);");
    EXPECT_DOUBLE_EQ(evalScalar("double(istrellis(t))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(istrellis(poly2trellis(7, [171 133])))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(istrellis(poly2trellis(4, [13 15 17])))"), 1.0);
}

TEST_F(IstrellisTest, RejectsNonTrellis)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(istrellis(struct('a', 1)))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(istrellis(5))"), 0.0);        // not a struct
    EXPECT_DOUBLE_EQ(evalScalar("double(istrellis([1 2 3]))"), 0.0);  // matrix
    EXPECT_DOUBLE_EQ(evalScalar("double(istrellis('hello'))"), 0.0);  // char
}

TEST_F(IstrellisTest, PublicApi)
{
    eval("t = poly2trellis(3, [6 7]); bad = struct('a', 1);");
    EXPECT_NE(comm::istrellis(*engine.getVariable("t")).toScalar(), 0.0);
    EXPECT_EQ(comm::istrellis(*engine.getVariable("bad")).toScalar(), 0.0);
}
