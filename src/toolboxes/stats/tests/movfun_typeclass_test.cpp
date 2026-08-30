// toolboxes/stats/tests/movfun_typeclass_test.cpp
//
// Regression guard for bugs/stats/movfun-typeclass.md: movsum/movprod/movmean
// used to throw "Not a double array" on integer/logical input. MATLAB R2025b
// PROMOTES integer/logical to double for these arithmetic moving functions
// (class NOT preserved). Window 3, endpoints 'shrink'. Bit-exact MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MovfunTypeClassTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// movsum(int8) -> double [4 6 8 11 9].
TEST_F(MovfunTypeClassTest, MovsumInt8)
{
    eval("y = movsum(int8([3 1 2 5 4]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(y)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 8.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 11.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 9.0);
}

// movprod(int8) -> double [3 6 10 40 20].
TEST_F(MovfunTypeClassTest, MovprodInt8)
{
    eval("y = movprod(int8([3 1 2 5 4]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"), 40.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 20.0);
}

// movmean(int8) -> double, fractional values.
TEST_F(MovfunTypeClassTest, MovmeanInt8)
{
    eval("y = movmean(int8([3 1 2 5 4]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'double')"), 1.0);
    EXPECT_NEAR(evalScalar("y(1)"), 2.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(3)"), 8.0 / 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(4)"), 11.0 / 3.0, 1e-12);
    EXPECT_NEAR(evalScalar("y(5)"), 4.5, 1e-12);
}

// movsum(logical) -> double [1 2 2 2 1].
TEST_F(MovfunTypeClassTest, MovsumLogical)
{
    eval("y = movsum(logical([1 0 1 1 0]), 3);");
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(5)"), 1.0);
}

// uint16 also promotes to double; double input unchanged (regression).
TEST_F(MovfunTypeClassTest, Uint16PromotesAndDoubleUnchanged)
{
    eval("y = movsum(uint16([30 10 50 20]), 2);");   // [30 40 60 70]
    EXPECT_DOUBLE_EQ(evalScalar("isa(y,'double')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), 40.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"), 60.0);
    eval("d = movsum([1 2 3 4], 2);");               // double path untouched
    EXPECT_DOUBLE_EQ(evalScalar("d(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("d(4)"), 7.0);
}
