// libs/builtin/tests/types_public_api_test.cpp
//
// Direct-call tests for numkit::builtin type functions.

#include <numkit/builtin/language/types/types.hpp>

#include <memory_resource>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

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
    Value r = numkit::builtin::int8(mr, Value::scalar(500.0, mr));
    ASSERT_EQ(r.type(), ValueType::INT8);
    EXPECT_EQ(*static_cast<const int8_t *>(r.rawData()),
              std::numeric_limits<int8_t>::max());
}

TEST(BuiltinTypesPublicApi, Uint8SaturatesNegativeToZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::uint8(mr, Value::scalar(-7.0, mr));
    ASSERT_EQ(r.type(), ValueType::UINT8);
    EXPECT_EQ(*static_cast<const uint8_t *>(r.rawData()), 0u);
}

TEST(BuiltinTypesPublicApi, Int32RoundsToNearest)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::int32(mr, Value::scalar(3.7, mr));
    EXPECT_EQ(*static_cast<const int32_t *>(r.rawData()), 4);
}

TEST(BuiltinTypesPublicApi, Int32OfNanIsZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value nan = Value::scalar(std::nan(""), mr);
    Value r = numkit::builtin::int32(mr, nan);
    EXPECT_EQ(*static_cast<const int32_t *>(r.rawData()), 0);
}

TEST(BuiltinTypesPublicApi, SingleConvertsDoubleToFloat)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::single(mr, Value::scalar(3.14, mr));
    ASSERT_EQ(r.type(), ValueType::SINGLE);
    EXPECT_NEAR(*static_cast<const float *>(r.rawData()), 3.14f, 1e-6f);
}

TEST(BuiltinTypesPublicApi, ToDoubleFromInt32IsDoubleTyped)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value i = numkit::builtin::int32(mr, Value::scalar(5.0, mr));
    Value d = numkit::builtin::toDouble(mr, i);
    ASSERT_EQ(d.type(), ValueType::DOUBLE);
    EXPECT_DOUBLE_EQ(d.toScalar(), 5.0);
}

// ── logical ─────────────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, LogicalFromNumericNonZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::logical(mr, Value::scalar(7.5, mr));
    ASSERT_TRUE(r.isLogical());
    EXPECT_TRUE(r.toBool());
}

TEST(BuiltinTypesPublicApi, LogicalFromZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::logical(mr, Value::scalar(0.0, mr));
    EXPECT_FALSE(r.toBool());
}

// ── Type predicates ─────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, PredicatesOnDoubleScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = Value::scalar(3.14, mr);

    EXPECT_TRUE(numkit::builtin::isnumeric(mr, x).toBool());
    EXPECT_FALSE(numkit::builtin::islogical(mr, x).toBool());
    EXPECT_FALSE(numkit::builtin::ischar(mr, x).toBool());
    EXPECT_FALSE(numkit::builtin::iscell(mr, x).toBool());
    EXPECT_FALSE(numkit::builtin::isempty(mr, x).toBool());
    EXPECT_TRUE(numkit::builtin::isscalar(mr, x).toBool());
    EXPECT_TRUE(numkit::builtin::isreal(mr, x).toBool());
    EXPECT_FALSE(numkit::builtin::isinteger(mr, x).toBool());
    EXPECT_TRUE(numkit::builtin::isfloat(mr, x).toBool());
    EXPECT_FALSE(numkit::builtin::issingle(mr, x).toBool());
}

TEST(BuiltinTypesPublicApi, IsintegerAfterInt32)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value i = numkit::builtin::int32(mr, Value::scalar(5.0, mr));
    EXPECT_TRUE(numkit::builtin::isinteger(mr, i).toBool());
    EXPECT_FALSE(numkit::builtin::isfloat(mr, i).toBool());
}

TEST(BuiltinTypesPublicApi, IsemptyOnEmpty)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::isempty(mr, Value::empty()).toBool());
}

// ── isnan / isinf ───────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, IsnanScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::isnan(mr, Value::scalar(std::nan(""), mr))
                    .toBool());
    EXPECT_FALSE(numkit::builtin::isnan(mr, Value::scalar(1.0, mr)).toBool());
}

TEST(BuiltinTypesPublicApi, IsinfScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    double posInf = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(numkit::builtin::isinf(mr, Value::scalar(posInf, mr)).toBool());
    EXPECT_FALSE(numkit::builtin::isinf(mr, Value::scalar(1.0, mr)).toBool());
}

TEST(BuiltinTypesPublicApi, IsnanVectorElementwise)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    auto v = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    double *d = v.doubleDataMut();
    d[0] = 1.0; d[1] = std::nan(""); d[2] = 0.0;
    Value r = numkit::builtin::isnan(mr, v);
    ASSERT_EQ(r.type(), ValueType::LOGICAL);
    EXPECT_EQ(r.logicalData()[0], 0u);
    EXPECT_EQ(r.logicalData()[1], 1u);
    EXPECT_EQ(r.logicalData()[2], 0u);
}

// ── isequal / isequaln ──────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, IsequalMatchingScalars)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    EXPECT_TRUE(numkit::builtin::isequal(mr, Value::scalar(5.0, mr),
                                            Value::scalar(5.0, mr))
                    .toBool());
}

TEST(BuiltinTypesPublicApi, IsequalDifferentTypesReturnsFalse)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value d = Value::scalar(5.0, mr);
    Value i = numkit::builtin::int32(mr, d);
    EXPECT_FALSE(numkit::builtin::isequal(mr, d, i).toBool());
}

TEST(BuiltinTypesPublicApi, IsequalNanVsNan)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value n1 = Value::scalar(std::nan(""), mr);
    Value n2 = Value::scalar(std::nan(""), mr);
    // isequal: NaN != NaN
    EXPECT_FALSE(numkit::builtin::isequal(mr, n1, n2).toBool());
    // isequaln: NaN == NaN
    EXPECT_TRUE(numkit::builtin::isequaln(mr, n1, n2).toBool());
}

// ── class ───────────────────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, ClassOfDouble)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value r = numkit::builtin::classOf(mr, Value::scalar(1.0, mr));
    EXPECT_EQ(r.toString(), "double");
}

TEST(BuiltinTypesPublicApi, ClassOfInt32)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value i = numkit::builtin::int32(mr, Value::scalar(1.0, mr));
    Value r = numkit::builtin::classOf(mr, i);
    EXPECT_EQ(r.toString(), "int32");
}

// ── Pack 36: cast / swapbytes ────────────────────────────────────────
TEST(BuiltinTypesPublicApi, CastDispatchesByName)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = Value::scalar(3.7, mr);
    EXPECT_EQ(numkit::builtin::cast(mr, v, "int32").type(), numkit::ValueType::INT32);
    EXPECT_EQ(numkit::builtin::cast(mr, v, "uint16").type(), numkit::ValueType::UINT16);
    EXPECT_EQ(numkit::builtin::cast(mr, v, "single").type(), numkit::ValueType::SINGLE);
    EXPECT_EQ(numkit::builtin::cast(mr, v, "logical").type(), numkit::ValueType::LOGICAL);
}

TEST(BuiltinTypesPublicApi, CastIntegerSaturates)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = Value::scalar(99999.0, mr);
    Value r = numkit::builtin::cast(mr, v, "int8");
    EXPECT_EQ(r.elemAsDouble(0), 127.0);
}

TEST(BuiltinTypesPublicApi, CastBadClassThrows)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = Value::scalar(0.0, mr);
    EXPECT_THROW(numkit::builtin::cast(mr, v, "bogus"), numkit::Error);
}

TEST(BuiltinTypesPublicApi, SwapbytesUint16)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::builtin::uint16(mr, Value::scalar(258.0, mr));  // 0x0102
    Value r = numkit::builtin::swapbytes(mr, v);
    EXPECT_EQ(r.type(), numkit::ValueType::UINT16);
    EXPECT_EQ(r.elemAsDouble(0), 513.0);  // 0x0201
}

TEST(BuiltinTypesPublicApi, SwapbytesUint32)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::builtin::uint32(mr, Value::scalar(1.0, mr));
    Value r = numkit::builtin::swapbytes(mr, v);
    EXPECT_EQ(r.elemAsDouble(0), 16777216.0);  // 0x01000000
}

TEST(BuiltinTypesPublicApi, SwapbytesInvolution)
{
    auto *mr = std::pmr::get_default_resource();
    // swapbytes(swapbytes(x)) == x for every supported type.
    Value v = numkit::builtin::int32(mr, Value::scalar(0x12345678, mr));
    Value r = numkit::builtin::swapbytes(mr, numkit::builtin::swapbytes(mr, v));
    EXPECT_EQ(r.elemAsDouble(0), v.elemAsDouble(0));
}

TEST(BuiltinTypesPublicApi, SwapbytesByteWidth1IsIdentity)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::builtin::uint8(mr, Value::scalar(0x42, mr));
    Value r = numkit::builtin::swapbytes(mr, v);
    EXPECT_EQ(r.elemAsDouble(0), 0x42);
}

// ── Pack 36: typecast ────────────────────────────────────────────────
TEST(BuiltinTypesPublicApi, TypecastUint32ToUint16)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::builtin::uint32(mr, Value::scalar(258.0, mr));
    Value r = numkit::builtin::typecast(mr, v, "uint16");
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
    Value r = numkit::builtin::typecast(mr, v, "uint8");
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
    EXPECT_THROW(numkit::builtin::typecast(mr, v, "uint16"), numkit::Error);
}

TEST(BuiltinTypesPublicApi, TypecastSameTypeIsIdentity)
{
    auto *mr = std::pmr::get_default_resource();
    Value v = numkit::builtin::int32(mr, Value::scalar(42.0, mr));
    Value r = numkit::builtin::typecast(mr, v, "int32");
    EXPECT_EQ(r.numel(), 1u);
    EXPECT_EQ(r.elemAsDouble(0), 42.0);
}
