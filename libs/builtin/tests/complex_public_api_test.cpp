// libs/builtin/tests/complex_public_api_test.cpp
//
// Direct-call tests for numkit::builtin::{real, imag, conj, complex, angle}.
// Exercises the algorithm without going through Engine, Parser, VM, or the
// registration adapter.

#include <numkit/builtin/math/elementary/complex.hpp>

#include <memory_resource>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using numkit::Error;
using numkit::ValueType;
using numkit::Value;
using Complex = std::complex<double>;

namespace {

Value makeComplexRow(std::pmr::memory_resource *mr, std::initializer_list<Complex> vals)
{
    auto v = Value::complexMatrix(1, vals.size(), mr);
    Complex *data = v.complexDataMut();
    size_t i = 0;
    for (const Complex &c : vals)
        data[i++] = c;
    return v;
}

Value makeRealRow(std::pmr::memory_resource *mr, std::initializer_list<double> vals)
{
    auto v = Value::matrix(1, vals.size(), ValueType::DOUBLE, mr);
    double *data = v.doubleDataMut();
    size_t i = 0;
    for (double x : vals)
        data[i++] = x;
    return v;
}

} // namespace

// ── real ────────────────────────────────────────────────────────────────
TEST(BuiltinComplexPublicApi, RealOfComplexScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = Value::complexScalar(Complex(3.0, 4.0), mr);
    Value r = numkit::builtin::real(mr, x);
    ASSERT_TRUE(r.isScalar());
    EXPECT_DOUBLE_EQ(r.toScalar(), 3.0);
}

TEST(BuiltinComplexPublicApi, RealOfComplexVector)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = makeComplexRow(mr, {Complex(1, 10), Complex(2, 20), Complex(3, 30)});
    Value r = numkit::builtin::real(mr, x);
    ASSERT_EQ(r.type(), ValueType::DOUBLE);
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_DOUBLE_EQ(r.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r.doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(r.doubleData()[2], 3.0);
}

TEST(BuiltinComplexPublicApi, RealOfRealIsPassThrough)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = Value::scalar(5.0, mr);
    Value r = numkit::builtin::real(mr, x);
    EXPECT_DOUBLE_EQ(r.toScalar(), 5.0);
}

// ── imag ────────────────────────────────────────────────────────────────
TEST(BuiltinComplexPublicApi, ImagOfComplexScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = Value::complexScalar(Complex(3.0, 4.0), mr);
    Value r = numkit::builtin::imag(mr, x);
    EXPECT_DOUBLE_EQ(r.toScalar(), 4.0);
}

TEST(BuiltinComplexPublicApi, ImagOfRealReturnsZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = Value::scalar(7.0, mr);
    Value r = numkit::builtin::imag(mr, x);
    EXPECT_DOUBLE_EQ(r.toScalar(), 0.0);
}

// ── conj ────────────────────────────────────────────────────────────────
TEST(BuiltinComplexPublicApi, ConjFlipsImagSign)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = makeComplexRow(mr, {Complex(1, 2), Complex(-3, 4), Complex(0, -5)});
    Value r = numkit::builtin::conj(mr, x);
    ASSERT_TRUE(r.isComplex());
    const Complex *c = r.complexData();
    EXPECT_EQ(c[0], Complex(1, -2));
    EXPECT_EQ(c[1], Complex(-3, -4));
    EXPECT_EQ(c[2], Complex(0, 5));
}

TEST(BuiltinComplexPublicApi, ConjOfRealIsPassThrough)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = makeRealRow(mr, {1.0, 2.0, 3.0});
    Value r = numkit::builtin::conj(mr, x);
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_DOUBLE_EQ(r.doubleData()[0], 1.0);
}

// ── complex ─────────────────────────────────────────────────────────────
TEST(BuiltinComplexPublicApi, ComplexOneArgAddsZeroImag)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value re = Value::scalar(3.0, mr);
    Value r = numkit::builtin::complex(mr, re);
    ASSERT_TRUE(r.isComplex());
    EXPECT_EQ(r.toComplex(), Complex(3.0, 0.0));
}

TEST(BuiltinComplexPublicApi, ComplexTwoArgsScalar)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value re = Value::scalar(3.0, mr);
    Value im = Value::scalar(4.0, mr);
    Value r = numkit::builtin::complex(mr, re, im);
    EXPECT_EQ(r.toComplex(), Complex(3.0, 4.0));
}

TEST(BuiltinComplexPublicApi, ComplexBroadcastsScalarImag)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value re = makeRealRow(mr, {1.0, 2.0, 3.0});
    Value im = Value::scalar(10.0, mr);
    Value r = numkit::builtin::complex(mr, re, im);
    ASSERT_TRUE(r.isComplex());
    ASSERT_EQ(r.numel(), 3u);
    const Complex *c = r.complexData();
    EXPECT_EQ(c[0], Complex(1, 10));
    EXPECT_EQ(c[1], Complex(2, 10));
    EXPECT_EQ(c[2], Complex(3, 10));
}

TEST(BuiltinComplexPublicApi, ComplexMismatchedDimsThrows)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value re = makeRealRow(mr, {1.0, 2.0, 3.0});
    Value im = makeRealRow(mr, {1.0, 2.0});
    EXPECT_THROW(numkit::builtin::complex(mr, re, im), Error);
}

// ── angle ───────────────────────────────────────────────────────────────
TEST(BuiltinComplexPublicApi, AngleOfUnitImag)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = Value::complexScalar(Complex(0, 1), mr);
    Value r = numkit::builtin::angle(mr, x);
    EXPECT_NEAR(r.toScalar(), M_PI / 2.0, 1e-12);
}

TEST(BuiltinComplexPublicApi, AngleOfNegativeRealIsPi)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = Value::scalar(-1.0, mr);
    Value r = numkit::builtin::angle(mr, x);
    EXPECT_NEAR(r.toScalar(), M_PI, 1e-12);
}

TEST(BuiltinComplexPublicApi, AngleOfPositiveRealIsZero)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = Value::scalar(5.0, mr);
    Value r = numkit::builtin::angle(mr, x);
    EXPECT_NEAR(r.toScalar(), 0.0, 1e-12);
}

TEST(BuiltinComplexPublicApi, AngleOfComplexVector)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value x = makeComplexRow(mr, {Complex(1, 0), Complex(0, 1), Complex(-1, 0)});
    Value r = numkit::builtin::angle(mr, x);
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_NEAR(r.doubleData()[0], 0.0, 1e-12);
    EXPECT_NEAR(r.doubleData()[1], M_PI / 2.0, 1e-12);
    EXPECT_NEAR(r.doubleData()[2], M_PI, 1e-12);
}
