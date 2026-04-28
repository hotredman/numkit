// libs/builtin/tests/types_public_api_test.cpp
//
// Direct-call tests for numkit::builtin type functions.

#include <numkit/builtin/datatypes/numeric/types.hpp>

#include <numkit/core/allocator.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

using numkit::Allocator;
using numkit::ValueType;
using numkit::Value;

// ── Numeric constructors: saturation ─────────────────────────────────────
TEST(BuiltinTypesPublicApi, Int8SaturatesAboveMax)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::int8(alloc, Value::scalar(500.0, &alloc));
    ASSERT_EQ(r.type(), ValueType::INT8);
    EXPECT_EQ(*static_cast<const int8_t *>(r.rawData()),
              std::numeric_limits<int8_t>::max());
}

TEST(BuiltinTypesPublicApi, Uint8SaturatesNegativeToZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::uint8(alloc, Value::scalar(-7.0, &alloc));
    ASSERT_EQ(r.type(), ValueType::UINT8);
    EXPECT_EQ(*static_cast<const uint8_t *>(r.rawData()), 0u);
}

TEST(BuiltinTypesPublicApi, Int32RoundsToNearest)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::int32(alloc, Value::scalar(3.7, &alloc));
    EXPECT_EQ(*static_cast<const int32_t *>(r.rawData()), 4);
}

TEST(BuiltinTypesPublicApi, Int32OfNanIsZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value nan = Value::scalar(std::nan(""), &alloc);
    Value r = numkit::builtin::int32(alloc, nan);
    EXPECT_EQ(*static_cast<const int32_t *>(r.rawData()), 0);
}

TEST(BuiltinTypesPublicApi, SingleConvertsDoubleToFloat)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::single(alloc, Value::scalar(3.14, &alloc));
    ASSERT_EQ(r.type(), ValueType::SINGLE);
    EXPECT_NEAR(*static_cast<const float *>(r.rawData()), 3.14f, 1e-6f);
}

TEST(BuiltinTypesPublicApi, ToDoubleFromInt32IsDoubleTyped)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value i = numkit::builtin::int32(alloc, Value::scalar(5.0, &alloc));
    Value d = numkit::builtin::toDouble(alloc, i);
    ASSERT_EQ(d.type(), ValueType::DOUBLE);
    EXPECT_DOUBLE_EQ(d.toScalar(), 5.0);
}

// ── logical ─────────────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, LogicalFromNumericNonZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::logical(alloc, Value::scalar(7.5, &alloc));
    ASSERT_TRUE(r.isLogical());
    EXPECT_TRUE(r.toBool());
}

TEST(BuiltinTypesPublicApi, LogicalFromZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::logical(alloc, Value::scalar(0.0, &alloc));
    EXPECT_FALSE(r.toBool());
}

// ── Type predicates ─────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, PredicatesOnDoubleScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value x = Value::scalar(3.14, &alloc);

    EXPECT_TRUE(numkit::builtin::isnumeric(alloc, x).toBool());
    EXPECT_FALSE(numkit::builtin::islogical(alloc, x).toBool());
    EXPECT_FALSE(numkit::builtin::ischar(alloc, x).toBool());
    EXPECT_FALSE(numkit::builtin::iscell(alloc, x).toBool());
    EXPECT_FALSE(numkit::builtin::isempty(alloc, x).toBool());
    EXPECT_TRUE(numkit::builtin::isscalar(alloc, x).toBool());
    EXPECT_TRUE(numkit::builtin::isreal(alloc, x).toBool());
    EXPECT_FALSE(numkit::builtin::isinteger(alloc, x).toBool());
    EXPECT_TRUE(numkit::builtin::isfloat(alloc, x).toBool());
    EXPECT_FALSE(numkit::builtin::issingle(alloc, x).toBool());
}

TEST(BuiltinTypesPublicApi, IsintegerAfterInt32)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value i = numkit::builtin::int32(alloc, Value::scalar(5.0, &alloc));
    EXPECT_TRUE(numkit::builtin::isinteger(alloc, i).toBool());
    EXPECT_FALSE(numkit::builtin::isfloat(alloc, i).toBool());
}

TEST(BuiltinTypesPublicApi, IsemptyOnEmpty)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::builtin::isempty(alloc, Value::empty()).toBool());
}

// ── isnan / isinf ───────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, IsnanScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::builtin::isnan(alloc, Value::scalar(std::nan(""), &alloc))
                    .toBool());
    EXPECT_FALSE(numkit::builtin::isnan(alloc, Value::scalar(1.0, &alloc)).toBool());
}

TEST(BuiltinTypesPublicApi, IsinfScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    double posInf = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(numkit::builtin::isinf(alloc, Value::scalar(posInf, &alloc)).toBool());
    EXPECT_FALSE(numkit::builtin::isinf(alloc, Value::scalar(1.0, &alloc)).toBool());
}

TEST(BuiltinTypesPublicApi, IsnanVectorElementwise)
{
    Allocator alloc = Allocator::defaultAllocator();
    auto v = Value::matrix(1, 3, ValueType::DOUBLE, &alloc);
    double *d = v.doubleDataMut();
    d[0] = 1.0; d[1] = std::nan(""); d[2] = 0.0;
    Value r = numkit::builtin::isnan(alloc, v);
    ASSERT_EQ(r.type(), ValueType::LOGICAL);
    EXPECT_EQ(r.logicalData()[0], 0u);
    EXPECT_EQ(r.logicalData()[1], 1u);
    EXPECT_EQ(r.logicalData()[2], 0u);
}

// ── isequal / isequaln ──────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, IsequalMatchingScalars)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::builtin::isequal(alloc, Value::scalar(5.0, &alloc),
                                            Value::scalar(5.0, &alloc))
                    .toBool());
}

TEST(BuiltinTypesPublicApi, IsequalDifferentTypesReturnsFalse)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value d = Value::scalar(5.0, &alloc);
    Value i = numkit::builtin::int32(alloc, d);
    EXPECT_FALSE(numkit::builtin::isequal(alloc, d, i).toBool());
}

TEST(BuiltinTypesPublicApi, IsequalNanVsNan)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value n1 = Value::scalar(std::nan(""), &alloc);
    Value n2 = Value::scalar(std::nan(""), &alloc);
    // isequal: NaN != NaN
    EXPECT_FALSE(numkit::builtin::isequal(alloc, n1, n2).toBool());
    // isequaln: NaN == NaN
    EXPECT_TRUE(numkit::builtin::isequaln(alloc, n1, n2).toBool());
}

// ── class ───────────────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, ClassOfDouble)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value r = numkit::builtin::classOf(alloc, Value::scalar(1.0, &alloc));
    EXPECT_EQ(r.toString(), "double");
}

TEST(BuiltinTypesPublicApi, ClassOfInt32)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value i = numkit::builtin::int32(alloc, Value::scalar(1.0, &alloc));
    Value r = numkit::builtin::classOf(alloc, i);
    EXPECT_EQ(r.toString(), "int32");
}
