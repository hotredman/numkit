// libs/builtin/tests/setops_typeclass_test.cpp
//
// Regression guard for bugs/builtin/setops-typeclass.md: ismember / intersect /
// setdiff / union used to throw "Not a double array" on char / logical /
// integer input. MATLAB R2025b accepts them; intersect/setdiff/union PRESERVE
// the input class on the values (ia/ib stay double), ismember returns logical
// tf + double loc. Values + classes below are bit-exact MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SetopsTypeClassTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ismember(char,char) -> logical scalar.
TEST_F(SetopsTypeClassTest, IsmemberCharScalar)
{
    EXPECT_DOUBLE_EQ(evalScalar("ismember('b','abcd')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ismember('z','abcd')"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(ismember('b','abcd'))"), 1.0);
}

// [tf,loc]=ismember(char,char) -> tf logical, loc double.
TEST_F(SetopsTypeClassTest, IsmemberCharVectorLoc)
{
    eval("[tf, loc] = ismember('xbq','abcd');");
    EXPECT_DOUBLE_EQ(evalScalar("tf(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("tf(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(tf)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("loc(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("loc(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(loc)"), 0.0);   // loc is double
}

// ismember on integer.
TEST_F(SetopsTypeClassTest, IsmemberInteger)
{
    EXPECT_DOUBLE_EQ(evalScalar("ismember(int8(2),int8([1 2 3]))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ismember(int8(7),int8([1 2 3]))"), 0.0);
}

// intersect(char,char) -> char 'bc', ia/ib double.
TEST_F(SetopsTypeClassTest, IntersectCharIndices)
{
    eval("[c, ia, ib] = intersect('cabc','bdc');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(c,'bc')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(c)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("islogical(ia)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 3.0);   // 'b' first in 'cabc' at 3
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 1.0);   // 'c' first at 1
    EXPECT_DOUBLE_EQ(evalScalar("ib(1)"), 1.0);   // 'b' in 'bdc' at 1
    EXPECT_DOUBLE_EQ(evalScalar("ib(2)"), 3.0);   // 'c' at 3
}

// intersect on integer preserves class.
TEST_F(SetopsTypeClassTest, IntersectIntegerClass)
{
    eval("c = intersect(int8([3 1 2]),int8([2 4 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("isa(c,'int8')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
}

// intersect on logical.
TEST_F(SetopsTypeClassTest, IntersectLogicalClass)
{
    eval("c = intersect(logical([1 0 1]),logical([0 0 1]));");
    EXPECT_DOUBLE_EQ(evalScalar("islogical(c)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 1.0);
}

// setdiff(char) -> char 'ace'.
TEST_F(SetopsTypeClassTest, SetdiffChar)
{
    eval("c = setdiff('abce','bd');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(c,'ace')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(c)"), 1.0);
}

// union(char) sorted + 'stable'.
TEST_F(SetopsTypeClassTest, UnionChar)
{
    eval("c = union('ab','bc');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(c,'abc')"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("ischar(c)"), 1.0);
    eval("s = union('bca','db','stable');");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(s,'bcad')"), 1.0);
}
