// toolboxes/builtin/tests/types_public_api_test.cpp
//
// Direct-call tests for numkit::builtin type functions.

#include <numkit/lang/types/types.hpp>

#include <memory_resource>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>

using numkit::ValueType;
using numkit::Value;

// ── Numeric constructors: saturation ─────────────────────────────────────
TEST(BuiltinTypesPublicApi, Int8SaturatesAboveMax)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::int8(Value::scalar(500.0, mr), mr);
    ASSERT_EQ(r.type(), ValueType::INT8);
    EXPECT_EQ(*static_cast<const int8_t *>(r.rawData()),
              std::numeric_limits<int8_t>::max());
}

TEST(BuiltinTypesPublicApi, Uint8SaturatesNegativeToZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::uint8(Value::scalar(-7.0, mr), mr);
    ASSERT_EQ(r.type(), ValueType::UINT8);
    EXPECT_EQ(*static_cast<const uint8_t *>(r.rawData()), 0u);
}

TEST(BuiltinTypesPublicApi, Int32RoundsToNearest)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::int32(Value::scalar(3.7, mr), mr);
    EXPECT_EQ(*static_cast<const int32_t *>(r.rawData()), 4);
}

TEST(BuiltinTypesPublicApi, Int32OfNanIsZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value nan = Value::scalar(std::nan(""), mr);
    Value r = numkit::lang::int32(nan, mr);
    EXPECT_EQ(*static_cast<const int32_t *>(r.rawData()), 0);
}

TEST(BuiltinTypesPublicApi, SingleConvertsDoubleToFloat)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::single(Value::scalar(3.14, mr), mr);
    ASSERT_EQ(r.type(), ValueType::SINGLE);
    EXPECT_NEAR(*static_cast<const float *>(r.rawData()), 3.14f, 1e-6f);
}

TEST(BuiltinTypesPublicApi, ToDoubleFromInt32IsDoubleTyped)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value i = numkit::lang::int32(Value::scalar(5.0, mr), mr);
    Value d = numkit::lang::toDouble(i, mr);
    ASSERT_EQ(d.type(), ValueType::DOUBLE);
    EXPECT_DOUBLE_EQ(d.toScalar(), 5.0);
}

// ── logical ─────────────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, LogicalFromNumericNonZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::logical(Value::scalar(7.5, mr), mr);
    ASSERT_TRUE(r.isLogical());
    EXPECT_TRUE(r.toBool());
}

TEST(BuiltinTypesPublicApi, LogicalFromZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::logical(Value::scalar(0.0, mr), mr);
    EXPECT_FALSE(r.toBool());
}

// ── Type predicates ─────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, PredicatesOnDoubleScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = Value::scalar(3.14, mr);

    EXPECT_TRUE(numkit::lang::isnumeric(x, mr).toBool());
    EXPECT_FALSE(numkit::lang::islogical(x, mr).toBool());
    EXPECT_FALSE(numkit::lang::ischar(x, mr).toBool());
    EXPECT_FALSE(numkit::lang::iscell(x, mr).toBool());
    EXPECT_FALSE(numkit::lang::isempty(x, mr).toBool());
    EXPECT_TRUE(numkit::lang::isscalar(x, mr).toBool());
    EXPECT_TRUE(numkit::lang::isreal(x, mr).toBool());
    EXPECT_FALSE(numkit::lang::isinteger(x, mr).toBool());
    EXPECT_TRUE(numkit::lang::isfloat(x, mr).toBool());
    EXPECT_FALSE(numkit::lang::issingle(x, mr).toBool());
}

TEST(BuiltinTypesPublicApi, IsintegerAfterInt32)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value i = numkit::lang::int32(Value::scalar(5.0, mr), mr);
    EXPECT_TRUE(numkit::lang::isinteger(i, mr).toBool());
    EXPECT_FALSE(numkit::lang::isfloat(i, mr).toBool());
}

TEST(BuiltinTypesPublicApi, IsemptyOnEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::lang::isempty(Value::empty(), mr).toBool());
}

// ── isnan / isinf ───────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, IsnanScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::lang::isnan(Value::scalar(std::nan(""), mr), mr)
                    .toBool());
    EXPECT_FALSE(numkit::lang::isnan(Value::scalar(1.0, mr), mr).toBool());
}

TEST(BuiltinTypesPublicApi, IsinfScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    double posInf = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(numkit::lang::isinf(Value::scalar(posInf, mr), mr).toBool());
    EXPECT_FALSE(numkit::lang::isinf(Value::scalar(1.0, mr), mr).toBool());
}

TEST(BuiltinTypesPublicApi, IsnanVectorElementwise)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto v = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    double *d = v.doubleDataMut();
    d[0] = 1.0; d[1] = std::nan(""); d[2] = 0.0;
    Value r = numkit::lang::isnan(v, mr);
    ASSERT_EQ(r.type(), ValueType::LOGICAL);
    EXPECT_EQ(r.logicalData()[0], 0u);
    EXPECT_EQ(r.logicalData()[1], 1u);
    EXPECT_EQ(r.logicalData()[2], 0u);
}

// ── isequal / isequaln ──────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, IsequalMatchingScalars)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::lang::isequal(Value::scalar(5.0, mr), Value::scalar(5.0, mr), mr)
                    .toBool());
}

TEST(BuiltinTypesPublicApi, IsequalDifferentTypesReturnsFalse)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value d = Value::scalar(5.0, mr);
    Value i = numkit::lang::int32(d, mr);
    EXPECT_FALSE(numkit::lang::isequal(d, i, mr).toBool());
}

TEST(BuiltinTypesPublicApi, IsequalNanVsNan)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value n1 = Value::scalar(std::nan(""), mr);
    Value n2 = Value::scalar(std::nan(""), mr);
    // isequal: NaN != NaN
    EXPECT_FALSE(numkit::lang::isequal(n1, n2, mr).toBool());
    // isequaln: NaN == NaN
    EXPECT_TRUE(numkit::lang::isequaln(n1, n2, mr).toBool());
}

// ── class ───────────────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, ClassOfDouble)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::lang::classOf(Value::scalar(1.0, mr), mr);
    EXPECT_EQ(r.toString(), "double");
}

TEST(BuiltinTypesPublicApi, ClassOfInt32)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value i = numkit::lang::int32(Value::scalar(1.0, mr), mr);
    Value r = numkit::lang::classOf(i, mr);
    EXPECT_EQ(r.toString(), "int32");
}

// ── Pack 36: cast / swapbytes ────────────────────────────────────────
TEST(BuiltinTypesPublicApi, CastDispatchesByName)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = Value::scalar(3.7, mr);
    EXPECT_EQ(numkit::lang::cast(v, "int32", mr).type(), numkit::ValueType::INT32);
    EXPECT_EQ(numkit::lang::cast(v, "uint16", mr).type(), numkit::ValueType::UINT16);
    EXPECT_EQ(numkit::lang::cast(v, "single", mr).type(), numkit::ValueType::SINGLE);
    EXPECT_EQ(numkit::lang::cast(v, "logical", mr).type(), numkit::ValueType::LOGICAL);
}

TEST(BuiltinTypesPublicApi, CastIntegerSaturates)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = Value::scalar(99999.0, mr);
    Value r = numkit::lang::cast(v, "int8", mr);
    EXPECT_EQ(r.elemAsDouble(0), 127.0);
}

TEST(BuiltinTypesPublicApi, CastBadClassThrows)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = Value::scalar(0.0, mr);
    EXPECT_THROW(numkit::lang::cast(v, "bogus", mr), numkit::Error);
}

TEST(BuiltinTypesPublicApi, SwapbytesUint16)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::lang::uint16(Value::scalar(258.0, mr), mr);  // 0x0102
    Value r = numkit::lang::swapbytes(v, mr);
    EXPECT_EQ(r.type(), numkit::ValueType::UINT16);
    EXPECT_EQ(r.elemAsDouble(0), 513.0);  // 0x0201
}

TEST(BuiltinTypesPublicApi, SwapbytesUint32)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::lang::uint32(Value::scalar(1.0, mr), mr);
    Value r = numkit::lang::swapbytes(v, mr);
    EXPECT_EQ(r.elemAsDouble(0), 16777216.0);  // 0x01000000
}

TEST(BuiltinTypesPublicApi, SwapbytesInvolution)
{
    auto *mr = std::pmr::get_default_resource();
    // swapbytes(swapbytes(x)) == x for every supported type.
    Value v = numkit::lang::int32(Value::scalar(0x12345678, mr), mr);
    Value r = numkit::lang::swapbytes(numkit::lang::swapbytes(v, mr), mr);
    EXPECT_EQ(r.elemAsDouble(0), v.elemAsDouble(0));
}

TEST(BuiltinTypesPublicApi, SwapbytesByteWidth1IsIdentity)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::lang::uint8(Value::scalar(0x42, mr), mr);
    Value r = numkit::lang::swapbytes(v, mr);
    EXPECT_EQ(r.elemAsDouble(0), 0x42);
}

// ── Pack 36: typecast ────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, TypecastUint32ToUint16)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::lang::uint32(Value::scalar(258.0, mr), mr);
    Value r = numkit::lang::typecast(v, "uint16", mr);
    EXPECT_EQ(r.type(), numkit::ValueType::UINT16);
    EXPECT_EQ(r.numel(), 2u);
    EXPECT_EQ(r.elemAsDouble(0), 258.0);
    EXPECT_EQ(r.elemAsDouble(1),   0.0);
}

TEST(BuiltinTypesPublicApi, TypecastSingleArrayToUint8)
{
    auto *mr = std::pmr::get_default_resource();
    auto v = Value::matrix(1, 2, numkit::ValueType::SINGLE, mr);
    static_cast<float *>(v.rawDataMut())[0] = 1.0f;
    static_cast<float *>(v.rawDataMut())[1] = 2.0f;
    Value r = numkit::lang::typecast(v, "uint8", mr);
    EXPECT_EQ(r.numel(), 8u);
    // 1.0f → 0x3F800000 little-endian → bytes 00 00 80 3F
    EXPECT_EQ(r.elemAsDouble(2), 128.0);
    EXPECT_EQ(r.elemAsDouble(3),  63.0);
    // 2.0f → 0x40000000 → bytes 00 00 00 40
    EXPECT_EQ(r.elemAsDouble(7),  64.0);
}

TEST(BuiltinTypesPublicApi, TypecastMisalignedSizeThrows)
{
    auto *mr = std::pmr::get_default_resource();
    // 3 uint8 = 3 bytes; cannot reinterpret as uint16 (needs multiple of 2).
    auto v = Value::matrix(1, 3, numkit::ValueType::UINT8, mr);
    EXPECT_THROW(numkit::lang::typecast(v, "uint16", mr), numkit::Error);
}

TEST(BuiltinTypesPublicApi, TypecastSameTypeIsIdentity)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::lang::int32(Value::scalar(42.0, mr), mr);
    Value r = numkit::lang::typecast(v, "int32", mr);
    EXPECT_EQ(r.numel(), 1u);
    EXPECT_EQ(r.elemAsDouble(0), 42.0);
}
