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
// fspecial3 covers every documented MATLAB R2025b branch:
//   * average    — ones(hsize)/prod(hsize), default hsize [5 5 5].
//   * gaussian    — separable anisotropic Gaussian, normalised to sum 1;
//                   sigma scalar or per-axis [σrow σcol σpage], default 1.
//   * ellipsoid   — integer-grid mask {(Δr/a)²+(Δc/b)²+(Δp/c)² ≤ 1}
//                   normalised by voxel count; size 2·ceil(semiaxes)+1.
//   * laplacian   — two-parameter (γ1, γ2) 3×3×3 discrete Laplacian
//                   (Lindeberg 1994 §; γ1 weights the 12 edge neighbours,
//                   γ2 the 8 corner neighbours), sums to zero.
//   * log         — Laplacian of (anisotropic) Gaussian ∇²G, zero-mean,
//                   default sigma 1.
//   * prewitt / sobel — separable 3×3×3 gradient operators along X/Y/Z.
//
// Reference: standard separable filter constructions; J. Lim,
//   "Two-Dimensional Signal and Image Processing", 1990 (LoG, gradient).
//
// KNOWN GAPs (deferred to v2):
//   * fsamp2 / ftrans2 / fwind1 — now implemented in fir2d.cpp.
//   * gabor — Gabor filter object. Requires a class infrastructure
//     beyond v1 scope (MATLAB-OOP, blocked by §0).

#include <numkit/image/filter/filter.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

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
                0, 0, "fspecial3", "", "numkit:fspecial3:BadHsize");
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
                0, 0, "fspecial3", "", "numkit:fspecial3:BadSigma");
}

} // namespace

namespace {

// Parse semiaxes (ellipsoid): scalar → [a a a], 3-vec → [a b c], default 5.
// Order matches MATLAB: element 0 → rows, 1 → cols, 2 → pages.
Sig3 parseSemiaxes(const Value &v, double defv = 5.0)
{
    if (v.isEmpty() || v.isUnset()) return {defv, defv, defv};
    if (v.isScalar()) { const double a = v.toScalar(); return {a, a, a}; }
    if (v.numel() == 3)
        return {v.elemAsDouble(0), v.elemAsDouble(1), v.elemAsDouble(2)};
    throw Error("fspecial3: semiaxes must be a scalar or 3-element vector",
                0, 0, "fspecial3", "", "numkit:fspecial3:BadSemiaxes");
}

} // namespace

// ── fspecial3 ─────────────────────────────────────────────────────────
// Two-layer typed entry. Positional args after `type` are interpreted
// per the MATLAB R2025b signature:
//   average:        a1 = hsize
//   gaussian / log: a1 = hsize,    a2 = sigma
//   ellipsoid:      a1 = semiaxes
//   laplacian:      a1 = gamma1,   a2 = gamma2
//   prewitt/sobel:  a1 = direction
Value fspecial3(const std::string &type, const Value &a1, const Value &a2,
                std::pmr::memory_resource *mr)
{
    if (type == "average") {
        Size3 s = parseHsize(a1);
        const double v = 1.0 / static_cast<double>(s.H * s.W * s.P);
        return makeKernel3(mr, s, [v](size_t, size_t, size_t) { return v; });
    }

    if (type == "gaussian") {
        Size3 s = parseHsize(a1);
        // sigma element 0 → rows, 1 → cols, 2 → pages (MATLAB convention).
        Sig3 sg = parseSigma3(a2, 1.0);
        const double sr = sg.sx, sc = sg.sy, sp = sg.sz;
        const double cy = (static_cast<double>(s.H) - 1.0) / 2.0;
        const double cx = (static_cast<double>(s.W) - 1.0) / 2.0;
        const double cz = (static_cast<double>(s.P) - 1.0) / 2.0;
        Value out = makeKernel3(mr, s, [&](size_t r, size_t c, size_t p) {
            const double dr = (static_cast<double>(r) - cy) / sr;
            const double dc = (static_cast<double>(c) - cx) / sc;
            const double dp = (static_cast<double>(p) - cz) / sp;
            return std::exp(-0.5 * (dr*dr + dc*dc + dp*dp));
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
        // semiaxes element 0 → rows, 1 → cols, 2 → pages.
        Sig3 ax = parseSemiaxes(a1, 5.0);
        const double ar = ax.sx, ac = ax.sy, ap = ax.sz;
        if (ar <= 0.0 || ac <= 0.0 || ap <= 0.0)
            throw Error("fspecial3: ellipsoid semiaxes must be positive",
                        0, 0, "fspecial3", "", "numkit:fspecial3:BadSemiaxes");
        // Grid half-extent = ceil(semiaxis); size = 2·half + 1.
        Size3 s{ static_cast<size_t>(2.0 * std::ceil(ar) + 1.0),
                 static_cast<size_t>(2.0 * std::ceil(ac) + 1.0),
                 static_cast<size_t>(2.0 * std::ceil(ap) + 1.0) };
        const double cy = (static_cast<double>(s.H) - 1.0) / 2.0;
        const double cx = (static_cast<double>(s.W) - 1.0) / 2.0;
        const double cz = (static_cast<double>(s.P) - 1.0) / 2.0;
        Value out = makeKernel3(mr, s, [&](size_t r, size_t c, size_t p) {
            const double dr = (static_cast<double>(r) - cy) / ar;
            const double dc = (static_cast<double>(c) - cx) / ac;
            const double dp = (static_cast<double>(p) - cz) / ap;
            return (dr*dr + dc*dc + dp*dp <= 1.0) ? 1.0 : 0.0;
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
        // Two-parameter 3×3×3 discrete Laplacian. gamma1 weights the 12
        // edge (√2) neighbours, gamma2 the 8 corner (√3) neighbours; the
        // 6 face (1) neighbours carry (1−γ1−γ2); the centre is set so the
        // kernel sums to zero. Defaults γ1=γ2=0 → classic 6-neighbour
        // Laplacian (face +1, centre −6).
        const double g1 = (!a1.isEmpty() && !a1.isUnset()) ? a1.toScalar() : 0.0;
        const double g2 = (!a2.isEmpty() && !a2.isUnset()) ? a2.toScalar() : 0.0;
        if (g1 < 0.0)
            throw Error("fspecial3: Expected input number 2, GAMMA1, to be nonnegative.",
                        0, 0, "fspecial3", "", "numkit:fspecial3:BadGamma");
        if (g2 < 0.0)
            throw Error("fspecial3: Expected input number 3, GAMMA2, to be nonnegative.",
                        0, 0, "fspecial3", "", "numkit:fspecial3:BadGamma");
        if (g1 + g2 > 1.0)
            throw Error("fspecial3: GAMMA1 + GAMMA2 should be less than or equal to 1 and greater than 0.",
                        0, 0, "fspecial3", "", "numkit:fspecial3:BadGamma");
        const double face   = 1.0 - g1 - g2;        // 6 neighbours, dist² 1
        const double edge   = g1 / 4.0;             // 12 neighbours, dist² 2
        const double corner = g2 / 4.0;             // 8 neighbours, dist² 3
        const double center = -6.0 + 3.0 * g1 + 4.0 * g2;
        Size3 ks{3, 3, 3};
        return makeKernel3(mr, ks, [&](size_t r, size_t c, size_t p) -> double {
            const int dr = static_cast<int>(r) - 1;
            const int dc = static_cast<int>(c) - 1;
            const int dp = static_cast<int>(p) - 1;
            const int d2 = dr*dr + dc*dc + dp*dp;
            if (d2 == 0) return center;
            if (d2 == 1) return face;
            if (d2 == 2) return edge;
            return corner;  // d2 == 3
        });
    }

    if (type == "log") {
        // Laplacian of an (anisotropic) Gaussian: ∇²G = G · Σ_d (Δd²−σd²)/σd⁴,
        // with G normalised to unit sum first, then the whole kernel made
        // zero-mean. Reduces to (r²−3σ²)/σ⁴·G in the isotropic case.
        Size3 s = parseHsize(a1);
        Sig3 sg = parseSigma3(a2, 1.0);
        const double sr = sg.sx, sc = sg.sy, sp = sg.sz;
        const double cy = (static_cast<double>(s.H) - 1.0) / 2.0;
        const double cx = (static_cast<double>(s.W) - 1.0) / 2.0;
        const double cz = (static_cast<double>(s.P) - 1.0) / 2.0;
        const size_t N = s.H * s.W * s.P;
        // Step 1: normalised Gaussian.
        Value gK = makeKernel3(mr, s, [&](size_t r, size_t c, size_t p) {
            const double dr = (static_cast<double>(r) - cy) / sr;
            const double dc = (static_cast<double>(c) - cx) / sc;
            const double dp = (static_cast<double>(p) - cz) / sp;
            return std::exp(-0.5 * (dr*dr + dc*dc + dp*dp));
        });
        double gsum = 0.0;
        { const double *gd = gK.doubleData(); for (size_t i = 0; i < N; ++i) gsum += gd[i]; }
        const double inv = (gsum > 0.0) ? 1.0 / gsum : 1.0;
        const double sr2 = sr*sr, sc2 = sc*sc, sp2 = sp*sp;
        const double *gd = gK.doubleData();
        // Step 2: multiply normalised G by the Laplacian factor.
        Value out = makeKernel3(mr, s, [&](size_t r, size_t c, size_t p) {
            const double dr = static_cast<double>(r) - cy;
            const double dc = static_cast<double>(c) - cx;
            const double dp = static_cast<double>(p) - cz;
            const double Gn = gd[lin3(r, c, p, s.H, s.W)] * inv;  // normalised Gaussian
            const double lap = (dr*dr - sr2) / (sr2*sr2)
                             + (dc*dc - sc2) / (sc2*sc2)
                             + (dp*dp - sp2) / (sp2*sp2);
            return Gn * lap;
        });
        // Step 3: subtract mean so the kernel sums to zero.
        double mean = 0.0;
        const double *od = out.doubleData();
        for (size_t i = 0; i < N; ++i) mean += od[i];
        mean /= static_cast<double>(N);
        double *odm = out.doubleDataMut();
        for (size_t i = 0; i < N; ++i) odm[i] -= mean;
        return out;
    }

    if (type == "sobel" || type == "prewitt") {
        // Separable 3×3×3 gradient. Derivative [+1, 0, −1] along the chosen
        // axis (X→cols, Y→rows, Z→pages); smoothing [1 1 1] (prewitt) /
        // [1 2 1] (sobel) along the other two axes.
        std::string dir = "X";
        if (!a1.isEmpty() && !a1.isUnset() && (a1.isChar() || a1.isString()))
            dir = a1.toString();
        const bool isSobel = (type == "sobel");
        const double w0 = 1.0;
        const double w1 = isSobel ? 2.0 : 1.0;
        Size3 ks{3, 3, 3};
        return makeKernel3(mr, ks, [&](size_t r, size_t c, size_t p) -> double {
            const int dr = static_cast<int>(r) - 1;
            const int dc = static_cast<int>(c) - 1;
            const int dp = static_cast<int>(p) - 1;
            auto sm = [&](int x) -> double { return (x == 0) ? w1 : w0; };
            if (dir == "X" || dir == "x") return static_cast<double>(-dc) * sm(dr) * sm(dp);
            if (dir == "Y" || dir == "y") return static_cast<double>(-dr) * sm(dc) * sm(dp);
            if (dir == "Z" || dir == "z") return static_cast<double>(-dp) * sm(dr) * sm(dc);
            throw Error("fspecial3: direction must be 'X', 'Y', or 'Z'",
                        0, 0, "fspecial3", "", "numkit:fspecial3:BadDir");
        });
    }

    throw Error("fspecial3: unsupported type '" + type + "'",
                0, 0, "fspecial3", "", "numkit:fspecial3:BadType");
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
                    0, 0, "fwind2", "", "numkit:fwind2:Not2D");
    const size_t H = Hd.dims().rows();
    const size_t W = Hd.dims().cols();
    if (w.dims().rows() != H || w.dims().cols() != W)
        throw Error("fwind2: window must match Hd shape",
                    0, 0, "fwind2", "", "numkit:fwind2:ShapeMismatch");

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

} // namespace numkit::image
