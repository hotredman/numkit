// libs/image/src/filter/filter_design.cpp
//
// Image Toolbox filter-design utilities (cycle 4).
//
//   fspecial3(type [, hsize [, sigma|semiaxes|gamma]])
//                                3-D filter kernels: 'average',
//                                'gaussian', 'laplacian', 'log',
//                                'prewitt', 'sobel', 'ellipsoid'.
//   fwind2(Hd, win)              2-D FIR via 2-D window method.
//                                ifft2-shift(Hd) elementwise × win.
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.
//
// KNOWN GAPs (deferred to v2):
//   * fsamp2 / ftrans2 / fwind1 — require 2-D FFT/IFFT + Chebyshev
//     recurrence (ftrans2). Stubbed with explicit "not implemented"
//     errors so MATLAB scripts that touch these get a clear message
//     instead of "undefined function".
//   * gabor — Gabor filter object. Requires a class infrastructure
//     beyond v1 scope.
//   * fspecial3 with 4-arg laplacian (gamma1, gamma2) supports only
//     gamma1=gamma2=0 (default) in v1.

#include <numkit/image/filter/filter.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::image {

namespace {

// Resolve hsize spec into 3 dims.
//   missing → [5 5 5]
//   scalar → [n n n]
//   3-vec  → as-is
struct Size3 { size_t H, W, P; };
Size3 parseHsize(const Value &hsize)
{
    if (hsize.isEmpty() || hsize.isUnset()) return {5, 5, 5};
    if (hsize.isScalar()) {
        const size_t n = static_cast<size_t>(hsize.toScalar());
        return {n, n, n};
    }
    if (hsize.numel() == 3) {
        return {static_cast<size_t>(hsize.elemAsDouble(0)),
                static_cast<size_t>(hsize.elemAsDouble(1)),
                static_cast<size_t>(hsize.elemAsDouble(2))};
    }
    throw Error("fspecial3: hsize must be scalar or 3-element vector",
                0, 0, "fspecial3", "", "m:fspecial3:BadHsize");
}

inline size_t lin3(size_t r, size_t c, size_t p, size_t H, size_t W)
{
    return r + c * H + p * H * W;  // column-major over R, C, then P-page
}

// Helper: write H × W × P kernel from a generator (r, c, p) → val.
template <typename Gen>
Value makeKernel3(std::pmr::memory_resource *mr, Size3 s, Gen gen)
{
    Value out = Value::matrix3d(s.H, s.W, s.P, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (size_t p = 0; p < s.P; ++p)
        for (size_t c = 0; c < s.W; ++c)
            for (size_t r = 0; r < s.H; ++r)
                od[lin3(r, c, p, s.H, s.W)] = gen(r, c, p);
    return out;
}

// Read sigma argument (scalar or 3-vec) with default fallback.
struct Sig3 { double sx, sy, sz; };
Sig3 parseSigma3(const Value &sig, double defv = 1.0)
{
    if (sig.isEmpty() || sig.isUnset()) return {defv, defv, defv};
    if (sig.isScalar()) {
        const double s = sig.toScalar();
        return {s, s, s};
    }
    if (sig.numel() == 3) {
        return {sig.elemAsDouble(0), sig.elemAsDouble(1), sig.elemAsDouble(2)};
    }
    throw Error("fspecial3: sigma must be scalar or 3-element vector",
                0, 0, "fspecial3", "", "m:fspecial3:BadSigma");
}

} // namespace

// ── fspecial3 ─────────────────────────────────────────────────────────
Value fspecial3(std::pmr::memory_resource *mr, const std::string &type,
                const Value &hsize, const Value &param)
{
    Size3 s = parseHsize(hsize);

    if (type == "average") {
        const double v = 1.0 / static_cast<double>(s.H * s.W * s.P);
        return makeKernel3(mr, s, [v](size_t, size_t, size_t) { return v; });
    }

    if (type == "gaussian") {
        Sig3 sg = parseSigma3(param, 1.0);
        const double cy = (static_cast<double>(s.H) - 1.0) / 2.0;
        const double cx = (static_cast<double>(s.W) - 1.0) / 2.0;
        const double cz = (static_cast<double>(s.P) - 1.0) / 2.0;
        // Compute unnormalized kernel then divide by sum.
        Value out = makeKernel3(mr, s, [&](size_t r, size_t c, size_t p) {
            const double dy = (static_cast<double>(r) - cy) / sg.sy;
            const double dx = (static_cast<double>(c) - cx) / sg.sx;
            const double dz = (static_cast<double>(p) - cz) / sg.sz;
            return std::exp(-0.5 * (dx*dx + dy*dy + dz*dz));
        });
        double sum = 0.0;
        const size_t N = s.H * s.W * s.P;
        const double *od = out.doubleData();
        for (size_t i = 0; i < N; ++i) sum += od[i];
        if (sum > 0.0) {
            double *odm = out.doubleDataMut();
            for (size_t i = 0; i < N; ++i) odm[i] /= sum;
        }
        return out;
    }

    if (type == "ellipsoid") {
        // semiaxes default 5 (scalar) or [a b c]
        double sa = 5.0, sb = 5.0, sc = 5.0;
        if (!param.isEmpty() && !param.isUnset()) {
            if (param.isScalar()) {
                sa = sb = sc = param.toScalar();
            } else if (param.numel() == 3) {
                sa = param.elemAsDouble(0);
                sb = param.elemAsDouble(1);
                sc = param.elemAsDouble(2);
            } else {
                throw Error("fspecial3: ellipsoid semiaxes must be scalar or 3-vec",
                            0, 0, "fspecial3", "", "m:fspecial3:BadSemiaxes");
            }
        }
        const double cy = (static_cast<double>(s.H) - 1.0) / 2.0;
        const double cx = (static_cast<double>(s.W) - 1.0) / 2.0;
        const double cz = (static_cast<double>(s.P) - 1.0) / 2.0;
        Value out = makeKernel3(mr, s, [&](size_t r, size_t c, size_t p) {
            const double dy = (static_cast<double>(r) - cy) / sb;
            const double dx = (static_cast<double>(c) - cx) / sa;
            const double dz = (static_cast<double>(p) - cz) / sc;
            return (dx*dx + dy*dy + dz*dz <= 1.0) ? 1.0 : 0.0;
        });
        double sum = 0.0;
        const size_t N = s.H * s.W * s.P;
        const double *od = out.doubleData();
        for (size_t i = 0; i < N; ++i) sum += od[i];
        if (sum > 0.0) {
            double *odm = out.doubleDataMut();
            for (size_t i = 0; i < N; ++i) odm[i] /= sum;
        }
        return out;
    }

    if (type == "laplacian") {
        // 3-D Laplacian (alpha=0 default). Standard discrete Laplacian:
        // -6 at center, +1 at 6 face neighbours, 0 elsewhere. 3×3×3.
        Size3 ks{3, 3, 3};
        Value out = makeKernel3(mr, ks, [](size_t r, size_t c, size_t p) -> double {
            const int dr = static_cast<int>(r) - 1;
            const int dc = static_cast<int>(c) - 1;
            const int dp = static_cast<int>(p) - 1;
            const int d2 = dr*dr + dc*dc + dp*dp;
            if (d2 == 0) return -6.0;
            if (d2 == 1) return  1.0;
            return 0.0;
        });
        return out;
    }

    if (type == "log") {
        // Laplacian-of-Gaussian: ∇²G(x,y,z) for 3-D Gaussian with sigma.
        Sig3 sg = parseSigma3(param, 0.5);
        const double sig2 = sg.sx * sg.sx;  // assume isotropic for v1
        const double cy = (static_cast<double>(s.H) - 1.0) / 2.0;
        const double cx = (static_cast<double>(s.W) - 1.0) / 2.0;
        const double cz = (static_cast<double>(s.P) - 1.0) / 2.0;
        Value out = makeKernel3(mr, s, [&](size_t r, size_t c, size_t p) {
            const double dy = static_cast<double>(r) - cy;
            const double dx = static_cast<double>(c) - cx;
            const double dz = static_cast<double>(p) - cz;
            const double r2 = dx*dx + dy*dy + dz*dz;
            const double g = std::exp(-r2 / (2.0 * sig2));
            // 3-D LoG: (r²/σ⁴ - 3/σ²) * G  (kernel normalized so sum ≈ 0)
            return (r2 / (sig2 * sig2) - 3.0 / sig2) * g;
        });
        // Subtract mean so kernel sum = 0 (matches MATLAB's normalization).
        double mean = 0.0;
        const size_t N = s.H * s.W * s.P;
        const double *od = out.doubleData();
        for (size_t i = 0; i < N; ++i) mean += od[i];
        mean /= static_cast<double>(N);
        double *odm = out.doubleDataMut();
        for (size_t i = 0; i < N; ++i) odm[i] -= mean;
        return out;
    }

    if (type == "sobel" || type == "prewitt") {
        // 3-D edge kernel along direction X / Y / Z.
        // Default direction = "X". param holds the direction string.
        std::string dir = "X";
        if (!param.isEmpty() && !param.isUnset()) {
            if (param.isChar() || param.isString()) dir = param.toString();
        }
        const bool isSobel = (type == "sobel");
        // Smoothing kernel along non-derivative dims.
        // Sobel:  [1, 2, 1]
        // Prewitt:[1, 1, 1]
        const double w0 = isSobel ? 1.0 : 1.0;
        const double w1 = isSobel ? 2.0 : 1.0;
        // Outer product over the 2 smoothing dims, derivative [-1, 0, +1] on chosen dim.
        Size3 ks{3, 3, 3};
        Value out = makeKernel3(mr, ks, [&](size_t r, size_t c, size_t p) -> double {
            const int dr = static_cast<int>(r) - 1;
            const int dc = static_cast<int>(c) - 1;
            const int dp = static_cast<int>(p) - 1;
            auto sm = [&](int x) -> double { return (x == 0) ? w1 : w0; };
            auto dv = [](int x) -> double { return -static_cast<double>(x); };  // [-1, 0, +1]
            // dv signs flipped: we want kernel that responds to gradient in chosen dir.
            // x_dir = position along direction, signed: dv(-1) = +1, dv(0) = 0, dv(+1) = -1.
            // Actually MATLAB uses [+1, 0, -1] on derivative axis.
            if (dir == "X" || dir == "x") return  static_cast<double>(-dc) * sm(dr) * sm(dp);
            if (dir == "Y" || dir == "y") return  static_cast<double>(-dr) * sm(dc) * sm(dp);
            if (dir == "Z" || dir == "z") return  static_cast<double>(-dp) * sm(dr) * sm(dc);
            (void)dv;
            throw Error("fspecial3: direction must be 'X', 'Y', or 'Z'",
                        0, 0, "fspecial3", "", "m:fspecial3:BadDir");
        });
        return out;
    }

    throw Error("fspecial3: unsupported type '" + type + "'",
                0, 0, "fspecial3", "", "m:fspecial3:BadType");
}

// ── fwind2 ────────────────────────────────────────────────────────────
// 2-D FIR via 2-D window method (basic form):
//   h = fftshift(ifft2(ifftshift(Hd))) .* w
// Without an in-house 2D-FFT in core, we implement the equivalent
// closed-form via the inverse-DFT formula (O(MNHW)). Acceptable for the
// small filter sizes typical for 2-D image filter design.
Value fwind2(std::pmr::memory_resource *mr, const Value &Hd, const Value &w)
{
    if (Hd.dims().is3D() || w.dims().is3D())
        throw Error("fwind2: inputs must be 2-D",
                    0, 0, "fwind2", "", "m:fwind2:Not2D");
    const size_t H = Hd.dims().rows();
    const size_t W = Hd.dims().cols();
    if (w.dims().rows() != H || w.dims().cols() != W)
        throw Error("fwind2: window must match Hd shape",
                    0, 0, "fwind2", "", "m:fwind2:ShapeMismatch");

    Value h = Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return h;
    double *hd = h.doubleDataMut();

    // Compute h(m,n) = (1/(MN)) Σ_p Σ_q Hd(p,q) * exp(j 2π (mp/M + nq/N))
    // Hd is assumed real-symmetric so the imaginary part cancels out.
    // Output centered: m = -(H-1)/2 .. (H-1)/2, similarly n.
    const double cM = (static_cast<double>(H) - 1.0) / 2.0;
    const double cN = (static_cast<double>(W) - 1.0) / 2.0;
    const double inv = 1.0 / static_cast<double>(H * W);
    for (size_t mi = 0; mi < H; ++mi) {
        const double m = static_cast<double>(mi) - cM;
        for (size_t ni = 0; ni < W; ++ni) {
            const double n = static_cast<double>(ni) - cN;
            double acc_re = 0.0;
            for (size_t p = 0; p < H; ++p) {
                const double fp = static_cast<double>(p) - cM;  // freq index centered
                for (size_t q = 0; q < W; ++q) {
                    const double fq = static_cast<double>(q) - cN;
                    const double Hdv = Hd.elemAsDouble(p + q * H);
                    const double phase = 2.0 * M_PI * (m * fp / static_cast<double>(H)
                                                     + n * fq / static_cast<double>(W));
                    acc_re += Hdv * std::cos(phase);
                }
            }
            hd[mi + ni * H] = acc_re * inv * w.elemAsDouble(mi + ni * H);
        }
    }
    // Normalise so DC gain matches input (MATLAB's fwind2 normalizes).
    double sum = 0.0;
    for (size_t i = 0; i < H * W; ++i) sum += hd[i];
    if (sum != 0.0) {
        const double scale = 1.0 / sum;
        for (size_t i = 0; i < H * W; ++i) hd[i] *= scale;
    }
    return h;
}

namespace detail {

void fspecial3_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fspecial3: requires at least (type)",
                    0, 0, "fspecial3", "", "m:fspecial3:nargin");
    if (!args[0].isChar() && !args[0].isString())
        throw Error("fspecial3: type must be a string",
                    0, 0, "fspecial3", "", "m:fspecial3:BadType");
    const std::string type = args[0].toString();
    Value empty;
    // Sobel/prewitt: 2nd arg is direction string (no hsize — fixed 3x3x3).
    // Other types: 2nd arg is hsize, 3rd arg is sigma/semiaxes/gamma.
    Value hsize = empty;
    Value param = empty;
    if (type == "sobel" || type == "prewitt") {
        if (args.size() >= 2) param = args[1];   // direction
    } else {
        if (args.size() >= 2) hsize = args[1];
        if (args.size() >= 3) param = args[2];
    }
    outs[0] = fspecial3(ctx.engine->resource(), type, hsize, param);
}

void fwind2_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fwind2: requires (Hd, win) — 4-arg form (f1, f2, Hd, win) deferred",
                    0, 0, "fwind2", "", "m:fwind2:nargin");
    outs[0] = fwind2(ctx.engine->resource(), args[0], args[1]);
}

// Stubs for the deferred filter-design fns: throw with explicit
// KNOWN GAP error so calls fail loud instead of silent undefined-fn.
void fsamp2_reg(Span<const Value> /*args*/, size_t /*nargout*/,
                Span<Value> /*outs*/, CallContext &)
{
    throw Error("fsamp2: not implemented in v1 — requires 2-D IFFT "
                "infrastructure. KNOWN GAP, deferred.",
                0, 0, "fsamp2", "", "m:fsamp2:NotImpl");
}

void ftrans2_reg(Span<const Value> /*args*/, size_t /*nargout*/,
                 Span<Value> /*outs*/, CallContext &)
{
    throw Error("ftrans2: not implemented in v1 — requires Chebyshev "
                "polynomial recurrence. KNOWN GAP, deferred.",
                0, 0, "ftrans2", "", "m:ftrans2:NotImpl");
}

void fwind1_reg(Span<const Value> /*args*/, size_t /*nargout*/,
                Span<Value> /*outs*/, CallContext &)
{
    throw Error("fwind1: not implemented in v1 — requires Chebyshev "
                "transformation of 1-D window into 2-D. KNOWN GAP.",
                0, 0, "fwind1", "", "m:fwind1:NotImpl");
}

void gabor_reg(Span<const Value> /*args*/, size_t /*nargout*/,
               Span<Value> /*outs*/, CallContext &)
{
    throw Error("gabor: not implemented in v1 — Gabor filter object "
                "requires class infrastructure. KNOWN GAP.",
                0, 0, "gabor", "", "m:gabor:NotImpl");
}

} // namespace detail

} // namespace numkit::image
