// toolboxes/builtin/tests/discrete_setops_public_api_test.cpp
//
// Direct C++ API guard for the tolerance-aware set operations lifted from
// adapter-only to public typed entry points in discrete.hpp:
// numkit::builtin::{setxor, allunique, numunique, ismembertol, uniquetol}.
// Calls the public functions directly (not via the engine).

#include <numkit/builtin/math/discrete/discrete.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value sv(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(DiscreteSetopsPublicApi, SetxorAllNumUnique)
{
    StandardEngine e;
    // setxor([1 2 3],[2 3 4]) -> [1 4]  (mr defaulted)
    Value c = numkit::math::setxor(sv(e, "[1 2 3]", "a"), sv(e, "[2 3 4]", "b"));
    ASSERT_EQ(c.numel(), 2u);
    EXPECT_DOUBLE_EQ(c.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(c.doubleData()[1], 4.0);
    // allunique
    EXPECT_NE(numkit::math::allunique(sv(e, "[1 2 3]", "u1"), e.resource()).toScalar(),
              0.0);
    EXPECT_EQ(numkit::math::allunique(sv(e, "[1 2 2]", "u2"), e.resource()).toScalar(),
              0.0);
    // numunique
    EXPECT_DOUBLE_EQ(
        numkit::math::numunique(sv(e, "[1 2 2 3 3 3]", "n"), e.resource()).toScalar(),
        3.0);
}

TEST(DiscreteSetopsPublicApi, IsmemberUniqueTol)
{
    StandardEngine e;
    // ismembertol([1 5],[1 2 3]) -> [1 0] (exact membership; default tol)
    Value tf = numkit::math::ismembertol(sv(e, "[1 5]", "q"), sv(e, "[1 2 3]", "s"));
    ASSERT_EQ(tf.numel(), 2u);
    EXPECT_NE(tf.elemAsDouble(0), 0.0);
    EXPECT_EQ(tf.elemAsDouble(1), 0.0);
    // explicit tol argument
    Value tf2 = numkit::math::ismembertol(sv(e, "[2]", "q2"), sv(e, "[1 2 3]", "s2"),
                                     1e-6, e.resource());
    EXPECT_NE(tf2.elemAsDouble(0), 0.0);
    // uniquetol collapses exact duplicates -> [1 2 3]
    Value u = numkit::math::uniquetol(sv(e, "[1 2 2 3]", "x"), 1e-6, e.resource());
    EXPECT_EQ(u.numel(), 3u);
}
