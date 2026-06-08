// toolboxes/builtin/tests/manip_public_api_test.cpp
//
// Direct C++ API guard for the array-manip helpers lifted from
// adapter-only to public typed entry points: numkit::builtin::sub2ind
// (variadic subscripts via Span) and ind2sub (multi-output via
// std::vector). Calls the public functions directly (not via the engine).

#include <numkit/builtin/language/arrays/manip.hpp>
#include <numkit/core/engine.hpp>

#include <vector>
#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value mvar(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(ManipPublicApi, Sub2ind)
{
    StandardEngine e;
    Value siz = mvar(e, "[3 4]", "siz");
    // scalar subscripts: (row 2, col 3) in a 3x4 array -> linear 8
    std::vector<Value> subs1 = {mvar(e, "2", "r"), mvar(e, "3", "c")};
    Value lin = builtin::sub2ind(siz, Span<const Value>(subs1)); // mr defaulted
    EXPECT_DOUBLE_EQ(lin.toScalar(), 8.0);
    // vector subscripts -> [1 2 6]
    std::vector<Value> subs2 = {mvar(e, "[1 2 3]", "rr"), mvar(e, "[1 1 2]", "cc")};
    Value linv = builtin::sub2ind(siz, Span<const Value>(subs2), e.resource());
    ASSERT_EQ(linv.numel(), 3u);
    EXPECT_DOUBLE_EQ(linv.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(linv.doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(linv.doubleData()[2], 6.0);
    // empty siz throws
    std::vector<Value> s = {mvar(e, "1", "one")};
    EXPECT_ANY_THROW(
        builtin::sub2ind(mvar(e, "[]", "emptysz"), Span<const Value>(s)));
}

TEST(ManipPublicApi, Ind2sub)
{
    StandardEngine e;
    Value siz = mvar(e, "[3 4]", "siz");
    // scalar ind, nout defaults to numel(siz) = 2 -> (row 2, col 3)
    std::vector<Value> rs = builtin::ind2sub(siz, mvar(e, "8", "ind"));
    ASSERT_EQ(rs.size(), 2u);
    EXPECT_DOUBLE_EQ(rs[0].toScalar(), 2.0);
    EXPECT_DOUBLE_EQ(rs[1].toScalar(), 3.0);
    // vector ind [1 2 6], explicit nout = 2
    std::vector<Value> rv =
        builtin::ind2sub(siz, mvar(e, "[1 2 6]", "iv"), 2, e.resource());
    ASSERT_EQ(rv.size(), 2u);
    EXPECT_DOUBLE_EQ(rv[0].doubleData()[2], 3.0); // row of linear 6
    EXPECT_DOUBLE_EQ(rv[1].doubleData()[2], 2.0); // col of linear 6
    // round-trip back through sub2ind
    Value back = builtin::sub2ind(siz, Span<const Value>(rv), e.resource());
    EXPECT_DOUBLE_EQ(back.doubleData()[2], 6.0);
    // empty siz throws
    EXPECT_ANY_THROW(builtin::ind2sub(mvar(e, "[]", "esz"), mvar(e, "1", "i1")));
}
