// libs/dsp/src/MDspSos.cpp
//
// Second-order sections (SOS) family — see public header for the
// MATLAB-equivalent contract.
//
// Implementation overview:
//   * sosfilt: cascade of Direct-Form-II Transposed biquads. Each
//     section's leading a0 is normalised to 1 internally.
//   * zp2sos: greedy pole-zero pairing. Sort poles by descending
//     magnitude (closest-to-unit-circle first → most numerically
//     critical), pair complex conjugates, then for each pole pair
//     pick the closest zero pair. Build biquads from the (root pair)
//     → quadratic (1 - 2·Re(r)·z⁻¹ + |r|²·z⁻²) expansion.
//   * tf2sos: roots(b)/roots(a) (Durand-Kerner) → zp2sos.

#include <numkit/m/dsp/MDspSos.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace numkit::m::dsp {

using numkit::m::Complex;

namespace {

// ─────────────────────────────────────────────────────────────────
// sosfilt — Direct Form II Transposed biquad cascade
// ─────────────────────────────────────────────────────────────────

// In-place: y[n] for one biquad section, single-channel signal.
// b/a are pre-normalised so a0 = 1; the formula reduces to:
//   y[n] = b0·x[n] + s1
//   s1   = b1·x[n] - a1·y[n] + s2
//   s2   = b2·x[n] - a2·y[n]
void biquadDf2t(double b0, double b1, double b2,
                double a1, double a2,
                const double *x, double *y, size_t n)
{
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double xi = x[i];
        const double yi = b0 * xi + s1;
        s1 = b1 * xi - a1 * yi + s2;
        s2 = b2 * xi - a2 * yi;
        y[i] = yi;
    }
}

// ─────────────────────────────────────────────────────────────────
// Polynomial root finder (Durand-Kerner)
// ─────────────────────────────────────────────────────────────────
//
// Used by tf2sos to find roots of b(z) and a(z) without depending on
// an eigenvalue solver. Durand-Kerner is iterative, simultaneously
// converging on all N roots of a degree-N polynomial. Quadratic
// convergence near simple roots; ~50 iterations is sufficient for
// IEEE-double precision on coefficient magnitudes typical for IIR
// filter design (degrees 2..32).

void evalPoly(const std::vector<Complex> &coeffs, Complex z,
              Complex &out, Complex &deriv)
{
    // Horner evaluation of poly + derivative.
    Complex p = coeffs[0];
    Complex dp(0.0, 0.0);
    for (size_t i = 1; i < coeffs.size(); ++i) {
        dp = dp * z + p;
        p = p * z + coeffs[i];
    }
    out = p;
    deriv = dp;
}

// Accept coefficients in MATLAB convention: c[0] is the leading
// coefficient (highest power), c[n] is the constant. Returns the n
// (possibly complex) roots.
std::vector<Complex> rootsDurandKerner(const std::vector<double> &coeffs)
{
    // Strip leading zeros (degree drop); strip trailing zeros (root
    // at origin) so DK doesn't see a singular Jacobian.
    size_t lo = 0, hi = coeffs.size();
    while (lo < hi && coeffs[lo] == 0.0) ++lo;
    size_t origZeros = 0;
    while (hi > lo && coeffs[hi - 1] == 0.0) { --hi; ++origZeros; }
    if (hi - lo <= 1) {
        // Constant or empty — the only "roots" are the origin ones we
        // stripped (if any).
        return std::vector<Complex>(origZeros, Complex(0.0, 0.0));
    }
    std::vector<Complex> c(hi - lo);
    for (size_t i = 0; i < c.size(); ++i)
        c[i] = Complex(coeffs[lo + i] / coeffs[lo], 0.0);
    // c is now monic with c[0] = 1.
    const size_t n = c.size() - 1;

    // Initial guesses on a circle of radius ~ Cauchy bound, avoiding
    // a real-axis cluster (factor of e^(j·0.4) chosen empirically by
    // numerical-recipes-style implementations).
    double maxCoef = 0.0;
    for (size_t i = 1; i <= n; ++i)
        maxCoef = std::max(maxCoef, std::abs(c[i].real()));
    const double R = 1.0 + maxCoef;
    std::vector<Complex> roots(n);
    const double twoPi = 6.283185307179586;
    for (size_t k = 0; k < n; ++k) {
        const double theta = twoPi * static_cast<double>(k) / static_cast<double>(n) + 0.4;
        roots[k] = Complex(R * std::cos(theta), R * std::sin(theta));
    }

    // Durand-Kerner with relaxation: x_k ← x_k - p(x_k) / Π_{j≠k}(x_k - x_j).
    constexpr int kMaxIter = 200;
    constexpr double kTol = 1e-14;
    for (int it = 0; it < kMaxIter; ++it) {
        double maxStep = 0.0;
        for (size_t k = 0; k < n; ++k) {
            Complex p, dp;
            evalPoly(c, roots[k], p, dp);
            Complex denom(1.0, 0.0);
            for (size_t j = 0; j < n; ++j)
                if (j != k) denom *= (roots[k] - roots[j]);
            if (std::abs(denom) < 1e-300) continue;
            const Complex step = p / denom;
            roots[k] -= step;
            const double s = std::abs(step);
            if (s > maxStep) maxStep = s;
        }
        if (maxStep < kTol) break;
    }

    for (size_t i = 0; i < origZeros; ++i)
        roots.push_back(Complex(0.0, 0.0));
    return roots;
}

// ─────────────────────────────────────────────────────────────────
// Pole-zero pairing (zp2sos core)
// ─────────────────────────────────────────────────────────────────
//
// MATLAB pairs (real or complex-conjugate) pairs of poles starting
// from the largest |p| (closest to the unit circle, most likely to
// dominate frequency response). For each pole pair, the nearest
// zero pair (by Euclidean distance to the pole pair's centroid)
// becomes that section's numerator. Quadratic (1 + b1·z⁻¹ + b2·z⁻²)
// is built from the root pair via:
//   conjugate pair (a±bi): 1 - 2a·z⁻¹ + (a² + b²)·z⁻²
//   single real root r:    1 - r·z⁻¹

constexpr double kImagThresh = 1e-10;  // |Im(r)| below this → treat as real

inline bool isReal(Complex r)
{
    return std::abs(r.imag()) <= kImagThresh;
}

// Pop a real or complex-conjugate pair from `pool`. The pair is
// returned as two Complex; the picked-up indices are erased from pool.
// Returns false if pool is empty.
struct RootPair { Complex a, b; bool isPair; };

bool popPair(std::vector<Complex> &pool, RootPair &out)
{
    if (pool.empty()) return false;
    // Find the root with the largest magnitude — pair from there.
    size_t pickIdx = 0;
    double bestMag = std::abs(pool[0]);
    for (size_t i = 1; i < pool.size(); ++i) {
        const double m = std::abs(pool[i]);
        if (m > bestMag) { bestMag = m; pickIdx = i; }
    }
    const Complex pick = pool[pickIdx];
    if (isReal(pick)) {
        // Find another real root to pair with (closest by |·|), or
        // make a degree-1 section with a single real root.
        size_t mateIdx = pool.size();
        double bestDist = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < pool.size(); ++i) {
            if (i == pickIdx) continue;
            if (!isReal(pool[i])) continue;
            const double d = std::abs(pool[i] - pick);
            if (d < bestDist) { bestDist = d; mateIdx = i; }
        }
        if (mateIdx < pool.size()) {
            out = { Complex(pick.real(), 0.0), Complex(pool[mateIdx].real(), 0.0), true };
            // Erase both — careful with index shifting.
            const size_t hi = std::max(pickIdx, mateIdx);
            const size_t lo = std::min(pickIdx, mateIdx);
            pool.erase(pool.begin() + hi);
            pool.erase(pool.begin() + lo);
        } else {
            out = { Complex(pick.real(), 0.0), Complex(0.0, 0.0), false };
            pool.erase(pool.begin() + pickIdx);
        }
    } else {
        // Complex root: pair with its conjugate.
        const Complex conjP = std::conj(pick);
        size_t mateIdx = pool.size();
        double bestDist = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < pool.size(); ++i) {
            if (i == pickIdx) continue;
            const double d = std::abs(pool[i] - conjP);
            if (d < bestDist) { bestDist = d; mateIdx = i; }
        }
        if (mateIdx >= pool.size())
            throw MError("zp2sos: complex root has no conjugate in input",
                         0, 0, "zp2sos", "", "m:zp2sos:noConj");
        out = { pick, pool[mateIdx], true };
        const size_t hi = std::max(pickIdx, mateIdx);
        const size_t lo = std::min(pickIdx, mateIdx);
        pool.erase(pool.begin() + hi);
        pool.erase(pool.begin() + lo);
    }
    return true;
}

// Convert a (root pair) → (b1, b2) coefficients of (z² + b1·z + b2),
// representing the quadratic (1 + b1·z⁻¹ + b2·z⁻²).
//   conjugate pair (a±bi): b1 = -2a,  b2 = a² + b²
//   real pair (r1, r2):    b1 = -(r1+r2), b2 = r1·r2
//   single real (r):       b1 = -r,   b2 = 0
struct QuadCoeffs { double b1, b2; };

QuadCoeffs pairToQuad(const RootPair &p)
{
    if (!p.isPair) {
        return { -p.a.real(), 0.0 };
    }
    if (!isReal(p.a) || !isReal(p.b)) {
        // Complex conjugate pair (or near-conjugate due to numerics).
        return { -2.0 * p.a.real(),
                 p.a.real() * p.a.real() + p.a.imag() * p.a.imag() };
    }
    // Two real roots.
    return { -(p.a.real() + p.b.real()),
             p.a.real() * p.b.real() };
}

// Pair-pool centroid (a + b)/2 — used to find the closest pool of
// zeros for a given pole pair.
inline Complex pairCentroid(const RootPair &p)
{
    if (!p.isPair) return p.a;
    return (p.a + p.b) * 0.5;
}

// Pop the zero pair whose centroid is closest to `target`. Used to
// match each pole pair with its nearest zeros. Returns false if pool
// is empty (in which case the section gets a unity numerator).
bool popClosestZeroPair(std::vector<Complex> &zeros, Complex target,
                        RootPair &out)
{
    if (zeros.empty()) return false;
    // Find the closest zero. Then build the pair around it (its
    // conjugate if complex, the closest real otherwise).
    size_t pickIdx = 0;
    double bestDist = std::abs(zeros[0] - target);
    for (size_t i = 1; i < zeros.size(); ++i) {
        const double d = std::abs(zeros[i] - target);
        if (d < bestDist) { bestDist = d; pickIdx = i; }
    }
    const Complex pick = zeros[pickIdx];
    if (isReal(pick)) {
        // Find another real zero to pair with — closest by magnitude
        // distance to `target` (matches how MATLAB groups).
        size_t mateIdx = zeros.size();
        double bestMate = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < zeros.size(); ++i) {
            if (i == pickIdx) continue;
            if (!isReal(zeros[i])) continue;
            const double d = std::abs(zeros[i] - target);
            if (d < bestMate) { bestMate = d; mateIdx = i; }
        }
        if (mateIdx < zeros.size()) {
            out = { Complex(pick.real(), 0.0),
                    Complex(zeros[mateIdx].real(), 0.0), true };
            const size_t hi = std::max(pickIdx, mateIdx);
            const size_t lo = std::min(pickIdx, mateIdx);
            zeros.erase(zeros.begin() + hi);
            zeros.erase(zeros.begin() + lo);
        } else {
            out = { Complex(pick.real(), 0.0), Complex(0.0, 0.0), false };
            zeros.erase(zeros.begin() + pickIdx);
        }
    } else {
        // Complex: find conjugate.
        const Complex conjP = std::conj(pick);
        size_t mateIdx = zeros.size();
        double bestDist2 = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < zeros.size(); ++i) {
            if (i == pickIdx) continue;
            const double d = std::abs(zeros[i] - conjP);
            if (d < bestDist2) { bestDist2 = d; mateIdx = i; }
        }
        if (mateIdx >= zeros.size())
            throw MError("zp2sos: complex zero has no conjugate in input",
                         0, 0, "zp2sos", "", "m:zp2sos:noConj");
        out = { pick, zeros[mateIdx], true };
        const size_t hi = std::max(pickIdx, mateIdx);
        const size_t lo = std::min(pickIdx, mateIdx);
        zeros.erase(zeros.begin() + hi);
        zeros.erase(zeros.begin() + lo);
    }
    return true;
}

// Read a complex column vector from MValue (DOUBLE → real-only,
// COMPLEX → as-is). Empty input → empty vector.
std::vector<Complex> readComplexVec(const MValue &v, const char *fn)
{
    if (v.isEmpty()) return {};
    std::vector<Complex> out;
    out.reserve(v.numel());
    if (v.type() == MType::COMPLEX) {
        const Complex *p = v.complexData();
        for (size_t i = 0; i < v.numel(); ++i) out.push_back(p[i]);
    } else if (v.type() == MType::DOUBLE) {
        const double *p = v.doubleData();
        for (size_t i = 0; i < v.numel(); ++i) out.push_back(Complex(p[i], 0.0));
    } else {
        throw MError(std::string(fn) + ": zeros/poles must be DOUBLE or COMPLEX",
                     0, 0, fn, "", std::string("m:") + fn + ":type");
    }
    return out;
}

// Build the L×6 SOS matrix from the per-section (b0, b1, b2, a1, a2)
// arrays (a0 is always 1 in this representation). The leading b0
// scale is 1 unless distributeGain == true, in which case `gain` is
// folded into the first section's b0/b1/b2.
MValue buildSosMatrix(Allocator &alloc,
                      const std::vector<double> &b1s,
                      const std::vector<double> &b2s,
                      const std::vector<double> &a1s,
                      const std::vector<double> &a2s,
                      double leadingGain)
{
    const size_t L = a1s.size();
    auto sos = MValue::matrix(L, 6, MType::DOUBLE, &alloc);
    double *p = sos.doubleDataMut();
    // Column-major: column c, row r → index c*L + r.
    for (size_t r = 0; r < L; ++r) {
        const double scale = (r == 0) ? leadingGain : 1.0;
        p[0 * L + r] = scale;             // b0
        p[1 * L + r] = scale * b1s[r];    // b1
        p[2 * L + r] = scale * b2s[r];    // b2
        p[3 * L + r] = 1.0;               // a0
        p[4 * L + r] = a1s[r];            // a1
        p[5 * L + r] = a2s[r];            // a2
    }
    return sos;
}

// Validate `sos` is L×6 with positive L; returns L.
size_t validateSosMatrix(const MValue &sos)
{
    if (sos.dims().ndim() != 2 || sos.dims().cols() != 6 || sos.dims().rows() == 0)
        throw MError("sosfilt: sos matrix must be L×6 with L >= 1",
                     0, 0, "sosfilt", "", "m:sosfilt:sosShape");
    if (sos.type() != MType::DOUBLE)
        throw MError("sosfilt: sos matrix must be DOUBLE",
                     0, 0, "sosfilt", "", "m:sosfilt:sosType");
    return sos.dims().rows();
}

// Apply the cascade to one channel `xs` of length n; result in `out`
// (must be pre-allocated). `scratch` is a length-n workspace reused
// across sections.
void applyCascade(const MValue &sos, const double *xs, double *out, size_t n,
                  std::vector<double> &scratch)
{
    const size_t L = sos.dims().rows();
    const double *p = sos.doubleData();
    // First section reads from xs, writes to out. Subsequent sections
    // alternate between out and scratch.
    auto readSection = [&](size_t r) {
        const double a0 = p[3 * L + r];
        if (a0 == 0.0)
            throw MError("sosfilt: section a0 is zero",
                         0, 0, "sosfilt", "", "m:sosfilt:zeroLead");
        return std::array<double, 5>{
            p[0 * L + r] / a0,  // b0
            p[1 * L + r] / a0,  // b1
            p[2 * L + r] / a0,  // b2
            p[4 * L + r] / a0,  // a1
            p[5 * L + r] / a0,  // a2
        };
    };
    const double *src = xs;
    double *dst = out;
    for (size_t s = 0; s < L; ++s) {
        const auto c = readSection(s);
        biquadDf2t(c[0], c[1], c[2], c[3], c[4], src, dst, n);
        // Swap buffers for next section.
        if (s + 1 < L) {
            if (dst == out) { src = out; dst = scratch.data(); }
            else            { src = scratch.data(); dst = out; }
        }
    }
    // If the last write went to scratch, copy back into out.
    if (dst != out)
        std::copy(scratch.begin(), scratch.begin() + n, out);
}

} // namespace

// ════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════

MValue sosfilt(Allocator &alloc, const MValue &sos, const MValue &x)
{
    const size_t L = validateSosMatrix(sos);
    if (x.type() != MType::DOUBLE)
        throw MError("sosfilt: signal x must be DOUBLE",
                     0, 0, "sosfilt", "", "m:sosfilt:xType");
    if (x.isEmpty())
        return createLike(x, MType::DOUBLE, &alloc);
    (void) L;

    auto out = createLike(x, MType::DOUBLE, &alloc);
    if (x.dims().isVector() || x.isScalar()) {
        const size_t n = x.numel();
        std::vector<double> scratch(n);
        applyCascade(sos, x.doubleData(), out.doubleDataMut(), n, scratch);
        return out;
    }
    // 2D matrix: filter each column independently (MATLAB default).
    const size_t rows = x.dims().rows();
    const size_t cols = x.dims().cols();
    std::vector<double> scratch(rows);
    const double *src = x.doubleData();
    double *dst = out.doubleDataMut();
    for (size_t c = 0; c < cols; ++c)
        applyCascade(sos, src + c * rows, dst + c * rows, rows, scratch);
    return out;
}

std::tuple<MValue, double>
zp2sosWithGain(Allocator &alloc, const MValue &zerosV, const MValue &polesV, double gain)
{
    if (polesV.isEmpty())
        throw MError("zp2sos: at least one pole is required",
                     0, 0, "zp2sos", "", "m:zp2sos:noPoles");

    auto zeros = readComplexVec(zerosV, "zp2sos");
    auto poles = readComplexVec(polesV, "zp2sos");

    // Derive section count from pole count (rounded up for an odd
    // real pole). The same number of zero pairs are pulled; if
    // |zeros| < 2L, missing slots become unity numerators.
    const size_t L = (poles.size() + 1) / 2;

    std::vector<double> b1s(L), b2s(L), a1s(L), a2s(L);
    for (size_t s = 0; s < L; ++s) {
        RootPair polePair;
        if (!popPair(poles, polePair))
            throw MError("zp2sos: internal — ran out of poles",
                         0, 0, "zp2sos", "", "m:zp2sos:internal");
        const auto polQ = pairToQuad(polePair);
        a1s[s] = polQ.b1;
        a2s[s] = polQ.b2;

        // Match this pole pair to the closest zero pair (by centroid).
        if (!zeros.empty()) {
            RootPair zeroPair;
            (void) popClosestZeroPair(zeros, pairCentroid(polePair), zeroPair);
            const auto zerQ = pairToQuad(zeroPair);
            b1s[s] = zerQ.b1;
            b2s[s] = zerQ.b2;
        } else {
            b1s[s] = 0.0;
            b2s[s] = 0.0;
        }
    }
    if (!zeros.empty())
        throw MError("zp2sos: more zeros than poles is not supported",
                     0, 0, "zp2sos", "", "m:zp2sos:moreZeros");

    return std::make_tuple(buildSosMatrix(alloc, b1s, b2s, a1s, a2s, /*leadingGain=*/1.0),
                           gain);
}

MValue zp2sos(Allocator &alloc, const MValue &zerosV, const MValue &polesV, double gain)
{
    auto [sos, g] = zp2sosWithGain(alloc, zerosV, polesV, gain);
    // Distribute gain into the first section's b coefficients.
    const size_t L = sos.dims().rows();
    double *p = sos.doubleDataMut();
    p[0 * L + 0] *= g;
    p[1 * L + 0] *= g;
    p[2 * L + 0] *= g;
    return sos;
}

namespace {

// Reverse polynomial coefficients into MATLAB form for rootsDurandKerner
// (which expects c[0] = leading). Caller already has them in MATLAB
// order, so this is the identity — kept named for clarity.
inline std::vector<double> coeffsAsVector(const MValue &v)
{
    if (v.type() != MType::DOUBLE || !v.dims().isVector())
        throw MError("tf2sos: b/a must be DOUBLE row/column vectors",
                     0, 0, "tf2sos", "", "m:tf2sos:type");
    const size_t n = v.numel();
    std::vector<double> out(n);
    const double *p = v.doubleData();
    for (size_t i = 0; i < n; ++i) out[i] = p[i];
    return out;
}

} // namespace

std::tuple<MValue, double>
tf2sosWithGain(Allocator &alloc, const MValue &b, const MValue &a)
{
    auto bv = coeffsAsVector(b);
    auto av = coeffsAsVector(a);
    if (av.empty() || av[0] == 0.0)
        throw MError("tf2sos: a(1) must be nonzero",
                     0, 0, "tf2sos", "", "m:tf2sos:zeroLead");
    if (bv.empty())
        throw MError("tf2sos: b is empty",
                     0, 0, "tf2sos", "", "m:tf2sos:emptyB");

    const double gain = bv[0] / av[0];
    auto zeros = rootsDurandKerner(bv);
    auto poles = rootsDurandKerner(av);

    // Wrap roots into MValue COMPLEX vectors for zp2sos.
    auto toCplxVec = [&](const std::vector<Complex> &v) -> MValue {
        if (v.empty())
            return MValue::matrix(0, 0, MType::COMPLEX, &alloc);
        auto r = MValue::matrix(v.size(), 1, MType::COMPLEX, &alloc);
        Complex *p = r.complexDataMut();
        for (size_t i = 0; i < v.size(); ++i) p[i] = v[i];
        return r;
    };
    return zp2sosWithGain(alloc, toCplxVec(zeros), toCplxVec(poles), gain);
}

MValue tf2sos(Allocator &alloc, const MValue &b, const MValue &a)
{
    auto [sos, g] = tf2sosWithGain(alloc, b, a);
    const size_t L = sos.dims().rows();
    double *p = sos.doubleDataMut();
    p[0 * L + 0] *= g;
    p[1 * L + 0] *= g;
    p[2 * L + 0] *= g;
    return sos;
}

namespace detail {

void sosfilt_reg(Span<const MValue> args, size_t /*nargout*/,
                 Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("sosfilt: requires (sos, x)",
                     0, 0, "sosfilt", "", "m:sosfilt:nargin");
    outs[0] = sosfilt(ctx.engine->allocator(), args[0], args[1]);
}

void zp2sos_reg(Span<const MValue> args, size_t nargout,
                Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 3)
        throw MError("zp2sos: requires (z, p[, k])",
                     0, 0, "zp2sos", "", "m:zp2sos:nargin");
    const double gain = (args.size() >= 3 && !args[2].isEmpty())
                            ? args[2].toScalar()
                            : 1.0;
    auto &alloc = ctx.engine->allocator();
    if (nargout >= 2) {
        auto [sos, g] = zp2sosWithGain(alloc, args[0], args[1], gain);
        outs[0] = std::move(sos);
        outs[1] = MValue::scalar(g, &alloc);
    } else {
        outs[0] = zp2sos(alloc, args[0], args[1], gain);
    }
}

void tf2sos_reg(Span<const MValue> args, size_t nargout,
                Span<MValue> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw MError("tf2sos: requires (b, a)",
                     0, 0, "tf2sos", "", "m:tf2sos:nargin");
    auto &alloc = ctx.engine->allocator();
    if (nargout >= 2) {
        auto [sos, g] = tf2sosWithGain(alloc, args[0], args[1]);
        outs[0] = std::move(sos);
        outs[1] = MValue::scalar(g, &alloc);
    } else {
        outs[0] = tf2sos(alloc, args[0], args[1]);
    }
}

} // namespace detail

} // namespace numkit::m::dsp
