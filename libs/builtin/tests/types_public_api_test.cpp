// libs/builtin/tests/types_public_api_test.cpp
//
// Direct-call tests for numkit::m::builtin type functions.

#include <numkit/m/builtin/MStdTypes.hpp>

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

using numkit::m::Allocator;
using numkit::m::MType;
using numkit::m::MValue;

// ── Numeric constructors: saturation ─────────────────────────────────────
TEST(BuiltinTypesPublicApi, Int8SaturatesAboveMax)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue r = numkit::m::builtin::int8(alloc, MValue::scalar(500.0, &alloc));
    ASSERT_EQ(r.type(), MType::INT8);
    EXPECT_EQ(*static_cast<const int8_t *>(r.rawData()),
              std::numeric_limits<int8_t>::max());
}

TEST(BuiltinTypesPublicApi, Uint8SaturatesNegativeToZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue r = numkit::m::builtin::uint8(alloc, MValue::scalar(-7.0, &alloc));
    ASSERT_EQ(r.type(), MType::UINT8);
    EXPECT_EQ(*static_cast<const uint8_t *>(r.rawData()), 0u);
}

TEST(BuiltinTypesPublicApi, Int32RoundsToNearest)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue r = numkit::m::builtin::int32(alloc, MValue::scalar(3.7, &alloc));
    EXPECT_EQ(*static_cast<const int32_t *>(r.rawData()), 4);
}

TEST(BuiltinTypesPublicApi, Int32OfNanIsZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue nan = MValue::scalar(std::nan(""), &alloc);
    MValue r = numkit::m::builtin::int32(alloc, nan);
    EXPECT_EQ(*static_cast<const int32_t *>(r.rawData()), 0);
}

TEST(BuiltinTypesPublicApi, SingleConvertsDoubleToFloat)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue r = numkit::m::builtin::single(alloc, MValue::scalar(3.14, &alloc));
    ASSERT_EQ(r.type(), MType::SINGLE);
    EXPECT_NEAR(*static_cast<const float *>(r.rawData()), 3.14f, 1e-6f);
}

TEST(BuiltinTypesPublicApi, ToDoubleFromInt32IsDoubleTyped)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue i = numkit::m::builtin::int32(alloc, MValue::scalar(5.0, &alloc));
    MValue d = numkit::m::builtin::toDouble(alloc, i);
    ASSERT_EQ(d.type(), MType::DOUBLE);
    EXPECT_DOUBLE_EQ(d.toScalar(), 5.0);
}

// ── logical ─────────────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, LogicalFromNumericNonZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue r = numkit::m::builtin::logical(alloc, MValue::scalar(7.5, &alloc));
    ASSERT_TRUE(r.isLogical());
    EXPECT_TRUE(r.toBool());
}

TEST(BuiltinTypesPublicApi, LogicalFromZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue r = numkit::m::builtin::logical(alloc, MValue::scalar(0.0, &alloc));
    EXPECT_FALSE(r.toBool());
}

// ── Type predicates ─────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, PredicatesOnDoubleScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = MValue::scalar(3.14, &alloc);

    EXPECT_TRUE(numkit::m::builtin::isnumeric(alloc, x).toBool());
    EXPECT_FALSE(numkit::m::builtin::islogical(alloc, x).toBool());
    EXPECT_FALSE(numkit::m::builtin::ischar(alloc, x).toBool());
    EXPECT_FALSE(numkit::m::builtin::iscell(alloc, x).toBool());
    EXPECT_FALSE(numkit::m::builtin::isempty(alloc, x).toBool());
    EXPECT_TRUE(numkit::m::builtin::isscalar(alloc, x).toBool());
    EXPECT_TRUE(numkit::m::builtin::isreal(alloc, x).toBool());
    EXPECT_FALSE(numkit::m::builtin::isinteger(alloc, x).toBool());
    EXPECT_TRUE(numkit::m::builtin::isfloat(alloc, x).toBool());
    EXPECT_FALSE(numkit::m::builtin::issingle(alloc, x).toBool());
}

TEST(BuiltinTypesPublicApi, IsintegerAfterInt32)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue i = numkit::m::builtin::int32(alloc, MValue::scalar(5.0, &alloc));
    EXPECT_TRUE(numkit::m::builtin::isinteger(alloc, i).toBool());
    EXPECT_FALSE(numkit::m::builtin::isfloat(alloc, i).toBool());
}

TEST(BuiltinTypesPublicApi, IsemptyOnEmpty)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::m::builtin::isempty(alloc, MValue::empty()).toBool());
}

// ── isnan / isinf ───────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, IsnanScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::m::builtin::isnan(alloc, MValue::scalar(std::nan(""), &alloc))
                    .toBool());
    EXPECT_FALSE(numkit::m::builtin::isnan(alloc, MValue::scalar(1.0, &alloc)).toBool());
}

TEST(BuiltinTypesPublicApi, IsinfScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    double posInf = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(numkit::m::builtin::isinf(alloc, MValue::scalar(posInf, &alloc)).toBool());
    EXPECT_FALSE(numkit::m::builtin::isinf(alloc, MValue::scalar(1.0, &alloc)).toBool());
}

TEST(BuiltinTypesPublicApi, IsnanVectorElementwise)
{
    Allocator alloc = Allocator::defaultAllocator();
    auto v = MValue::matrix(1, 3, MType::DOUBLE, &alloc);
    double *d = v.doubleDataMut();
    d[0] = 1.0; d[1] = std::nan(""); d[2] = 0.0;
    MValue r = numkit::m::builtin::isnan(alloc, v);
    ASSERT_EQ(r.type(), MType::LOGICAL);
    EXPECT_EQ(r.logicalData()[0], 0u);
    EXPECT_EQ(r.logicalData()[1], 1u);
    EXPECT_EQ(r.logicalData()[2], 0u);
}

// ── isequal / isequaln ──────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, IsequalMatchingScalars)
{
    Allocator alloc = Allocator::defaultAllocator();
    EXPECT_TRUE(numkit::m::builtin::isequal(alloc, MValue::scalar(5.0, &alloc),
                                            MValue::scalar(5.0, &alloc))
                    .toBool());
}

TEST(BuiltinTypesPublicApi, IsequalDifferentTypesReturnsFalse)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue d = MValue::scalar(5.0, &alloc);
    MValue i = numkit::m::builtin::int32(alloc, d);
    EXPECT_FALSE(numkit::m::builtin::isequal(alloc, d, i).toBool());
}

TEST(BuiltinTypesPublicApi, IsequalNanVsNan)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue n1 = MValue::scalar(std::nan(""), &alloc);
    MValue n2 = MValue::scalar(std::nan(""), &alloc);
    // isequal: NaN != NaN
    EXPECT_FALSE(numkit::m::builtin::isequal(alloc, n1, n2).toBool());
    // isequaln: NaN == NaN
    EXPECT_TRUE(numkit::m::builtin::isequaln(alloc, n1, n2).toBool());
}

// ── class ───────────────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, ClassOfDouble)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue r = numkit::m::builtin::classOf(alloc, MValue::scalar(1.0, &alloc));
    EXPECT_EQ(r.toString(), "double");
}

TEST(BuiltinTypesPublicApi, ClassOfInt32)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue i = numkit::m::builtin::int32(alloc, MValue::scalar(1.0, &alloc));
    MValue r = numkit::m::builtin::classOf(alloc, i);
    EXPECT_EQ(r.toString(), "int32");
}
