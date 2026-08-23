// toolboxes/builtin/tests/permutations_public_api_test.cpp
//
// Direct C++ API guard for the permutation utilities lifted from
// adapter-only to public typed entry points in math/permutations.hpp:
// numkit::builtin::{colperm, symrcm}. Reference values match the
// script-level colperm_test.cpp / symrcm_test.cpp guards (MATLAB R2025b).

#include <numkit/builtin/specfun.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value pv(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(PermutationsPublicApi, Colperm)
{
    StandardEngine e;
    // columns by ascending nnz -> [3 4 1 2]  (mr defaulted)
    Value p =
        numkit::builtin::colperm(pv(e, "[0 1 0 1; 1 1 1 0; 0 1 0 0; 1 0 0 0]", "S"));
    ASSERT_EQ(p.numel(), 4u);
    EXPECT_EQ(p.dims().rows(), 1u);
    EXPECT_DOUBLE_EQ(p.doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(p.doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(p.doubleData()[2], 1.0);
    EXPECT_DOUBLE_EQ(p.doubleData()[3], 2.0);
    // any element != 0 counts as a nonzero (negatives included) -> [3 1 2]
    Value p2 =
        numkit::builtin::colperm(pv(e, "[-1 0 0; 2 -3 0; 0 4 5]", "S2"), e.resource());
    EXPECT_DOUBLE_EQ(p2.doubleData()[0], 3.0);
    EXPECT_DOUBLE_EQ(p2.doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(p2.doubleData()[2], 2.0);
}

TEST(PermutationsPublicApi, Symrcm)
{
    StandardEngine e;
    // tridiagonal 5x5 -> reversed ordering [5 4 3 2 1]
    Value p = numkit::builtin::symrcm(
        pv(e, "[1 1 0 0 0; 1 1 1 0 0; 0 1 1 1 0; 0 0 1 1 1; 0 0 0 1 1]", "T"),
        e.resource());
    ASSERT_EQ(p.numel(), 5u);
    EXPECT_EQ(p.dims().rows(), 1u);
    EXPECT_DOUBLE_EQ(p.doubleData()[0], 5.0);
    EXPECT_DOUBLE_EQ(p.doubleData()[1], 4.0);
    EXPECT_DOUBLE_EQ(p.doubleData()[4], 1.0);
    // identity -> isolated nodes -> [1 2 3]
    Value pi = numkit::builtin::symrcm(pv(e, "eye(3)", "I"), e.resource());
    ASSERT_EQ(pi.numel(), 3u);
    EXPECT_DOUBLE_EQ(pi.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(pi.doubleData()[2], 3.0);
    // non-square throws
    EXPECT_ANY_THROW(numkit::builtin::symrcm(pv(e, "[1 2 3; 4 5 6]", "R"), e.resource()));
}
