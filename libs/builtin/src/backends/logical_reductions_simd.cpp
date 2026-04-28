// libs/builtin/src/backends/logical_reductions_simd.cpp
//
// Highway dynamic-dispatch any() / all() with early-exit. Replaces the
// applyAlongDim+promoteToDouble path that used to dominate the bench
// (320 ms / 339 ms for 1M doubles → see project_perf_optimization_plan.md
// Phase P1).
//
// Three wins over the old path:
//   1. LOGICAL input is scanned as bytes (32 elems/AVX2 vector vs 4
//      doubles), no LOGICAL→DOUBLE promote pass.
//   2. Vector / dim=1 (column) input is contiguous → SIMD scan with
//      `AllFalse(Ne(v, 0))` decides 4-vector chunks at once.
//   3. Early-exit: any() returns at the first vector containing a
//      non-zero; all() returns at the first vector containing a zero.
//
// Scalar fallback for dim=2/3 (strided across column-major / page
// strides) — gather isn't a win here and the slice is usually small.

#include <numkit/builtin/lang/arrays/matrix.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include "../helpers.hpp"
#include "../reduction_helpers.hpp"

#include <complex>
#include <cstddef>
#include <cstdint>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/logical_reductions_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// ── SIMD scans (unit-stride only) ───────────────────────────────────────
// Each scans contiguous `n` elems and returns the boolean answer for
// any() / all(). 4× unrolled to keep load + compare in flight while the
// branch predictor handles the per-block exit test.

bool AnyDoubleScan(const double *HWY_RESTRICT p, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto zero = hn::Zero(d);

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto m0 = hn::Ne(hn::LoadU(d, p + i + 0 * N), zero);
        const auto m1 = hn::Ne(hn::LoadU(d, p + i + 1 * N), zero);
        const auto m2 = hn::Ne(hn::LoadU(d, p + i + 2 * N), zero);
        const auto m3 = hn::Ne(hn::LoadU(d, p + i + 3 * N), zero);
        const auto m  = hn::Or(hn::Or(m0, m1), hn::Or(m2, m3));
        if (!hn::AllFalse(d, m)) return true;
    }
    for (; i + N <= n; i += N) {
        if (!hn::AllFalse(d, hn::Ne(hn::LoadU(d, p + i), zero))) return true;
    }
    for (; i < n; ++i) if (p[i] != 0.0) return true;
    return false;
}

bool AllDoubleScan(const double *HWY_RESTRICT p, std::size_t n)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);
    const auto zero = hn::Zero(d);

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto m0 = hn::Eq(hn::LoadU(d, p + i + 0 * N), zero);
        const auto m1 = hn::Eq(hn::LoadU(d, p + i + 1 * N), zero);
        const auto m2 = hn::Eq(hn::LoadU(d, p + i + 2 * N), zero);
        const auto m3 = hn::Eq(hn::LoadU(d, p + i + 3 * N), zero);
        const auto m  = hn::Or(hn::Or(m0, m1), hn::Or(m2, m3));
        if (!hn::AllFalse(d, m)) return false;
    }
    for (; i + N <= n; i += N) {
        if (!hn::AllFalse(d, hn::Eq(hn::LoadU(d, p + i), zero))) return false;
    }
    for (; i < n; ++i) if (p[i] == 0.0) return false;
    return true;
}

bool AnyByteScan(const uint8_t *HWY_RESTRICT p, std::size_t n)
{
    const hn::ScalableTag<uint8_t> d;
    const std::size_t N = hn::Lanes(d);
    const auto zero = hn::Zero(d);

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto m0 = hn::Ne(hn::LoadU(d, p + i + 0 * N), zero);
        const auto m1 = hn::Ne(hn::LoadU(d, p + i + 1 * N), zero);
        const auto m2 = hn::Ne(hn::LoadU(d, p + i + 2 * N), zero);
        const auto m3 = hn::Ne(hn::LoadU(d, p + i + 3 * N), zero);
        const auto m  = hn::Or(hn::Or(m0, m1), hn::Or(m2, m3));
        if (!hn::AllFalse(d, m)) return true;
    }
    for (; i + N <= n; i += N) {
        if (!hn::AllFalse(d, hn::Ne(hn::LoadU(d, p + i), zero))) return true;
    }
    for (; i < n; ++i) if (p[i] != 0) return true;
    return false;
}

bool AllByteScan(const uint8_t *HWY_RESTRICT p, std::size_t n)
{
    const hn::ScalableTag<uint8_t> d;
    const std::size_t N = hn::Lanes(d);
    const auto zero = hn::Zero(d);

    std::size_t i = 0;
    for (; i + 4 * N <= n; i += 4 * N) {
        const auto m0 = hn::Eq(hn::LoadU(d, p + i + 0 * N), zero);
        const auto m1 = hn::Eq(hn::LoadU(d, p + i + 1 * N), zero);
        const auto m2 = hn::Eq(hn::LoadU(d, p + i + 2 * N), zero);
        const auto m3 = hn::Eq(hn::LoadU(d, p + i + 3 * N), zero);
        const auto m  = hn::Or(hn::Or(m0, m1), hn::Or(m2, m3));
        if (!hn::AllFalse(d, m)) return false;
    }
    for (; i + N <= n; i += N) {
        if (!hn::AllFalse(d, hn::Eq(hn::LoadU(d, p + i), zero))) return false;
    }
    for (; i < n; ++i) if (p[i] == 0) return false;
    return true;
}

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::builtin {

HWY_EXPORT(AnyDoubleScan);
HWY_EXPORT(AllDoubleScan);
HWY_EXPORT(AnyByteScan);
HWY_EXPORT(AllByteScan);

namespace {

// Strided scalar scans for dim=2 / dim=3 reductions across a column-major
// matrix. SIMD gather isn't a win for early-exit boolean reduction.
bool anyDoubleStrided(const double *p, std::size_t n, std::size_t stride) {
    for (std::size_t i = 0; i < n; ++i) if (p[i * stride] != 0.0) return true;
    return false;
}
bool allDoubleStrided(const double *p, std::size_t n, std::size_t stride) {
    for (std::size_t i = 0; i < n; ++i) if (p[i * stride] == 0.0) return false;
    return true;
}
bool anyByteStrided(const uint8_t *p, std::size_t n, std::size_t stride) {
    for (std::size_t i = 0; i < n; ++i) if (p[i * stride] != 0) return true;
    return false;
}
bool allByteStrided(const uint8_t *p, std::size_t n, std::size_t stride) {
    for (std::size_t i = 0; i < n; ++i) if (p[i * stride] == 0) return false;
    return true;
}

// Generic fallback for SINGLE / INT* / COMPLEX. MATLAB any(complex) is true
// when either real or imag is non-zero; everything else is "elem != 0".
template <bool IsAny>
uint8_t scanGeneric(const Value &x, std::size_t base, std::size_t n, std::size_t stride)
{
    if (x.isComplex()) {
        const auto *p = x.complexData();
        for (std::size_t i = 0; i < n; ++i) {
            const auto z = p[base + i * stride];
            const bool nz = z.real() != 0.0 || z.imag() != 0.0;
            if constexpr (IsAny) { if (nz)  return 1; }
            else                 { if (!nz) return 0; }
        }
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            const bool nz = x.elemAsDouble(base + i * stride) != 0.0;
            if constexpr (IsAny) { if (nz)  return 1; }
            else                 { if (!nz) return 0; }
        }
    }
    return IsAny ? 0 : 1;
}

template <bool IsAny>
uint8_t scanSlice(const Value &x, std::size_t base, std::size_t n, std::size_t stride)
{
    if (n == 0) return IsAny ? 0 : 1; // any([]) = false, all([]) = true
    if (x.type() == ValueType::DOUBLE) {
        const double *p = x.doubleData() + base;
        if (stride == 1) {
            if constexpr (IsAny) return HWY_DYNAMIC_DISPATCH(AnyDoubleScan)(p, n) ? 1 : 0;
            else                 return HWY_DYNAMIC_DISPATCH(AllDoubleScan)(p, n) ? 1 : 0;
        }
        if constexpr (IsAny) return anyDoubleStrided(p, n, stride) ? 1 : 0;
        else                 return allDoubleStrided(p, n, stride) ? 1 : 0;
    }
    if (x.type() == ValueType::LOGICAL) {
        const uint8_t *p = x.logicalData() + base;
        if (stride == 1) {
            if constexpr (IsAny) return HWY_DYNAMIC_DISPATCH(AnyByteScan)(p, n) ? 1 : 0;
            else                 return HWY_DYNAMIC_DISPATCH(AllByteScan)(p, n) ? 1 : 0;
        }
        if constexpr (IsAny) return anyByteStrided(p, n, stride) ? 1 : 0;
        else                 return allByteStrided(p, n, stride) ? 1 : 0;
    }
    return scanGeneric<IsAny>(x, base, n, stride);
}

template <bool IsAny>
Value logicalReduceImpl(Allocator &alloc, const Value &x, int dim)
{
    if (x.isEmpty())
        return Value::logicalScalar(IsAny ? false : true, &alloc);
    if (x.isScalar()) {
        const bool nz = x.isComplex()
            ? (x.toComplex().real() != 0.0 || x.toComplex().imag() != 0.0)
            : (x.elemAsDouble(0) != 0.0);
        return Value::logicalScalar(nz, &alloc);
    }
    if (x.dims().isVector()) {
        const uint8_t v = scanSlice<IsAny>(x, 0, x.numel(), 1);
        return Value::logicalScalar(v != 0, &alloc);
    }

    const int d = detail::resolveDim(x, dim, IsAny ? "any" : "all");
    const auto &dd = x.dims();

    // ND fallback: rank ≥ 4 — use stride arithmetic.
    if (dd.ndim() >= 4 && d >= 1 && d <= dd.ndim()) {
        auto shape = detail::outShapeForDimND(x, d);
        Value out = Value::matrixND(shape.data(),
                                      static_cast<int>(shape.size()),
                                      ValueType::LOGICAL, &alloc);
        uint8_t *dst = out.logicalDataMut();
        const std::size_t sliceLen = dd.dim(d - 1);
        std::size_t B = 1;
        for (int i = 0; i < d - 1; ++i) B *= dd.dim(i);
        std::size_t O = 1;
        for (int i = d; i < dd.ndim(); ++i) O *= dd.dim(i);
        for (std::size_t o = 0; o < O; ++o)
            for (std::size_t b = 0; b < B; ++b) {
                const std::size_t base = o * sliceLen * B + b;
                dst[o * B + b] = scanSlice<IsAny>(x, base, sliceLen, B);
            }
        return out;
    }

    const auto outShape = detail::outShapeForDim(x, d);
    Value out = createMatrix(outShape, ValueType::LOGICAL, &alloc);
    uint8_t *dst = out.logicalDataMut();

    const std::size_t R = dd.rows(), C = dd.cols(), P = dd.is3D() ? dd.pages() : 1;

    if (d == 1) {
        std::size_t outIdx = 0;
        for (std::size_t pp = 0; pp < P; ++pp)
            for (std::size_t c = 0; c < C; ++c) {
                const std::size_t base = pp * R * C + c * R;
                dst[outIdx++] = scanSlice<IsAny>(x, base, R, 1);
            }
    } else if (d == 2) {
        std::size_t outIdx = 0;
        for (std::size_t pp = 0; pp < P; ++pp)
            for (std::size_t r = 0; r < R; ++r) {
                const std::size_t base = pp * R * C + r;
                dst[outIdx++] = scanSlice<IsAny>(x, base, C, R);
            }
    } else if (d == 3) {
        std::size_t outIdx = 0;
        for (std::size_t c = 0; c < C; ++c)
            for (std::size_t r = 0; r < R; ++r) {
                const std::size_t base = c * R + r;
                dst[outIdx++] = scanSlice<IsAny>(x, base, P, R * C);
            }
    }
    return out;
}

} // namespace

Value anyOf(Allocator &alloc, const Value &x, int dim)
{
    return logicalReduceImpl<true>(alloc, x, dim);
}

Value allOf(Allocator &alloc, const Value &x, int dim)
{
    return logicalReduceImpl<false>(alloc, x, dim);
}

} // namespace numkit::builtin
#endif // HWY_ONCE
