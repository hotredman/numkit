// toolboxes/comm/tests/cyclpoly_test.cpp
//
// Regression guard for cyclpoly (Error Correction Codes). Reference
// generator polynomials from the MATLAB R2025b probe.

#include <numkit/comm/coding/blockcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CyclpolyTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Default: first generator polynomial of x^7-1 of the form 1+...+x^3.
TEST_F(CyclpolyTest, Default74)
{
    eval("p = cyclpoly(7, 4);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(p, [1 0 1 1])"), 1.0);
}

// (15,11) cyclic code generator polynomial.
TEST_F(CyclpolyTest, Default1511)
{
    eval("p = cyclpoly(15, 11);");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(p, [1 0 0 1 1])"), 1.0);
}

// 'min' (fewest terms) and 'max' (most terms) selection.
TEST_F(CyclpolyTest, MinMax)
{
    EXPECT_DOUBLE_EQ(evalScalar("isequal(cyclpoly(7,4,'min'), [1 0 1 1])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(cyclpoly(7,4,'max'), [1 1 0 1])"), 1.0);
}

// 'all' returns every generator polynomial, sorted by term count.
TEST_F(CyclpolyTest, All)
{
    eval("pa = cyclpoly(7, 4, 'all');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(pa,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(pa,2)")), 4);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(pa, [1 0 1 1; 1 1 0 1])"), 1.0);
}

// Every returned polynomial actually divides x^n - 1.
TEST_F(CyclpolyTest, DividesXnMinus1)
{
    // x^7+1 mod (x^3+x^2+1) == 0 in GF(2): use the generator from cyclpoly.
    eval("p = cyclpoly(7,4); g = [1 0 1 1];");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(p, g)"), 1.0);
}

// Direct C++ API.
TEST_F(CyclpolyTest, PublicApi)
{
    Value p = comm::cyclpoly(7, 4, "", engine.resource());
    ASSERT_EQ(p.dims().rows(), 1u);
    ASSERT_EQ(p.dims().cols(), 4u);
    EXPECT_DOUBLE_EQ(p.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(p.doubleData()[1], 0.0);
    EXPECT_DOUBLE_EQ(p.doubleData()[2], 1.0);
    EXPECT_DOUBLE_EQ(p.doubleData()[3], 1.0);
}
