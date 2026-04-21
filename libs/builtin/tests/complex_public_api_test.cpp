// libs/builtin/tests/complex_public_api_test.cpp
//
// Direct-call tests for numkit::m::builtin::{real, imag, conj, complex, angle}.
// Exercises the algorithm without going through Engine, Parser, VM, or the
// registration adapter.

#include <numkit/m/builtin/MStdComplex.hpp>

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using numkit::m::Allocator;
using numkit::m::MError;
using numkit::m::MType;
using numkit::m::MValue;
using Complex = std::complex<double>;

namespace {

MValue makeComplexRow(Allocator &alloc, std::initializer_list<Complex> vals)
{
    auto v = MValue::complexMatrix(1, vals.size(), &alloc);
    Complex *data = v.complexDataMut();
    size_t i = 0;
    for (const Complex &c : vals)
        data[i++] = c;
    return v;
}

MValue makeRealRow(Allocator &alloc, std::initializer_list<double> vals)
{
    auto v = MValue::matrix(1, vals.size(), MType::DOUBLE, &alloc);
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
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = MValue::complexScalar(Complex(3.0, 4.0), &alloc);
    MValue r = numkit::m::builtin::real(alloc, x);
    ASSERT_TRUE(r.isScalar());
    EXPECT_DOUBLE_EQ(r.toScalar(), 3.0);
}

TEST(BuiltinComplexPublicApi, RealOfComplexVector)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeComplexRow(alloc, {Complex(1, 10), Complex(2, 20), Complex(3, 30)});
    MValue r = numkit::m::builtin::real(alloc, x);
    ASSERT_EQ(r.type(), MType::DOUBLE);
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_DOUBLE_EQ(r.doubleData()[0], 1.0);
    EXPECT_DOUBLE_EQ(r.doubleData()[1], 2.0);
    EXPECT_DOUBLE_EQ(r.doubleData()[2], 3.0);
}

TEST(BuiltinComplexPublicApi, RealOfRealIsPassThrough)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = MValue::scalar(5.0, &alloc);
    MValue r = numkit::m::builtin::real(alloc, x);
    EXPECT_DOUBLE_EQ(r.toScalar(), 5.0);
}

// ── imag ────────────────────────────────────────────────────────────────
TEST(BuiltinComplexPublicApi, ImagOfComplexScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = MValue::complexScalar(Complex(3.0, 4.0), &alloc);
    MValue r = numkit::m::builtin::imag(alloc, x);
    EXPECT_DOUBLE_EQ(r.toScalar(), 4.0);
}

TEST(BuiltinComplexPublicApi, ImagOfRealReturnsZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = MValue::scalar(7.0, &alloc);
    MValue r = numkit::m::builtin::imag(alloc, x);
    EXPECT_DOUBLE_EQ(r.toScalar(), 0.0);
}

// ── conj ────────────────────────────────────────────────────────────────
TEST(BuiltinComplexPublicApi, ConjFlipsImagSign)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeComplexRow(alloc, {Complex(1, 2), Complex(-3, 4), Complex(0, -5)});
    MValue r = numkit::m::builtin::conj(alloc, x);
    ASSERT_TRUE(r.isComplex());
    const Complex *c = r.complexData();
    EXPECT_EQ(c[0], Complex(1, -2));
    EXPECT_EQ(c[1], Complex(-3, -4));
    EXPECT_EQ(c[2], Complex(0, 5));
}

TEST(BuiltinComplexPublicApi, ConjOfRealIsPassThrough)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeRealRow(alloc, {1.0, 2.0, 3.0});
    MValue r = numkit::m::builtin::conj(alloc, x);
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_DOUBLE_EQ(r.doubleData()[0], 1.0);
}

// ── complex ─────────────────────────────────────────────────────────────
TEST(BuiltinComplexPublicApi, ComplexOneArgAddsZeroImag)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue re = MValue::scalar(3.0, &alloc);
    MValue r = numkit::m::builtin::complex(alloc, re);
    ASSERT_TRUE(r.isComplex());
    EXPECT_EQ(r.toComplex(), Complex(3.0, 0.0));
}

TEST(BuiltinComplexPublicApi, ComplexTwoArgsScalar)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue re = MValue::scalar(3.0, &alloc);
    MValue im = MValue::scalar(4.0, &alloc);
    MValue r = numkit::m::builtin::complex(alloc, re, im);
    EXPECT_EQ(r.toComplex(), Complex(3.0, 4.0));
}

TEST(BuiltinComplexPublicApi, ComplexBroadcastsScalarImag)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue re = makeRealRow(alloc, {1.0, 2.0, 3.0});
    MValue im = MValue::scalar(10.0, &alloc);
    MValue r = numkit::m::builtin::complex(alloc, re, im);
    ASSERT_TRUE(r.isComplex());
    ASSERT_EQ(r.numel(), 3u);
    const Complex *c = r.complexData();
    EXPECT_EQ(c[0], Complex(1, 10));
    EXPECT_EQ(c[1], Complex(2, 10));
    EXPECT_EQ(c[2], Complex(3, 10));
}

TEST(BuiltinComplexPublicApi, ComplexMismatchedDimsThrows)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue re = makeRealRow(alloc, {1.0, 2.0, 3.0});
    MValue im = makeRealRow(alloc, {1.0, 2.0});
    EXPECT_THROW(numkit::m::builtin::complex(alloc, re, im), MError);
}

// ── angle ───────────────────────────────────────────────────────────────
TEST(BuiltinComplexPublicApi, AngleOfUnitImag)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = MValue::complexScalar(Complex(0, 1), &alloc);
    MValue r = numkit::m::builtin::angle(alloc, x);
    EXPECT_NEAR(r.toScalar(), M_PI / 2.0, 1e-12);
}

TEST(BuiltinComplexPublicApi, AngleOfNegativeRealIsPi)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = MValue::scalar(-1.0, &alloc);
    MValue r = numkit::m::builtin::angle(alloc, x);
    EXPECT_NEAR(r.toScalar(), M_PI, 1e-12);
}

TEST(BuiltinComplexPublicApi, AngleOfPositiveRealIsZero)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = MValue::scalar(5.0, &alloc);
    MValue r = numkit::m::builtin::angle(alloc, x);
    EXPECT_NEAR(r.toScalar(), 0.0, 1e-12);
}

TEST(BuiltinComplexPublicApi, AngleOfComplexVector)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue x = makeComplexRow(alloc, {Complex(1, 0), Complex(0, 1), Complex(-1, 0)});
    MValue r = numkit::m::builtin::angle(alloc, x);
    ASSERT_EQ(r.numel(), 3u);
    EXPECT_NEAR(r.doubleData()[0], 0.0, 1e-12);
    EXPECT_NEAR(r.doubleData()[1], M_PI / 2.0, 1e-12);
    EXPECT_NEAR(r.doubleData()[2], M_PI, 1e-12);
}
