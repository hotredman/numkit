// codegen/tests/bridge_test.cpp
//
// Value-ABI bridge ① (DESIGN.md §6a): the opaque C ABI dispatches to real
// numkit builtins by name, with box/unbox round-tripping scalars and arrays.
// Exercises the C ABI directly (the generated code will use it via the
// prelude RAII wrapper, brick ②).

#include "nk_codegen_rt.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>

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

// ---- Plugin / extension ABI (DESIGN.md §6b) --------------------------------
// A plugin PROVIDES functions with the nk_fn signature (the mirror of
// nk_call). Once registered, they resolve from numkit source / nk_call /
// embedding exactly like a builtin — proving the register -> resolve ->
// dispatch loop with no core change (uses Engine::registerFunction).

// A plugin function: doubles its scalar argument.
static nk_val nk_test_double(const nk_val *args, size_t nargs, size_t nargout,
                             nk_val *extra_outs, nk_error *err)
{
    (void)nargs;
    (void)nargout;
    (void)extra_outs;
    (void)err;
    return nk_box_scalar(nk_unbox_scalar(args[0]) * 2.0);
}

TEST(Plugin, RegisterAndCall)
{
    ASSERT_EQ(nk_register_fn("nk_test_double", nk_test_double), 0);
    nk_val x = nk_box_scalar(21.0);
    nk_val r = nk_call("nk_test_double", &x, 1, 1, nullptr, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_DOUBLE_EQ(nk_unbox_scalar(r), 42.0);
    nk_release(x);
    nk_release(r);
}

// A two-output plugin function: [q, r] = divmod(a, b).
static nk_val nk_test_divmod(const nk_val *args, size_t nargs, size_t nargout,
                             nk_val *extra_outs, nk_error *err)
{
    (void)nargs;
    (void)err;
    const double a = nk_unbox_scalar(args[0]);
    const double b = nk_unbox_scalar(args[1]);
    if (nargout >= 2 && extra_outs) extra_outs[0] = nk_box_scalar(std::fmod(a, b));
    return nk_box_scalar(std::trunc(a / b));  // quotient (first output)
}

TEST(Plugin, RegisterMultiOutput)
{
    ASSERT_EQ(nk_register_fn("nk_test_divmod", nk_test_divmod), 0);
    nk_val args[2] = {nk_box_scalar(17.0), nk_box_scalar(5.0)};
    nk_val extra[1] = {nullptr};
    nk_val q = nk_call("nk_test_divmod", args, 2, 2, extra, nullptr);
    ASSERT_NE(q, nullptr);
    ASSERT_NE(extra[0], nullptr);
    EXPECT_DOUBLE_EQ(nk_unbox_scalar(q), 3.0);         // 17 div 5
    EXPECT_DOUBLE_EQ(nk_unbox_scalar(extra[0]), 2.0);  // 17 mod 5
    nk_release(args[0]);
    nk_release(args[1]);
    nk_release(q);
    nk_release(extra[0]);
}

// A plugin function that reports an error must surface as nk_error at the
// call site, never crash the process.
static nk_val nk_test_raise(const nk_val *args, size_t nargs, size_t nargout,
                            nk_val *extra_outs, nk_error *err)
{
    (void)args;
    (void)nargs;
    (void)nargout;
    (void)extra_outs;
    if (err) {
        err->code = 7;
        std::snprintf(err->message, sizeof(err->message), "plugin says no");
    }
    return nullptr;
}

TEST(Plugin, ErrorPropagates)
{
    ASSERT_EQ(nk_register_fn("nk_test_raise", nk_test_raise), 0);
    nk_val  x = nk_box_scalar(1.0);
    nk_error e;
    e.code = 0;
    nk_val r = nk_call("nk_test_raise", &x, 1, 1, nullptr, &e);
    EXPECT_EQ(r, nullptr);            // failed cleanly
    EXPECT_NE(e.code, 0);             // error reported across the bridge
    EXPECT_NE(e.message[0], '\0');    // with a message
    nk_release(x);
}

// Bad arguments to registration are rejected, not crashed.
TEST(Plugin, RegisterRejectsNull)
{
    EXPECT_NE(nk_register_fn(nullptr, nk_test_double), 0);
    EXPECT_NE(nk_register_fn("nk_test_null", nullptr), 0);
}
