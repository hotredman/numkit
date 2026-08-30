// toolboxes/builtin/tests/unique_last_test.cpp
//
// Regression guard for bugs/builtin/unique-last.md: the sorted-order 'last'
// occurrence selector. [C,ia,ic]=unique(A,'last') reports the LAST occurrence
// of each value in ia, for the vector, complex and 'rows' paths. The default
// ('first') is unchanged. Expected values from MATLAB R2025b.
//
// Remaining sub-gap (DISABLED_UniqueStableLast in known_bugs_test.cpp): the
// 'stable'+'last' ORDERING (MATLAB orders by last occurrence) is not matched.

#include <numkit/core/engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class UniqueLastTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(UniqueLastTest, VectorLast)
{
    eval("[c, ia, ic] = unique([3 1 2 1 3], 'last');");
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 4.0);   // value 1 last @4
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 3.0);   // value 2 @3
    EXPECT_DOUBLE_EQ(evalScalar("ia(3)"), 5.0);   // value 3 last @5
    EXPECT_DOUBLE_EQ(evalScalar("ic(1)"), 3.0);   // X = C(ic)
    EXPECT_DOUBLE_EQ(evalScalar("ic(2)"), 1.0);
}

TEST_F(UniqueLastTest, VectorFirstUnchanged)
{
    eval("[c, ia] = unique([3 1 2 1 3]);");           // default 'first'
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(3)"), 1.0);
    eval("[c2, ia2] = unique([3 1 2 1 3], 'first');"); // explicit 'first'
    EXPECT_DOUBLE_EQ(evalScalar("ia2(1)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia2(3)"), 1.0);
}

TEST_F(UniqueLastTest, ComplexLast)
{
    eval("[c, ia] = unique([1+1i 2 1+1i 3], 'last');");
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 3.0);   // 1+1i last @3
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 2.0);   // 2 @2
    EXPECT_DOUBLE_EQ(evalScalar("ia(3)"), 4.0);   // 3 @4
}

TEST_F(UniqueLastTest, RowsLast)
{
    eval("[c, ia, ic] = unique([1 2;3 4;1 2;5 6], 'rows', 'last');");
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 3.0);   // row [1 2] last @3
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 2.0);   // row [3 4] @2
    EXPECT_DOUBLE_EQ(evalScalar("ia(3)"), 4.0);   // row [5 6] @4
    EXPECT_DOUBLE_EQ(evalScalar("ic(1)"), 1.0);   // X = C(ic,:)
    EXPECT_DOUBLE_EQ(evalScalar("ic(2)"), 2.0);
}

TEST_F(UniqueLastTest, NanLast)
{
    // Each NaN is distinct; finite value 1 last @3. MATLAB: C=[1 NaN NaN],
    // ia=[3 2 4].
    eval("[c, ia] = unique([1 NaN 1 NaN], 'last');");
    EXPECT_DOUBLE_EQ(evalScalar("ia(1)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("ia(3)"), 4.0);
}
