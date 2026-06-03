// libs/builtin/tests/geom_public_api_test.cpp
//
// Exercises the PUBLIC C++ entry points of libs/builtin math/geom
// (numkit::builtin::*) directly — no engine dispatch. Guards the lift of
// geom primitives from adapter-only (`*_reg`, script-callable) to typed
// C++ API functions per docs/LIBRARY_API.md. Inputs are built via a
// throwaway Engine; the assertions call the C++ function itself.

#include <numkit/builtin/math/geom/geom.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value var(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(GeomPublicApiTest, Polyarea)
{
    Engine e;
    Value x = var(e, "[0 1 1 0]", "x");
    Value y = var(e, "[0 0 1 1]", "y");
    // unit square -> area 1
    EXPECT_DOUBLE_EQ(builtin::polyarea(x, y, e.resource()).toScalar(), 1.0);
    // mr defaults to nullptr -> process default resource
    EXPECT_DOUBLE_EQ(builtin::polyarea(x, y).toScalar(), 1.0);
    // triangle (0,0)-(1,0)-(0,1) -> area 0.5
    Value xt = var(e, "[0 1 0]", "xt");
    Value yt = var(e, "[0 0 1]", "yt");
    EXPECT_DOUBLE_EQ(builtin::polyarea(xt, yt, e.resource()).toScalar(), 0.5);
    // < 3 vertices -> 0
    Value x2 = var(e, "[0 1]", "x2");
    Value y2 = var(e, "[0 1]", "y2");
    EXPECT_DOUBLE_EQ(builtin::polyarea(x2, y2, e.resource()).toScalar(), 0.0);
    // shape mismatch throws
    Value yb = var(e, "[0 0 1]", "yb");
    EXPECT_ANY_THROW(builtin::polyarea(x, yb, e.resource()));
}
