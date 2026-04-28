// libs/builtin/src/lang/operators/backends/compare_simd.cpp
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
// to the generic scalar `compareImpl` in binary_ops.cpp.

#include "compare.hpp"

#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include "helpers.hpp"

#include <cstddef>
#include <cstdint>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backends/compare_simd.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::builtin {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

// IEEE-correct `!=` for SIMD: Highway's `Ne` lowers to `_CMP_NEQ_OQ`
// (ordered) which returns false for NaN-vs-anything — the opposite of
// C++/IEEE `a != b` which is true for NaN. `Not(Eq(a, b))` gets it
// right: `Eq` is also ordered (false for NaN-NaN), so `Not(false) =
// true`, matching IEEE. Used only by the Ne loop below; the other 5
// ops are correct under Highway's ordered semantics (NaN comparison =
// false matches the C++ scalar baseline for <, >, <=, >=, ==).
#define NK_IEEE_NE(d, a, b) hn::Not(hn::Eq((a), (b)))

// One SIMD vector → up-to-8 mask bits → up-to-8 output bytes. For double
// `Lanes(d)` is at most 8 across all targets we support, so a single byte
// of bit-storage is always sufficient and the unrolled inner loop emits
// straight-line stores after `-O2`.
// CMP_EXPR(d, va, vb) is an expression that evaluates to a Mask. For
// most ops it's `hn::Lt(va, vb)` / `hn::Gt(...)` etc.; for Ne it's
// `NK_IEEE_NE(d, va, vb)` to preserve IEEE NaN semantics.
#define NK_CMP_LOOP(NAME, CMP_EXPR, SCALAR_OP)                                          \
    void NAME##VV(const double *HWY_RESTRICT a, const double *HWY_RESTRICT b,           \
                  uint8_t *HWY_RESTRICT out, std::size_t n)                             \
    {                                                                                   \
        const hn::ScalableTag<double> d;                                                \
        const std::size_t N = hn::Lanes(d);                                             \
        std::size_t i = 0;                                                              \
        for (; i + N <= n; i += N) {                                                    \
            const auto va = hn::LoadU(d, a + i);                                        \
            const auto vb = hn::LoadU(d, b + i);                                        \
            const auto m = CMP_EXPR(d, va, vb);                                         \
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
            const auto va = hn::LoadU(d, a + i);                                        \
            const auto m = CMP_EXPR(d, va, sv);                                         \
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
            const auto vb = hn::LoadU(d, b + i);                                        \
            const auto m = CMP_EXPR(d, sv, vb);                                         \
            uint8_t bits[8] = {};                                                       \
            hn::StoreMaskBits(d, m, bits);                                              \
            for (std::size_t k = 0; k < N; ++k)                                         \
                out[i + k] = static_cast<uint8_t>((bits[0] >> k) & 1);                  \
        }                                                                               \
        for (; i < n; ++i)                                                              \
            out[i] = (s SCALAR_OP b[i]) ? 1 : 0;                                        \
    }

#define NK_VEC_EQ(d, a, b) hn::Eq((a), (b))
#define NK_VEC_LT(d, a, b) hn::Lt((a), (b))
#define NK_VEC_GT(d, a, b) hn::Gt((a), (b))
#define NK_VEC_LE(d, a, b) hn::Le((a), (b))
#define NK_VEC_GE(d, a, b) hn::Ge((a), (b))

NK_CMP_LOOP(EqLoop, NK_VEC_EQ,  ==)
NK_CMP_LOOP(NeLoop, NK_IEEE_NE, !=)
NK_CMP_LOOP(LtLoop, NK_VEC_LT,  < )
NK_CMP_LOOP(GtLoop, NK_VEC_GT,  > )
NK_CMP_LOOP(LeLoop, NK_VEC_LE,  <=)
NK_CMP_LOOP(GeLoop, NK_VEC_GE,  >=)

#undef NK_CMP_LOOP
#undef NK_VEC_EQ
#undef NK_VEC_LT
#undef NK_VEC_GT
#undef NK_VEC_LE
#undef NK_VEC_GE
#undef NK_IEEE_NE

} // namespace HWY_NAMESPACE
} // namespace numkit::builtin
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::builtin {

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
// string. The DOUBLE-scalar test catches `Value::scalar(0.0)` literals.
bool isDoubleish(const Value &v)
{
    if (v.type() != ValueType::DOUBLE) return false;
    if (v.isComplex())             return false;
    return true;
}

// Compatible shape: identical dims OR at least one operand is scalar.
// For mismatched non-scalar dims (row × col broadcast etc.), let the
// scalar `compareImpl` handle it.
bool compatibleShape(const Value &a, const Value &b)
{
    if (a.isScalar() || b.isScalar()) return true;
    return a.dims() == b.dims();
}

template <typename FnVV, typename FnVS, typename FnSV>
Value dispatchFast(const Value &a, const Value &b,
                    FnVV fnVV, FnVS fnVS, FnSV fnSV)
{
    if (!isDoubleish(a) || !isDoubleish(b)) return Value{};
    if (!compatibleShape(a, b))             return Value{};

    if (a.isScalar() && b.isScalar()) {
        // Two scalars — let the existing tiny scalar path own this; not
        // worth the SIMD setup overhead. Returning unset re-routes.
        return Value{};
    }

    if (a.isScalar()) {
        const double s = a.toScalar();
        auto r = createLike(b, ValueType::LOGICAL, nullptr);
        fnSV(s, b.doubleData(), r.logicalDataMut(), b.numel());
        return r;
    }
    if (b.isScalar()) {
        const double s = b.toScalar();
        auto r = createLike(a, ValueType::LOGICAL, nullptr);
        fnVS(a.doubleData(), s, r.logicalDataMut(), a.numel());
        return r;
    }
    // Same-shape array × array.
    auto r = createLike(a, ValueType::LOGICAL, nullptr);
    fnVV(a.doubleData(), b.doubleData(), r.logicalDataMut(), a.numel());
    return r;
}

} // namespace

#define NK_CMP_PUBLIC(FN, NAME)                                                  \
    Value FN(const Value &a, const Value &b)                                  \
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

} // namespace numkit::builtin
#endif // HWY_ONCE
