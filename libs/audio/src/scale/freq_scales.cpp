// libs/audio/src/scale/freq_scales.cpp
//
// Audio frequency-scale and loudness conversions.
//
// Each conversion is a closed-form formula implemented from its
// published source:
//   Mel:       O'Shaughnessy, "Speech Communication", 1987
//   Bark:      Traunmüller, JASA 88, 1990 (with low/high-freq corrections)
//   ERB:       Glasberg & Moore, Hearing Research 47, 1990
//              (constants 24.673 and 0.004368)
//   Phon/Sone: ISO 532-1:2017 (closed form) and ISO 532-2:2017 (Table 5)
//
// Numerical parity is checked against MATLAB R2025b in the parity
// harness (tools/parity).
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.

#include <numkit/audio/scale/freq_scales.hpp>
#include <numkit/builtin/math/interp/interp.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>

namespace numkit::audio {

namespace {

// Apply elementwise scalar fn f to input x; output same shape.
template <typename Fn>
Value elementwise(std::pmr::memory_resource *mr, const Value &x, Fn f)
{
    const size_t N = x.numel();
    Value out;
    if (x.dims().is3D())
        out = Value::matrix3d(x.dims().rows(), x.dims().cols(),
                              x.dims().pages(), ValueType::DOUBLE, mr);
    else
        out = Value::matrix(x.dims().rows(), x.dims().cols(),
                            ValueType::DOUBLE, mr);
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) od[i] = f(x.elemAsDouble(i));
    return out;
}

// ERB scale factor (Glasberg & Moore 1990): log(10) * 1000 / (24.673 * 4.368).
inline double erbScale()
{
    return std::log(10.0) * 1000.0 / (24.673 * 4.368);
}

} // namespace

// ── Mel (O'Shaughnessy default) ───────────────────────────────────────
Value hz2mel(std::pmr::memory_resource *mr, const Value &hz)
{
    return elementwise(mr, hz, [](double f) {
        return 2595.0 * std::log10(1.0 + f / 700.0);
    });
}

Value mel2hz(std::pmr::memory_resource *mr, const Value &mel)
{
    return elementwise(mr, mel, [](double m) {
        return 700.0 * (std::pow(10.0, m / 2595.0) - 1.0);
    });
}

// ── Bark (Traunmüller with corrections) ───────────────────────────────
Value hz2bark(std::pmr::memory_resource *mr, const Value &hz)
{
    return elementwise(mr, hz, [](double f) {
        double bark = 26.81 * f / (1960.0 + f) - 0.53;
        if (bark < 2.0)        bark = 0.85 * bark + 0.3;
        else if (bark > 20.1)  bark = 1.22 * bark - 4.422;
        return bark;
    });
}

Value bark2hz(std::pmr::memory_resource *mr, const Value &bark)
{
    return elementwise(mr, bark, [](double b) {
        if (b < 2.0)         b = (b - 0.3) / 0.85;
        else if (b > 20.1)   b = (b + 0.22 * 20.1) / 1.22;
        // The inverse uses 26.28 (not 26.81) in this denominator; the
        // asymmetry is intentional — it is not the exact algebraic
        // inverse of hz2bark. Parity checked against MATLAB R2025b.
        return 1960.0 * (b + 0.53) / (26.28 - b);
    });
}

// ── ERB (Glasberg-Moore) ──────────────────────────────────────────────
Value hz2erb(std::pmr::memory_resource *mr, const Value &hz)
{
    const double scale = erbScale();
    return elementwise(mr, hz, [scale](double f) {
        return scale * std::log10(1.0 + 0.004368 * f);
    });
}

Value erb2hz(std::pmr::memory_resource *mr, const Value &erb)
{
    const double scale = erbScale();
    return elementwise(mr, erb, [scale](double e) {
        return (std::pow(10.0, e / scale) - 1.0) / 0.004368;
    });
}

// ── Loudness ISO 532-1 / ISO 532-2 ────────────────────────────────────
//
// ISO 532-1 default — closed-form piecewise power law (ISO 532-1:2017):
//   sone = (phon/40)^(1/0.35)        if phon < 40
//   sone = 2^(phon/10 - 4)            otherwise
//   phon = 40 * sone^0.35             if sone < 1
//   phon = 40 + 10 * log2(sone)       otherwise
//
// ISO 532-2 alternative:
//   Uses ISO 532-2:2017 Table 5 directly via PCHIP interpolation +
//   linear extrapolation beyond 120 phon (337.6 sone). sone2phon is a
//   plain PCHIP clamped to >= 0; phon2sone uses the PCHIP value as an
//   initial guess and refines it with an inline bisection so that
//   sone2phon(phon2sone(p)) == p (both fall back to the PCHIP guess on
//   non-bracketing). Numerical parity (~1e-12) is checked against
//   MATLAB R2025b in the parity harness.

namespace {

// ISO 532-2:2017 Table 5: phon (row 0) → sone (row 1). 28 entries.
// These are the published ISO 532-2:2017 Table 5 values.
constexpr size_t kTab5N = 28;
constexpr double kTab5Phon[kTab5N] = {
    0.000, 2.200, 4.000, 5.000, 7.500, 10.00, 15.00, 20.00,
    25.0,  30.0,  35.0,  40.0,  45.0,  50.0,  55.0,  60.0,
    65.0,  70.0,  75.0,  80.0,  85.0,  90.0,  95.0,  100.0,
    105.0, 110.0, 115.0, 120.0
};
constexpr double kTab5Sone[kTab5N] = {
    0.001, 0.004, 0.008, 0.010, 0.019, 0.031, 0.073, 0.146,
    0.26,  0.43,  0.67,  1.00,  1.46,  2.09,  2.96,  4.14,
    5.77,  8.04,  11.2,  15.8,  22.7,  32.9,  47.7,  69.6,
    102.0, 151.0, 225.0, 337.6
};

// Make Value vectors backed by tab5 (column vectors for interp1 input).
Value tab5PhonVec(std::pmr::memory_resource *mr)
{
    Value v = Value::matrix(kTab5N, 1, ValueType::DOUBLE, mr);
    std::copy(kTab5Phon, kTab5Phon + kTab5N, v.doubleDataMut());
    return v;
}
Value tab5SoneVec(std::pmr::memory_resource *mr)
{
    Value v = Value::matrix(kTab5N, 1, ValueType::DOUBLE, mr);
    std::copy(kTab5Sone, kTab5Sone + kTab5N, v.doubleDataMut());
    return v;
}

// Linear extrapolation from last two points of (xs, ys) at query x.
inline double linearExtrap(const double *xs, const double *ys, size_t N, double x)
{
    const double x0 = xs[N - 2], y0 = ys[N - 2];
    const double x1 = xs[N - 1], y1 = ys[N - 1];
    const double slope = (x1 - x0 != 0.0) ? (y1 - y0) / (x1 - x0) : 0.0;
    return y1 + slope * (x - x1);
}

} // anon

Value phon2sone(std::pmr::memory_resource *mr, const Value &phon,
                bool standardIs532_2)
{
    if (!standardIs532_2) {
        return elementwise(mr, phon, [](double p) {
            if (p < 40.0) return std::pow(p / 40.0, 1.0 / 0.35);
            return std::pow(2.0, p / 10.0 - 4.0);
        });
    }
    // ISO 532-2: PCHIP initial guess + bisection refinement to make
    // phon2sone(sone2phon(s)) ≈ s (matching MATLAB phon2sone.m which
    // uses fzero on a similar inverse search). The initial guess from
    // PCHIP is typically within ~1%; bisection narrows to 1e-12 quickly.
    Value xs   = tab5PhonVec(mr);     // phon column for guess (forward)
    Value ys   = tab5SoneVec(mr);     // sone column for guess (forward)
    Value xs_s = tab5SoneVec(mr);     // sone column for sone2phon (inverse)
    Value ys_p = tab5PhonVec(mr);     // phon column for sone2phon (inverse)

    // Inline sone2phon ISO 532-2 (single scalar — for refinement loop).
    auto sone2phonInline = [&](double s) -> double {
        double phonVal;
        if (s > kTab5Sone[kTab5N - 1]) {
            phonVal = linearExtrap(kTab5Sone, kTab5Phon, kTab5N, s);
        } else {
            Value q = Value::scalar(s, mr);
            Value y = builtin::pchip(xs_s, ys_p, q, mr);
            phonVal = y.toScalar();
        }
        if (phonVal < 0.0) phonVal = 0.0;
        return phonVal;
    };

    return elementwise(mr, phon, [&](double p) {
        const double pCapped = (p > 144.0) ? 144.0 : p;
        // Initial guess via PCHIP forward table.
        double guess;
        {
            Value q = Value::scalar(pCapped, mr);
            Value y = builtin::pchip(xs, ys, q, mr);
            guess = y.toScalar();
        }
        if (guess <= 0.0) return guess;  // matches MATLAB low-p path
        // Build bracket around the guess; expand if not bracketing.
        // f(s) = sone2phon(s) - p. We want f(s)=0.
        double lo = guess * 0.5;
        double hi = guess * 1.5;
        double fLo = sone2phonInline(lo) - pCapped;
        double fHi = sone2phonInline(hi) - pCapped;
        size_t expand = 0;
        while (fLo * fHi > 0.0 && expand < 30) {
            // Walk the bracket outward (geometric).
            lo *= 0.5;
            hi *= 1.5;
            fLo = sone2phonInline(lo) - pCapped;
            fHi = sone2phonInline(hi) - pCapped;
            ++expand;
        }
        if (fLo * fHi > 0.0) return guess;  // give up, fall back to PCHIP guess
        // Bisection to ~1e-12 relative or 80 iters max.
        for (size_t it = 0; it < 80; ++it) {
            const double mid = 0.5 * (lo + hi);
            if (hi - lo < 1e-12 * std::max(1.0, std::abs(mid))) return mid;
            const double fMid = sone2phonInline(mid) - pCapped;
            if (fMid == 0.0) return mid;
            if (fLo * fMid < 0.0) { hi = mid; fHi = fMid; }
            else                  { lo = mid; fLo = fMid; }
        }
        return 0.5 * (lo + hi);
    });
}

Value sone2phon(std::pmr::memory_resource *mr, const Value &sone,
                bool standardIs532_2)
{
    if (!standardIs532_2) {
        return elementwise(mr, sone, [](double s) {
            if (s < 1.0) return 40.0 * std::pow(s, 0.35);
            return 40.0 + 10.0 * std::log2(s);
        });
    }
    // ISO 532-2: PCHIP from sone → phon, with linear extrapolation
    // beyond xv(2,end)=337.6 sone. Negative results clamped to 0.
    Value xs = tab5SoneVec(mr);
    Value ys = tab5PhonVec(mr);
    return elementwise(mr, sone, [&](double s) {
        double phon;
        if (s > kTab5Sone[kTab5N - 1]) {
            // Linear extrapolation matching MATLAB sone2phon.m branch.
            phon = linearExtrap(kTab5Sone, kTab5Phon, kTab5N, s);
        } else {
            Value q = Value::scalar(s, mr);
            Value y = builtin::pchip(xs, ys, q, mr);
            phon = y.toScalar();
        }
        if (phon < 0.0) phon = 0.0;
        return phon;
    });
}

namespace detail {

#define NK_ELEM_REG(FN)                                                          \
    void FN##_reg(Span<const Value> args, size_t /*nargout*/,                    \
                  Span<Value> outs, CallContext &ctx)                            \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#FN ": requires (x)",                                    \
                        0, 0, #FN, "", "m:" #FN ":nargin");                      \
        outs[0] = FN(ctx.engine->resource(), args[0]);                           \
    }

NK_ELEM_REG(hz2mel)
NK_ELEM_REG(mel2hz)
NK_ELEM_REG(hz2bark)
NK_ELEM_REG(bark2hz)
NK_ELEM_REG(hz2erb)
NK_ELEM_REG(erb2hz)

#undef NK_ELEM_REG

// phon2sone / sone2phon take an optional second arg = "ISO 532-1"
// (default) or "ISO 532-2". Cycle M added the ISO 532-2 path.
namespace {
bool isStandard532_2(const Value &v)
{
    if (v.type() == ValueType::CHAR || v.type() == ValueType::STRING) {
        std::string s = v.toString();
        // Case-insensitive compare.
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                        [](unsigned char c) { return std::tolower(c); });
        return (lower == "iso 532-2");
    }
    return false;
}
} // anon

void phon2sone_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("phon2sone: requires (phon [, standard])",
                    0, 0, "phon2sone", "", "m:phon2sone:nargin");
    bool iso532_2 = (args.size() >= 2) && isStandard532_2(args[1]);
    outs[0] = phon2sone(ctx.engine->resource(), args[0], iso532_2);
}

void sone2phon_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sone2phon: requires (sone [, standard])",
                    0, 0, "sone2phon", "", "m:sone2phon:nargin");
    bool iso532_2 = (args.size() >= 2) && isStandard532_2(args[1]);
    outs[0] = sone2phon(ctx.engine->resource(), args[0], iso532_2);
}

} // namespace detail

} // namespace numkit::audio
