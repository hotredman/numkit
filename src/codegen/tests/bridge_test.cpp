// codegen/tests/bridge_test.cpp
//
// Value-ABI bridge ① (DESIGN.md §6a): the opaque C ABI dispatches to real
// numkit builtins by name, with box/unbox round-tripping scalars and arrays.
// Exercises the C ABI directly (the generated code will use it via the
// prelude RAII wrapper, brick ②).

#include "nk_codegen_rt.h"

#include <gtest/gtest.h>

#include <cmath>

TEST(Bridge, ScalarBuiltin)
{
    nk_val x = nk_box_scalar(0.0);
    nk_val r = nk_call("sin", &x, 1, 1, nullptr, nullptr);
    EXPECT_DOUBLE_EQ(nk_unbox_scalar(r), 0.0);  // sin(0) = 0
    nk_release(x);
    nk_release(r);
}

TEST(Bridge, ScalarBuiltinNonzero)
{
    nk_val x = nk_box_scalar(1.0);
    nk_val r = nk_call("exp", &x, 1, 1, nullptr, nullptr);
    EXPECT_NEAR(nk_unbox_scalar(r), std::exp(1.0), 1e-12);
    nk_release(x);
    nk_release(r);
}

TEST(Bridge, ArrayBuiltinSum)
{
    const double in[3] = {1.0, 2.0, 3.0};
    nk_val       a     = nk_box_array(in, 3);
    EXPECT_EQ(nk_numel(a), 3u);
    nk_val r = nk_call("sum", &a, 1, 1, nullptr, nullptr);
    EXPECT_DOUBLE_EQ(nk_unbox_scalar(r), 6.0);
    nk_release(a);
    nk_release(r);
}

TEST(Bridge, ArrayBuiltinSortRoundTrip)
{
    const double in[4] = {3.0, 1.0, 4.0, 2.0};
    nk_val       a     = nk_box_array(in, 4);
    nk_val       s     = nk_call("sort", &a, 1, 1, nullptr, nullptr);
    ASSERT_EQ(nk_numel(s), 4u);
    double out[4] = {0, 0, 0, 0};
    nk_unbox_array(s, out, 4);
    EXPECT_DOUBLE_EQ(out[0], 1.0);
    EXPECT_DOUBLE_EQ(out[1], 2.0);
    EXPECT_DOUBLE_EQ(out[2], 3.0);
    EXPECT_DOUBLE_EQ(out[3], 4.0);
    nk_release(a);
    nk_release(s);
}

// A runtime error inside a bridged call must be TRANSLATED (nk_error),
// never thrown across the extern "C" boundary (that is UB). The call
// returns null and the process does not crash.
TEST(Bridge, ErrorTranslatedNotThrown)
{
    nk_val   x = nk_box_scalar(1.0);
    nk_error err;
    err.code = 0;
    nk_val r = nk_call("a_function_that_does_not_exist_zzz", &x, 1, 1, nullptr, &err);
    EXPECT_EQ(r, nullptr);            // failed cleanly
    EXPECT_NE(err.code, 0);           // error reported
    EXPECT_NE(err.message[0], '\0');  // with a message
    nk_release(x);
}
