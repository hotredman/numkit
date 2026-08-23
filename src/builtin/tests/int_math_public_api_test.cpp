// toolboxes/builtin/tests/int_math_public_api_test.cpp
//
// Direct C++ API guard for the bit accessors lifted from adapter-only to
// public typed entry points: numkit::builtin::bitget and bitset (the
// latter now takes its optional value as const Value& = Value::Empty per
// dev-docs/LIBRARY_API.md, replacing the old const Value* parameter).

#include <numkit/builtin/ops.hpp>
#include <numkit/core/engine.hpp>

#include <gtest/gtest.h>

using namespace numkit;

namespace {
Value bvar(Engine &e, const char *expr, const char *name)
{
    e.eval(std::string(name) + " = " + expr + ";");
    return *e.getVariable(name);
}
} // namespace

TEST(IntMathPublicApi, Bitget)
{
    StandardEngine e;
    Value a = bvar(e, "10", "a"); // 1010, bit1=0 bit2=1 bit3=0 bit4=1
    EXPECT_DOUBLE_EQ(numkit::builtin::bitget(a, bvar(e, "2", "n2")).toScalar(), 1.0);
    EXPECT_DOUBLE_EQ(
        numkit::builtin::bitget(a, bvar(e, "1", "n1"), e.resource()).toScalar(), 0.0);
    EXPECT_DOUBLE_EQ(
        numkit::builtin::bitget(a, bvar(e, "4", "n4"), e.resource()).toScalar(), 1.0);
    // vector bit positions broadcast against the scalar a
    Value bits = numkit::builtin::bitget(a, bvar(e, "[1 2 3 4]", "nv"), e.resource());
    ASSERT_EQ(bits.numel(), 4u);
    EXPECT_DOUBLE_EQ(bits.doubleData()[0], 0.0);
    EXPECT_DOUBLE_EQ(bits.doubleData()[1], 1.0);
    EXPECT_DOUBLE_EQ(bits.doubleData()[3], 1.0);
}

TEST(IntMathPublicApi, Bitset)
{
    StandardEngine e;
    // val omitted -> Value::Empty -> defaults to 1: bitset(0,3) = 4
    EXPECT_DOUBLE_EQ(
        numkit::builtin::bitset(bvar(e, "0", "z"), bvar(e, "3", "n3")).toScalar(), 4.0);
    // explicit Value::Empty val also defaults to 1: bitset(1,3) = 5
    EXPECT_DOUBLE_EQ(numkit::builtin::bitset(bvar(e, "1", "o"), bvar(e, "3", "n3b"),
                                     Value::Empty, e.resource())
                         .toScalar(),
                     5.0);
    // val = 0 clears the bit: bitset(7,1,0) = 6
    EXPECT_DOUBLE_EQ(numkit::builtin::bitset(bvar(e, "7", "s"), bvar(e, "1", "n1"),
                                     bvar(e, "0", "v0"), e.resource())
                         .toScalar(),
                     6.0);
    // val = 1 sets the bit: bitset(7,4,1) = 15
    EXPECT_DOUBLE_EQ(numkit::builtin::bitset(bvar(e, "7", "s2"), bvar(e, "4", "n4b"),
                                     bvar(e, "1", "v1"), e.resource())
                         .toScalar(),
                     15.0);
    // val outside {0,1} throws
    EXPECT_ANY_THROW(numkit::builtin::bitset(bvar(e, "1", "o2"), bvar(e, "2", "n2b"),
                                     bvar(e, "5", "bad"), e.resource()));
}
