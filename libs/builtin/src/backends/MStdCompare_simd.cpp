// libs/builtin/src/backends/MStdCompare_simd.cpp
//
// Highway dynamic-dispatch SIMD comparisons (==, ~=, <, >, <=, >=) for
// DOUBLE × DOUBLE inputs. Phase P1.5 — surfaced as the residual bottleneck
// after Phase P1's any/all SIMD landed (see project_perf_optimization_plan.md).
//
// Three cases per op (vector vs vector / vector vs scalar / scalar vs vector).
// The kernel writes 1 LOGICAL byte per double lane: comparison → mask →
// `StoreMaskBits` packs lanes into bits → small bit-extract loop expands
// to bytes. For double, lane count N ≤ 8 (AVX-512), so the bit-expand
// loop is fully unrolled by the compiler.
//
// Anything that isn't pure DOUBLE × DOUBLE (LOGICAL, integer, single,
// complex, broadcast across mismatched non-scalar shapes) falls through
// to the generic scalar `compareImpl` in MStdBinaryOps.cpp.

#include "MStdCompare.hpp"

#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include "../MStdHelpers.hpp"

#include <cstddef>
#include <cstdint>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/MStdCompare_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::m::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// One SIMD vector → up-to-8 mask bits → up-to-8 output bytes. For double
// `Lanes(d)` is at most 8 across all targets we support, so a single byte
// of bit-storage is always sufficient and the unrolled inner loop emits
// straight-line stores after `-O2`.
#define NK_CMP_LOOP(NAME, VEC_OP, SCALAR_OP)                                            \
    void NAME##VV(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,           \
                  uint8_t *HWY_RESTRICT out, std::size_t n)                             \
    {                                                                                   \
        const hn::ScalableTag<double> d;                                                \
        const std::size_t N = hn::Lanes(d);                                             \
        std::size_t i = 0;                                                              \
        for (; i + N <= n; i += N) {                                                    \
            const auto m = hn::VEC_OP(hn::LoadU(d, a + i), hn::LoadU(d, b + i));        \
            uint8_t bits[8] = {};                                                       \
            hn::StoreMaskBits(d, m, bits);                                              \
            for (std::size_t k = 0; k < N; ++k)                                         \
                out[i + k] = static_cast<uint8_t>((bits[0] >> k) & 1);                  \
        }                                                                               \
        for (; i < n; ++i)                                                              \
            out[i] = (a[i] SCALAR_OP b[i]) ? 1 : 0;                                     \
    }                                                                                   \
    void NAME##VS(const double *HWY_RESTRICT a, double s,                               \
                  uint8_t *HWY_RESTRICT out, std::size_t n)                             \
    {                                                                                   \
        const hn::ScalableTag<double> d;                                                \
        const std::size_t N = hn::Lanes(d);                                             \
        const auto sv = hn::Set(d, s);                                                  \
        std::size_t i = 0;                                                              \
        for (; i + N <= n; i += N) {                                                    \
            const auto m = hn::VEC_OP(hn::LoadU(d, a + i), sv);                         \
            uint8_t bits[8] = {};                                                       \
            hn::StoreMaskBits(d, m, bits);                                              \
            for (std::size_t k = 0; k < N; ++k)                                         \
                out[i + k] = static_cast<uint8_t>((bits[0] >> k) & 1);                  \
        }                                                                               \
        for (; i < n; ++i)                                                              \
            out[i] = (a[i] SCALAR_OP s) ? 1 : 0;                                        \
    }                                                                                   \
    void NAME##SV(double s, const double *HWY_RESTRICT b,                               \
                  uint8_t *HWY_RESTRICT out, std::size_t n)                             \
    {                                                                                   \
        const hn::ScalableTag<double> d;                                                \
        const std::size_t N = hn::Lanes(d);                                             \
        const auto sv = hn::Set(d, s);                                                  \
        std::size_t i = 0;                                                              \
        for (; i + N <= n; i += N) {                                                    \
            const auto m = hn::VEC_OP(sv, hn::LoadU(d, b + i));                         \
            uint8_t bits[8] = {};                                                       \
            hn::StoreMaskBits(d, m, bits);                                              \
            for (std::size_t k = 0; k < N; ++k)                                         \
                out[i + k] = static_cast<uint8_t>((bits[0] >> k) & 1);                  \
        }                                                                               \
        for (; i < n; ++i)                                                              \
            out[i] = (s SCALAR_OP b[i]) ? 1 : 0;                                        \
    }

NK_CMP_LOOP(EqLoop, Eq, ==)
NK_CMP_LOOP(NeLoop, Ne, !=)
NK_CMP_LOOP(LtLoop, Lt, < )
NK_CMP_LOOP(GtLoop, Gt, > )
NK_CMP_LOOP(LeLoop, Le, <=)
NK_CMP_LOOP(GeLoop, Ge, >=)

#undef NK_CMP_LOOP

} // namespace HWY_NAMESPACE
} // namespace numkit::m::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::m::builtin {

#define NK_CMP_EXPORT(NAME) \
    HWY_EXPORT(NAME##VV);   \
    HWY_EXPORT(NAME##VS);   \
    HWY_EXPORT(NAME##SV);

NK_CMP_EXPORT(EqLoop)
NK_CMP_EXPORT(NeLoop)
NK_CMP_EXPORT(LtLoop)
NK_CMP_EXPORT(GtLoop)
NK_CMP_EXPORT(LeLoop)
NK_CMP_EXPORT(GeLoop)

#undef NK_CMP_EXPORT

namespace {

// Fast-path classifier — true if SIMD path applies: both inputs DOUBLE
// (or DOUBLE scalar), no complex / logical / integer / single / char /
// string. The DOUBLE-scalar test catches `MValue::scalar(0.0)` literals.
bool isDoubleish(const MValue &v)
{
    if (v.type() != MType::DOUBLE) return false;
    if (v.isComplex())             return false;
    return true;
}

// Compatible shape: identical dims OR at least one operand is scalar.
// For mismatched non-scalar dims (row × col broadcast etc.), let the
// scalar `compareImpl` handle it.
bool compatibleShape(const MValue &a, const MValue &b)
{
    if (a.isScalar() || b.isScalar()) return true;
    return a.dims() == b.dims();
}

template <typename FnVV, typename FnVS, typename FnSV>
MValue dispatchFast(const MValue &a, const MValue &b,
                    FnVV fnVV, FnVS fnVS, FnSV fnSV)
{
    if (!isDoubleish(a) || !isDoubleish(b)) return MValue{};
    if (!compatibleShape(a, b))             return MValue{};

    if (a.isScalar() && b.isScalar()) {
        // Two scalars — let the existing tiny scalar path own this; not
        // worth the SIMD setup overhead. Returning unset re-routes.
        return MValue{};
    }

    if (a.isScalar()) {
        const double s = a.toScalar();
        auto r = createLike(b, MType::LOGICAL, nullptr);
        fnSV(s, b.doubleData(), r.logicalDataMut(), b.numel());
        return r;
    }
    if (b.isScalar()) {
        const double s = b.toScalar();
        auto r = createLike(a, MType::LOGICAL, nullptr);
        fnVS(a.doubleData(), s, r.logicalDataMut(), a.numel());
        return r;
    }
    // Same-shape array × array.
    auto r = createLike(a, MType::LOGICAL, nullptr);
    fnVV(a.doubleData(), b.doubleData(), r.logicalDataMut(), a.numel());
    return r;
}

} // namespace

#define NK_CMP_PUBLIC(FN, NAME)                                                  \
    MValue FN(const MValue &a, const MValue &b)                                  \
    {                                                                            \
        return dispatchFast(a, b,                                                \
            HWY_DYNAMIC_DISPATCH(NAME##VV),                                      \
            HWY_DYNAMIC_DISPATCH(NAME##VS),                                      \
            HWY_DYNAMIC_DISPATCH(NAME##SV));                                     \
    }

NK_CMP_PUBLIC(eqFast, EqLoop)
NK_CMP_PUBLIC(neFast, NeLoop)
NK_CMP_PUBLIC(ltFast, LtLoop)
NK_CMP_PUBLIC(gtFast, GtLoop)
NK_CMP_PUBLIC(leFast, LeLoop)
NK_CMP_PUBLIC(geFast, GeLoop)

#undef NK_CMP_PUBLIC

} // namespace numkit::m::builtin
#endif // HWY_ONCE
