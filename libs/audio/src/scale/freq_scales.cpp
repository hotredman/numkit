// libs/audio/src/scale/freq_scales.cpp
//
// Audio Toolbox frequency-scale and loudness conversions (cycle A).
// All formulas extracted bit-for-bit from MATLAB R2025b sources at
// $MATLABROOT/toolbox/audio/audio/{hz,mel,bark,erb,phon,sone}*.m
//
//   Mel:    O'Shaughnessy 1987 default
//   Bark:   Traunmüller 1990 with low/high-frequency corrections
//   ERB:    Glasberg & Moore 1990 (constants 24.673 and 0.004368)
//   Phon/Sone: ISO 532-1:2017
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.

#include <numkit/audio/scale/freq_scales.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

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

// MATLAB ERB scale factor: log(10) * 1000 / (24.673 * 4.368).
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
        // Note MATLAB uses 26.28 (not 26.81) in this denominator —
        // see toolbox/audio/audio/bark2hz.m. Asymmetry is intentional.
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

// ── Loudness ISO 532-1 ────────────────────────────────────────────────
Value phon2sone(std::pmr::memory_resource *mr, const Value &phon)
{
    return elementwise(mr, phon, [](double p) {
        if (p < 40.0) return std::pow(p / 40.0, 1.0 / 0.35);
        return std::pow(2.0, p / 10.0 - 4.0);
    });
}

Value sone2phon(std::pmr::memory_resource *mr, const Value &sone)
{
    return elementwise(mr, sone, [](double s) {
        if (s < 1.0) return 40.0 * std::pow(s, 0.35);
        return 40.0 + 10.0 * std::log2(s);
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
NK_ELEM_REG(phon2sone)
NK_ELEM_REG(sone2phon)

#undef NK_ELEM_REG

} // namespace detail

} // namespace numkit::audio
