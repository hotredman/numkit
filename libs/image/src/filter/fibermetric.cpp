// libs/image/src/filter/fibermetric.cpp
//
// Multiscale Hessian-based vesselness filter (Frangi 1998 "Multiscale
// vessel enhancement filtering", MICCAI '98).
//
// Algorithm transliterated verbatim from MATLAB R2025b fibermetric.m
// + the internal C++ builtin images.internal.builtins.fibermetric.
//
// Per scale i ∈ thickness:
//   σ_i = thickness_i / 6.
//   Gaussian-smooth I with σ_i (FilterSize = 2·ceil(3σ_i) + 1).
//   Compute Hessian elements via central finite differences:
//     Ixx[r,c] = I[r,c+1] - 2·I[r,c] + I[r,c-1]
//     Iyy[r,c] = I[r+1,c] - 2·I[r,c] + I[r-1,c]
//     Ixy[r,c] = (I[r+1,c+1] - I[r+1,c-1] - I[r-1,c+1] + I[r-1,c-1])/4
//   Eigenvalues of [[Ixx Ixy];[Ixy Iyy]]:
//     trace = Ixx + Iyy, det = Ixx·Iyy − Ixy²
//     disc = sqrt((Ixx − Iyy)² + 4·Ixy²) / 2  (more stable)
//     λ⁻ = trace/2 − disc, λ⁺ = trace/2 + disc.
//     Sort so |λ₁| ≤ |λ₂|.
//   Vesselness:
//     if (bright && λ₂ > 0) || (dark && λ₂ < 0): V = 0.
//     else: Rβ = λ₁/λ₂; S = sqrt(λ₁² + λ₂²);
//           V = exp(-Rβ²/(2β²)) · (1 - exp(-S²/(2c²)));
//           β = 0.5 (Frangi default).
//
// 3-D variant: 3 eigenvalues |λ₁| ≤ |λ₂| ≤ |λ₃|.
//   Rα = |λ₂|/|λ₃|, Rβ = |λ₁|/sqrt(|λ₂·λ₃|), S² = sum(λᵢ²).
//   if (bright && λ₂>0||λ₃>0) || (dark && λ₂<0||λ₃<0): V = 0.
//   else: V = (1 - exp(-Rα²/(2α²))) · exp(-Rβ²/(2β²)) ·
//             (1 - exp(-S²/(2c²)));
//   α = β = 0.5.
//
// Per-pixel response is the MAX over all scales.
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.

#define _USE_MATH_DEFINES
#include <numkit/image/filter/filter.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

namespace numkit::image {
namespace {

constexpr double kFrangiBeta = 0.5;     // 2-D and 3-D blobness param
constexpr double kFrangiAlpha = 0.5;    // 3-D plate-vs-line param

// Replicate-boundary clamp.
inline std::size_t clamp_idx(long i, std::size_t n)
{
    if (i < 0) return 0;
    if (i >= static_cast<long>(n)) return n - 1;
    return static_cast<std::size_t>(i);
}

double class_range(ValueType t)
{
    switch (t) {
        case ValueType::UINT8:  return 255.0;
        case ValueType::UINT16: return 65535.0;
        case ValueType::UINT32: return 4294967295.0;
        case ValueType::INT8:   return 255.0;
        case ValueType::INT16:  return 65535.0;
        case ValueType::INT32:  return 4294967295.0;
        default:                return 1.0;        // single, double
    }
}

// ── Convert input to working buffer (vector of double) ────────────
struct Buf {
    std::size_t H = 0, W = 0, D = 1;
    std::vector<double> v;
    bool is3d() const { return D > 1; }
    double &at(std::size_t r, std::size_t c, std::size_t z = 0) {
        return v[z * H * W + c * H + r];
    }
    double at(std::size_t r, std::size_t c, std::size_t z = 0) const {
        return v[z * H * W + c * H + r];
    }
};

Buf to_double_buf(const Value &V)
{
    Buf b;
    b.H = V.dims().rows();
    b.W = V.dims().cols();
    b.D = V.dims().is3D() ? V.dims().pages() : 1;
    b.v.resize(b.H * b.W * b.D);
    const std::size_t N = b.v.size();
    for (std::size_t i = 0; i < N; ++i)
        b.v[i] = V.elemAsDouble(i);
    return b;
}

// ── 1-D Gaussian kernel, length = 2·radius+1 ─────────────────────
std::vector<double> gauss1d(double sigma, int radius)
{
    const int n = 2 * radius + 1;
    std::vector<double> k(static_cast<std::size_t>(n));
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        const double x = i - radius;
        k[static_cast<std::size_t>(i)] =
            std::exp(-0.5 * (x / sigma) * (x / sigma));
        s += k[static_cast<std::size_t>(i)];
    }
    for (double &v : k) v /= s;
    return k;
}

// Separable 2-D Gaussian filter, replicate boundary.
Buf gauss2d(const Buf &In, double sigma)
{
    const int rad = static_cast<int>(std::ceil(3.0 * sigma));
    std::vector<double> k = gauss1d(sigma, rad);
    const std::size_t H = In.H, W = In.W;
    Buf tmp; tmp.H = H; tmp.W = W; tmp.D = 1; tmp.v.assign(H * W, 0.0);
    Buf out = tmp;
    // Rows pass.
    for (std::size_t c = 0; c < W; ++c)
        for (std::size_t r = 0; r < H; ++r) {
            double s = 0.0;
            for (int j = -rad; j <= rad; ++j) {
                const std::size_t rr = clamp_idx(
                    static_cast<long>(r) + j, H);
                s += k[static_cast<std::size_t>(j + rad)] * In.at(rr, c);
            }
            tmp.at(r, c) = s;
        }
    // Cols pass.
    for (std::size_t c = 0; c < W; ++c)
        for (std::size_t r = 0; r < H; ++r) {
            double s = 0.0;
            for (int j = -rad; j <= rad; ++j) {
                const std::size_t cc = clamp_idx(
                    static_cast<long>(c) + j, W);
                s += k[static_cast<std::size_t>(j + rad)] * tmp.at(r, cc);
            }
            out.at(r, c) = s;
        }
    return out;
}

// Separable 3-D Gaussian filter, replicate boundary.
Buf gauss3d(const Buf &In, double sigma)
{
    const int rad = static_cast<int>(std::ceil(3.0 * sigma));
    std::vector<double> k = gauss1d(sigma, rad);
    const std::size_t H = In.H, W = In.W, D = In.D;
    Buf tmp; tmp.H = H; tmp.W = W; tmp.D = D; tmp.v.assign(H * W * D, 0.0);
    Buf tmp2 = tmp, out = tmp;
    // Rows.
    for (std::size_t z = 0; z < D; ++z)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t r = 0; r < H; ++r) {
                double s = 0.0;
                for (int j = -rad; j <= rad; ++j) {
                    const std::size_t rr = clamp_idx(
                        static_cast<long>(r) + j, H);
                    s += k[static_cast<std::size_t>(j + rad)] * In.at(rr, c, z);
                }
                tmp.at(r, c, z) = s;
            }
    // Cols.
    for (std::size_t z = 0; z < D; ++z)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t r = 0; r < H; ++r) {
                double s = 0.0;
                for (int j = -rad; j <= rad; ++j) {
                    const std::size_t cc = clamp_idx(
                        static_cast<long>(c) + j, W);
                    s += k[static_cast<std::size_t>(j + rad)] * tmp.at(r, cc, z);
                }
                tmp2.at(r, c, z) = s;
            }
    // Slices.
    for (std::size_t z = 0; z < D; ++z)
        for (std::size_t c = 0; c < W; ++c)
            for (std::size_t r = 0; r < H; ++r) {
                double s = 0.0;
                for (int j = -rad; j <= rad; ++j) {
                    const std::size_t zz = clamp_idx(
                        static_cast<long>(z) + j, D);
                    s += k[static_cast<std::size_t>(j + rad)] * tmp2.at(r, c, zz);
                }
                out.at(r, c, z) = s;
            }
    return out;
}

// ── 2-D Frangi vesselness at one scale ─────────────────────────────
// Hessian elements are γ-normalized by σ² (Frangi 1998, "Multiscale
// vessel enhancement filtering", Eq. before Eq. 7: γ = 2 for vessels).
Buf frangi2d(const Buf &Ig, double c, bool bright, double sigma)
{
    const std::size_t H = Ig.H, W = Ig.W;
    Buf out; out.H = H; out.W = W; out.D = 1; out.v.assign(H * W, 0.0);
    const double beta_sq2 = 2.0 * kFrangiBeta * kFrangiBeta;
    const double c_sq2    = 2.0 * c * c;
    const double sigma2   = sigma * sigma;
    for (std::size_t cc = 0; cc < W; ++cc) {
        for (std::size_t r = 0; r < H; ++r) {
            const std::size_t rm = clamp_idx(static_cast<long>(r) - 1, H);
            const std::size_t rp = clamp_idx(static_cast<long>(r) + 1, H);
            const std::size_t cm = clamp_idx(static_cast<long>(cc) - 1, W);
            const std::size_t cp = clamp_idx(static_cast<long>(cc) + 1, W);
            const double v   = Ig.at(r,  cc);
            const double Ixx = sigma2 * (Ig.at(r,  cp) - 2.0 * v + Ig.at(r,  cm));
            const double Iyy = sigma2 * (Ig.at(rp, cc) - 2.0 * v + Ig.at(rm, cc));
            const double Ixy = sigma2 * 0.25 *
                                      ( Ig.at(rp, cp) - Ig.at(rp, cm)
                                      - Ig.at(rm, cp) + Ig.at(rm, cm));
            // Eigenvalues of 2x2 symmetric: trace/2 ± sqrt((diff/2)² + Ixy²).
            const double mean = 0.5 * (Ixx + Iyy);
            const double diff = 0.5 * (Ixx - Iyy);
            const double disc = std::sqrt(diff * diff + Ixy * Ixy);
            double l1 = mean - disc;
            double l2 = mean + disc;
            // Order by |.|: |λ1| ≤ |λ2|.
            if (std::fabs(l1) > std::fabs(l2)) std::swap(l1, l2);
            // Polarity check.
            if (bright) { if (l2 > 0.0) { out.at(r, cc) = 0.0; continue; } }
            else        { if (l2 < 0.0) { out.at(r, cc) = 0.0; continue; } }
            if (l2 == 0.0) { out.at(r, cc) = 0.0; continue; }
            const double Rb_sq = (l1 / l2) * (l1 / l2);
            const double S_sq  = l1 * l1 + l2 * l2;
            const double V = std::exp(-Rb_sq / beta_sq2)
                           * (1.0 - std::exp(-S_sq / c_sq2));
            out.at(r, cc) = V;
        }
    }
    return out;
}

// ── 3-D Frangi vesselness at one scale ─────────────────────────────
// 3x3 symmetric eigen decomposition via the closed-form trig solution
// (Smith, 1961; widely cited for 3x3 symmetric Hessians).
void eigvals_3x3(double a11, double a22, double a33,
                 double a12, double a13, double a23,
                 double &l1, double &l2, double &l3)
{
    // Coefficients of characteristic polynomial λ³ − tr·λ² + ... − det.
    const double p1 = a12 * a12 + a13 * a13 + a23 * a23;
    if (p1 == 0.0) {
        // Diagonal already.
        l1 = a11; l2 = a22; l3 = a33;
    } else {
        const double q = (a11 + a22 + a33) / 3.0;
        const double p2 = (a11 - q) * (a11 - q)
                        + (a22 - q) * (a22 - q)
                        + (a33 - q) * (a33 - q)
                        + 2.0 * p1;
        const double p = std::sqrt(p2 / 6.0);
        if (p == 0.0) { l1 = l2 = l3 = q; return; }
        const double b11 = (a11 - q) / p;
        const double b22 = (a22 - q) / p;
        const double b33 = (a33 - q) / p;
        const double b12 = a12 / p;
        const double b13 = a13 / p;
        const double b23 = a23 / p;
        // det(B) / 2
        const double r = 0.5 * (b11 * (b22 * b33 - b23 * b23)
                               - b12 * (b12 * b33 - b23 * b13)
                               + b13 * (b12 * b23 - b22 * b13));
        double rcl = r;
        if (rcl < -1.0) rcl = -1.0;
        if (rcl >  1.0) rcl =  1.0;
        const double phi = std::acos(rcl) / 3.0;
        l1 = q + 2.0 * p * std::cos(phi);
        l3 = q + 2.0 * p * std::cos(phi + 2.0 * M_PI / 3.0);
        l2 = 3.0 * q - l1 - l3;     // trace constraint
    }
    // Sort by absolute value: |l1| ≤ |l2| ≤ |l3|.
    if (std::fabs(l1) > std::fabs(l2)) std::swap(l1, l2);
    if (std::fabs(l2) > std::fabs(l3)) std::swap(l2, l3);
    if (std::fabs(l1) > std::fabs(l2)) std::swap(l1, l2);
}

Buf frangi3d(const Buf &Ig, double c, bool bright, double sigma)
{
    const std::size_t H = Ig.H, W = Ig.W, D = Ig.D;
    Buf out; out.H = H; out.W = W; out.D = D; out.v.assign(H * W * D, 0.0);
    const double alpha_sq2 = 2.0 * kFrangiAlpha * kFrangiAlpha;
    const double beta_sq2  = 2.0 * kFrangiBeta  * kFrangiBeta;
    const double c_sq2     = 2.0 * c * c;
    const double sigma2    = sigma * sigma;
    for (std::size_t z = 0; z < D; ++z) {
        const std::size_t zm = clamp_idx(static_cast<long>(z) - 1, D);
        const std::size_t zp = clamp_idx(static_cast<long>(z) + 1, D);
        for (std::size_t cc = 0; cc < W; ++cc) {
            const std::size_t cm = clamp_idx(static_cast<long>(cc) - 1, W);
            const std::size_t cp = clamp_idx(static_cast<long>(cc) + 1, W);
            for (std::size_t r = 0; r < H; ++r) {
                const std::size_t rm = clamp_idx(static_cast<long>(r) - 1, H);
                const std::size_t rp = clamp_idx(static_cast<long>(r) + 1, H);
                const double v   = Ig.at(r, cc, z);
                const double Ixx = sigma2 * (Ig.at(r,  cp, z) - 2.0 * v + Ig.at(r,  cm, z));
                const double Iyy = sigma2 * (Ig.at(rp, cc, z) - 2.0 * v + Ig.at(rm, cc, z));
                const double Izz = sigma2 * (Ig.at(r,  cc, zp) - 2.0 * v + Ig.at(r,  cc, zm));
                const double Ixy = sigma2 * 0.25 *
                                          (Ig.at(rp, cp, z) - Ig.at(rp, cm, z)
                                         - Ig.at(rm, cp, z) + Ig.at(rm, cm, z));
                const double Ixz = sigma2 * 0.25 *
                                          (Ig.at(r,  cp, zp) - Ig.at(r,  cm, zp)
                                         - Ig.at(r,  cp, zm) + Ig.at(r,  cm, zm));
                const double Iyz = sigma2 * 0.25 *
                                          (Ig.at(rp, cc, zp) - Ig.at(rm, cc, zp)
                                         - Ig.at(rp, cc, zm) + Ig.at(rm, cc, zm));
                double l1, l2, l3;
                eigvals_3x3(Ixx, Iyy, Izz, Ixy, Ixz, Iyz, l1, l2, l3);
                // Polarity: require both l2 and l3 to have the right sign.
                if (bright) {
                    if (l2 > 0.0 || l3 > 0.0) continue;
                } else {
                    if (l2 < 0.0 || l3 < 0.0) continue;
                }
                if (l2 == 0.0 || l3 == 0.0) continue;
                const double Ra_sq  = (l2 / l3) * (l2 / l3);
                const double prod23 = std::fabs(l2 * l3);
                if (prod23 == 0.0) continue;
                const double Rb_sq = (l1 * l1) / prod23;
                const double S_sq  = l1 * l1 + l2 * l2 + l3 * l3;
                const double V = (1.0 - std::exp(-Ra_sq / alpha_sq2))
                               * std::exp(-Rb_sq / beta_sq2)
                               * (1.0 - std::exp(-S_sq / c_sq2));
                out.at(r, cc, z) = V;
            }
        }
    }
    return out;
}

// Cast result buffer to output Value (single for non-double inputs,
// double for double).
Value buf_to_value(const Buf &B, ValueType targetClass,
                   std::pmr::memory_resource *mr)
{
    const std::size_t N = B.v.size();
    Value out;
    if (B.is3d())
        out = Value::matrix3d(B.H, B.W, B.D, targetClass, mr);
    else
        out = Value::matrix(B.H, B.W, targetClass, mr);
    if (targetClass == ValueType::DOUBLE) {
        std::memcpy(out.doubleDataMut(), B.v.data(), N * sizeof(double));
    } else {
        // SINGLE
        float *od = out.singleDataMut();
        for (std::size_t i = 0; i < N; ++i)
            od[i] = static_cast<float>(B.v[i]);
    }
    return out;
}

}  // namespace

Value fibermetric(const Value &I, const std::vector<double> &thickness_in,
                  double structure_sensitivity, bool bright_polarity,
                  std::pmr::memory_resource *mr)
{
    const auto &d = I.dims();
    if (d.rows() < 2 || d.cols() < 2)
        throw Error("fibermetric: I must be 2-D or 3-D with all dims >= 2",
                    0, 0, "fibermetric", "", "numkit:fibermetric:shape");
    const bool is3d = d.is3D() && d.pages() > 1;
    if (is3d && d.pages() < 2)
        throw Error("fibermetric: 3-D input must have at least 2 slices",
                    0, 0, "fibermetric", "", "numkit:fibermetric:slices");

    // Default thickness.
    std::vector<double> thickness = thickness_in;
    if (thickness.empty()) thickness = {4, 6, 8, 10, 12, 14};
    for (double t : thickness)
        if (!std::isfinite(t) || t <= 0.0 || std::floor(t) != t)
            throw Error("fibermetric: thickness must be a vector of positive "
                        "integers",
                        0, 0, "fibermetric", "",
                        "numkit:fibermetric:thickness");

    // Default c = range/100.
    double c = structure_sensitivity;
    if (c <= 0.0) c = class_range(I.type()) / 100.0;
    if (!std::isfinite(c) || c <= 0.0)
        throw Error("fibermetric: StructureSensitivity must be > 0",
                    0, 0, "fibermetric", "",
                    "numkit:fibermetric:sens");

    // Output class.
    const ValueType outClass = (I.type() == ValueType::DOUBLE)
                             ? ValueType::DOUBLE : ValueType::SINGLE;

    Buf In = to_double_buf(I);
    Buf Out; Out.H = In.H; Out.W = In.W; Out.D = In.D;
    Out.v.assign(In.v.size(), 0.0);

    for (double t : thickness) {
        const double sigma = t / 6.0;
        Buf G = is3d ? gauss3d(In, sigma) : gauss2d(In, sigma);
        Buf V = is3d ? frangi3d(G, c, bright_polarity, sigma)
                     : frangi2d(G, c, bright_polarity, sigma);
        for (std::size_t i = 0; i < Out.v.size(); ++i)
            if (V.v[i] > Out.v[i]) Out.v[i] = V.v[i];
    }
    return buf_to_value(Out, outClass, mr);
}

} // namespace numkit::image
