// libs/builtin/tests/unique_typeclass_test.cpp
//
// Regression guard for bugs/builtin/unique-typeclass.md: unique() used to throw
// "Not a double array" on char / logical / integer input (it was double-only).
// MATLAB R2025b returns the unique values in the SAME class; the ia/ic index
// outputs stay double. Values + classes below are bit-exact MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UniqueTypeClassTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// unique(char) -> char 'abc' (CLASS PRESERVED).
TEST_F(UniqueTypeClassTest, CharSorted)
{
    eval("u = unique('cbabc');");
    EXPECT_DOUBLE_EQ(evalScalar("ischar(u)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(u,'abc')"), 1.0);
}

// [u,ia,ic] on char — ia/ic are double, first-occurrence ia.
TEST_F(UniqueTypeClassTest, CharIndices)
{
    eval("[u, ia, ic] = unique('cbabc');");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(ia)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(ia)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 3.0);   // 'a' first at 3
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 2.0);   // 'b' first at 2
    EXPECT_DOUBLE_EQ(evalScalar("ia(3)"), 1.0);   // 'c' first at 1
    EXPECT_DOUBLE_EQ(evalScalar("ic(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ic(5)"), 3.0);
}

// 'stable' on char keeps first-occurrence order -> 'cba'.
TEST_F(UniqueTypeClassTest, CharStable)
{
    eval("u = unique('cbabc', 'stable');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(u,'cba')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(u)"), 1.0);
}

// unique(logical) -> logical [0 1].
TEST_F(UniqueTypeClassTest, Logical)
{
    eval("u = unique(logical([1 0 1 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(u)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("numel(u)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(2)"), 1.0);
}

// unique(int8) -> int8 [1 2 3] (class preserved).
TEST_F(UniqueTypeClassTest, Integer)
{
    eval("u = unique(int8([3 1 3 2]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(u,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(3)"), 3.0);
}

// Column vector orientation preserved + uint16 class.
TEST_F(UniqueTypeClassTest, ColumnVectorUint16)
{
    eval("u = unique(uint16([30;10;30;20]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(u,'uint16')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(u,1)"), 3.0);      // column vector: 3x1
    EXPECT_DOUBLE_EQ(evalScalar("size(u,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(1)"), 10.0);
    EXPECT_DOUBLE_EQ(evalScalar("u(3)"), 30.0);
}
