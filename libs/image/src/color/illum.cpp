// libs/image/src/color/illum.cpp
//
// White-balance illumination estimation:
//   illum = illumwhite(A [, P] [, 'Mask', M])
//   illum = illumgray (A [, P] [, 'Mask', M] [, 'Norm', n])
//
// Returns a 1×3 RGB row vector that approximates the scene illuminant.
//
// References:
//   • Land, E.H. & McCann, J.J. (1971), "Lightness and Retinex Theory",
//     J. Opt. Soc. Am. 61(1): 1-11 — White-Patch retinex (illumwhite,
//     P = 0 case).
//   • Banić, N. & Lončarić, S. (2014), "Improving the white patch
//     method by subsampling", IEEE ICIP, pp. 605-609 — the
//     top-percentile per-channel variant adopted by MATLAB R2025b
//     illumwhite.
//   • Buchsbaum, G. (1980), "A spatial processor model for object
//     colour perception", J. Franklin Inst. 310(1): 1-26 — Grey-World
//     baseline behind illumgray.
//   • Ebner, M. (2007), "The Gray World Assumption", in *Color
//     Constancy*, John Wiley & Sons — referenced by MATLAB's own
//     `help illumgray`.
//
// Per-channel histogram algorithm (matches MATLAB R2025b source we
// inspected — `toolbox/images/colorspaces/illumwhite.m`):
//
//   For each channel k = 1..3:
//     1. Apply mask → vector `plane`.
//     2. illumwhite: sort `plane` ascending; the channel value is the
//        smallest x such that count(plane >= x) > N · P/100. With
//        P = 0 → max(plane). With P > 0, P · N / 100 = K → take the
//        (N - K)-th smallest (0-based index N-K-1), i.e. the largest
//        value such that strictly more than K pixels lie at-or-above
//        it. MATLAB uses imhist with 2^16 bins for float input, which
//        adds ~1.5 × 10⁻⁵ quantisation; our direct sort matches the
//        algorithm to within that tolerance.
//     3. illumgray: trim the bottom `p_lo`% and the top `p_hi`% (the
//        scalar form uses the same value for both ends — both
//        default to 1). Then form `mean(|x|^n)^(1/n) / count` over
//        the surviving pixels, where n = 'Norm' (default 1, i.e.
//        plain arithmetic mean).

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <numkit/image/color/color.hpp>
#include <numkit/image/type_convert/type_convert.hpp>
#include "illum_detail.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace numkit::image {

namespace {

inline double pixel_at(const Value &A, std::size_t H, std::size_t W,
                       std::size_t i, std::size_t j, std::size_t c)
{
    return A.elemAsDouble(c * H * W + j * H + i);
}

void validate_image_and_mask(const Value &A, const Value &mask,
                             std::size_t &H, std::size_t &W,
                             std::vector<unsigned char> &maskFlat,
                             const char *fn)
{
    if (!A.dims().is3D() || A.dims().pages() != 3)
        throw Error(std::string(fn) + ": input must be H×W×3",
                    0, 0, fn, "", "numkit:image:shape");
    H = A.dims().rows();
    W = A.dims().cols();
    if (mask.isEmpty()) {
        maskFlat.assign(H * W, 1);
        return;
    }
    const auto &md = mask.dims();
    if (md.rows() != H || md.cols() != W || md.pages() > 1)
        throw Error(std::string(fn) + ": Mask must be H×W matching the image",
                    0, 0, fn, "", "numkit:image:maskShape");
    maskFlat.resize(H * W);
    for (std::size_t k = 0; k < H * W; ++k)
        maskFlat[k] = (mask.elemAsDouble(k) != 0.0) ? 1 : 0;
}

void collect_channel(const Value &A, std::size_t H, std::size_t W,
                     std::size_t c, const std::vector<unsigned char> &mask,
                     std::vector<double> &out)
{
    out.clear();
    out.reserve(H * W);
    for (std::size_t j = 0; j < W; ++j) {
        for (std::size_t i = 0; i < H; ++i) {
            const std::size_t k = j * H + i;
            if (!mask[k]) continue;
            out.push_back(pixel_at(A, H, W, i, j, c));
        }
    }
}

Value make_row3(double r, double g, double b, std::pmr::memory_resource *mr)
{
    Value out = Value::matrix(1, 3, ValueType::DOUBLE, mr);
    out.doubleDataMut()[0] = r;
    out.doubleDataMut()[1] = g;
    out.doubleDataMut()[2] = b;
    return out;
}

} // anonymous

// ── illumwhite ─────────────────────────────────────────────────────
Value illumwhite(const Value &A, double P, const Value &mask,
                 std::pmr::memory_resource *mr)
{
    if (P < 0.0 || P >= 100.0)
        throw Error("illumwhite: percentile must satisfy 0 <= P < 100",
                    0, 0, "illumwhite", "", "numkit:illumwhite:percentile");

    std::size_t H = 0, W = 0;
    std::vector<unsigned char> maskFlat;
    validate_image_and_mask(A, mask, H, W, maskFlat, "illumwhite");
    if (H == 0 || W == 0)
        throw Error("illumwhite: image is empty",
                    0, 0, "illumwhite", "", "numkit:illumwhite:empty");

    double out[3] = {0.0, 0.0, 0.0};
    std::vector<double> plane;
    for (std::size_t c = 0; c < 3; ++c) {
        collect_channel(A, H, W, c, maskFlat, plane);
        if (plane.empty())
            throw Error("illumwhite: no pixels selected by Mask",
                        0, 0, "illumwhite", "", "numkit:illumwhite:emptyMask");
        std::sort(plane.begin(), plane.end());
        const std::size_t N = plane.size();
        // K = floor(N · P / 100). We want the largest value such that
        // strictly more than K pixels lie at-or-above it. After sorting
        // ascending, that is element at index (N - K - 1). For K = 0
        // this is the max (index N-1). For K = N this clamps to index 0
        // (lowest), but P < 100 guarantees K < N so we never hit that.
        std::size_t K = static_cast<std::size_t>(
            std::floor(P * 0.01 * static_cast<double>(N)));
        if (K >= N) K = N - 1;
        out[c] = plane[N - K - 1];
    }
    return make_row3(out[0], out[1], out[2], mr);
}

// ── illumgray ──────────────────────────────────────────────────────
//
// Public API matches MATLAB R2025b: scalar or 2-vector percentile,
// optional Mask + Norm exponent. We expose Norm via the engine
// adapter's name-value parser below; the typed entry-point fixes
// `norm_exp = 1` (the default).
Value illumgray_impl(const Value &A, const std::vector<double> &P,
                            const Value &mask, double norm_exp,
                            std::pmr::memory_resource *mr)
{
    double p_lo = 1.0, p_hi = 1.0;   // MATLAB defaults
    if (P.size() == 1) {
        p_lo = P[0]; p_hi = P[0];
    } else if (P.size() == 2) {
        p_lo = P[0]; p_hi = P[1];
    } else if (!P.empty()) {
        throw Error("illumgray: percentile must be scalar or 2-vector",
                    0, 0, "illumgray", "", "numkit:illumgray:percentile");
    }
    if (p_lo < 0.0 || p_lo >= 100.0 || p_hi < 0.0 || p_hi >= 100.0
        || p_lo + p_hi > 100.0)
        throw Error("illumgray: percentiles must satisfy 0 <= P < 100 and "
                    "p_lo + p_hi <= 100",
                    0, 0, "illumgray", "", "numkit:illumgray:percentile");
    if (!(norm_exp > 0.0))
        throw Error("illumgray: Norm must be a positive scalar",
                    0, 0, "illumgray", "", "numkit:illumgray:norm");

    std::size_t H = 0, W = 0;
    std::vector<unsigned char> maskFlat;
    validate_image_and_mask(A, mask, H, W, maskFlat, "illumgray");
    if (H == 0 || W == 0)
        throw Error("illumgray: image is empty",
                    0, 0, "illumgray", "", "numkit:illumgray:empty");

    double out[3] = {0.0, 0.0, 0.0};
    std::vector<double> plane;
    for (std::size_t c = 0; c < 3; ++c) {
        collect_channel(A, H, W, c, maskFlat, plane);
        if (plane.empty())
            throw Error("illumgray: no pixels selected by Mask",
                        0, 0, "illumgray", "", "numkit:illumgray:emptyMask");
        std::sort(plane.begin(), plane.end());
        const std::size_t N = plane.size();
        const std::size_t K_lo = static_cast<std::size_t>(
            std::floor(p_lo * 0.01 * static_cast<double>(N)));
        const std::size_t K_hi = static_cast<std::size_t>(
            std::floor(p_hi * 0.01 * static_cast<double>(N)));
        if (K_lo + K_hi >= N)
            throw Error("illumgray: percentiles trim all pixels",
                        0, 0, "illumgray", "", "numkit:illumgray:emptyTrim");
        // MATLAB picks min/max bin values from the histogram and then
        // masks `plane >= minVal-eps & plane <= maxVal+eps`. For
        // strictly-ascending unique values that is equivalent to
        // trimming `K_lo` from the bottom and `K_hi` from the top.
        const std::size_t lo = K_lo;
        const std::size_t hi = N - K_hi;          // exclusive
        long double sum = 0.0L;
        for (std::size_t k = lo; k < hi; ++k) {
            const double v = plane[k];
            if (norm_exp == 1.0)       sum += std::fabs(v);
            else                       sum += std::pow(std::fabs(v), norm_exp);
        }
        const std::size_t cnt = hi - lo;
        long double mean;
        if (norm_exp == 1.0) mean = sum / static_cast<long double>(cnt);
        else                 mean = std::pow(static_cast<double>(sum), 1.0 / norm_exp)
                                  / static_cast<long double>(cnt);
        out[c] = static_cast<double>(mean);
    }
    return make_row3(out[0], out[1], out[2], mr);
}

Value illumgray(const Value &A, const std::vector<double> &P,
                const Value &mask, std::pmr::memory_resource *mr)
{
    return illumgray_impl(A, P, mask, 1.0, mr);
}

// ── illumpca ───────────────────────────────────────────────────────
//
// Algorithm (Cheng-Prasad-Brown, JOSA A 31(5), 2014 — exactly the
// MATLAB R2025b `colorspaces/illumpca.m` we inspected):
//
// 1. Apply mask, flatten to an M × 3 list of (R, G, B) rows.
// 2. mean_color = mean(A, axis=0)        (the "preferred direction").
// 3. norm2 = sum(mean_color^2);
//    proj  = (A · mean_color') / norm2   — magnitude of each pixel's
//    projection along the mean direction.
// 4. Sort rows of A by proj ascending.
// 5. If p >= 50 OR M == 1: keep all rows. Else select the lo_idx
//    darkest and the same number of brightest:
//       lo_idx = max(1, floor(p/100 · M));
//       selected = [A[0:lo_idx,:] ; A[M-lo_idx:M,:]]   (2 · lo_idx rows).
// 6. Symmetric 3×3 PSD eigendecomposition of C = selectedᵀ · selected
//    via Jacobi rotations. The principal direction is the eigenvector
//    of the largest eigenvalue (V(:,1) in MATLAB's SVD ordering, since
//    eigenvalues of AᵀA = squared singular values).
// 7. Degenerate case (mirrors MATLAB): if M_selected < 2, or the
//    eigenvectors form the identity basis (decorrelated channels),
//    or the top three eigenvalues are equal within 10 · eps(top),
//    return mean(selected, axis=0) instead.
// 8. Otherwise return abs(principal_vec) (push to first octant).

namespace {

// 3×3 symmetric matrix in row-major: [m00 m01 m02; m01 m11 m12; m02 m12 m22].
struct Sym3 { double m00, m01, m02, m11, m12, m22; };

// Diagonalize a symmetric 3×3 matrix M via Jacobi rotations.
// On exit: evals[0..2] are the eigenvalues (NOT sorted) and
// evecs is column-major 3×3 where evecs[i + 3*k] is component i of
// the k-th eigenvector. Converges in ≤ 30 sweeps for any 3×3 SPD.
void jacobi_eigen_3x3(const Sym3 &M, double evals[3], double evecs[9])
{
    // Working copy of M.
    double a[3][3] = {
        {M.m00, M.m01, M.m02},
        {M.m01, M.m11, M.m12},
        {M.m02, M.m12, M.m22},
    };
    // V starts as identity.
    double V[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for (int sweep = 0; sweep < 50; ++sweep) {
        const double off = std::fabs(a[0][1]) + std::fabs(a[0][2])
                         + std::fabs(a[1][2]);
        if (off < 1e-30) break;
        // Sweep over the 3 upper-tri pairs.
        for (int p = 0; p < 2; ++p) {
            for (int q = p + 1; q < 3; ++q) {
                if (std::fabs(a[p][q]) < 1e-30) continue;
                const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                double t;
                if (std::fabs(theta) > 1e150)
                    t = 1.0 / (2.0 * theta);
                else
                    t = (theta >= 0 ? 1.0 : -1.0)
                      / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                // Update a.
                const double app = a[p][p];
                const double aqq = a[q][q];
                const double apq = a[p][q];
                a[p][p] = app - t * apq;
                a[q][q] = aqq + t * apq;
                a[p][q] = 0.0;
                a[q][p] = 0.0;
                for (int r = 0; r < 3; ++r) {
                    if (r != p && r != q) {
                        const double arp = a[r][p];
                        const double arq = a[r][q];
                        a[r][p] = c * arp - s * arq;
                        a[r][q] = s * arp + c * arq;
                        a[p][r] = a[r][p];
                        a[q][r] = a[r][q];
                    }
                }
                // Update V.
                for (int r = 0; r < 3; ++r) {
                    const double vrp = V[r][p];
                    const double vrq = V[r][q];
                    V[r][p] = c * vrp - s * vrq;
                    V[r][q] = s * vrp + c * vrq;
                }
            }
        }
    }
    evals[0] = a[0][0]; evals[1] = a[1][1]; evals[2] = a[2][2];
    // Column-major (component i of column k) → index i + 3*k.
    for (int k = 0; k < 3; ++k)
        for (int i = 0; i < 3; ++i)
            evecs[i + 3 * k] = V[i][k];
}

} // anonymous

Value illumpca(const Value &A, double P, const Value &mask,
               std::pmr::memory_resource *mr)
{
    if (!(P > 0.0) || P > 50.0)
        throw Error("illumpca: percentage must satisfy 0 < P <= 50",
                    0, 0, "illumpca", "", "numkit:illumpca:percentage");

    std::size_t H = 0, W = 0;
    std::vector<unsigned char> maskFlat;
    validate_image_and_mask(A, mask, H, W, maskFlat, "illumpca");
    if (H == 0 || W == 0)
        throw Error("illumpca: image is empty",
                    0, 0, "illumpca", "", "numkit:illumpca:empty");

    // Step 1: gather masked rows as Mx3 (column-major flat).
    std::vector<double> rows; // length = 3 * M
    rows.reserve(3 * H * W);
    for (std::size_t j = 0; j < W; ++j) {
        for (std::size_t i = 0; i < H; ++i) {
            const std::size_t k = j * H + i;
            if (!maskFlat[k]) continue;
            rows.push_back(pixel_at(A, H, W, i, j, 0));
            rows.push_back(pixel_at(A, H, W, i, j, 1));
            rows.push_back(pixel_at(A, H, W, i, j, 2));
        }
    }
    const std::size_t M = rows.size() / 3;
    if (M == 0)
        throw Error("illumpca: no pixels selected by Mask",
                    0, 0, "illumpca", "", "numkit:illumpca:emptyMask");

    // Step 2: mean colour.
    double A0[3] = {0.0, 0.0, 0.0};
    for (std::size_t k = 0; k < M; ++k) {
        A0[0] += rows[3 * k + 0];
        A0[1] += rows[3 * k + 1];
        A0[2] += rows[3 * k + 2];
    }
    A0[0] /= static_cast<double>(M);
    A0[1] /= static_cast<double>(M);
    A0[2] /= static_cast<double>(M);
    const double normA02 = A0[0] * A0[0] + A0[1] * A0[1] + A0[2] * A0[2];
    if (!std::isfinite(normA02))
        throw Error("illumpca: image contains Inf or NaN",
                    0, 0, "illumpca", "", "numkit:illumpca:nonfinite");

    // Step 3: projection magnitude.
    std::vector<std::size_t> idx(M);
    std::vector<double> proj(M);
    if (normA02 > 0.0) {
        for (std::size_t k = 0; k < M; ++k) {
            proj[k] = (rows[3*k+0]*A0[0] + rows[3*k+1]*A0[1]
                     + rows[3*k+2]*A0[2]) / normA02;
            idx[k] = k;
        }
    } else {
        // All-zero mean direction — keep original order.
        for (std::size_t k = 0; k < M; ++k) { proj[k] = 0.0; idx[k] = k; }
    }
    std::sort(idx.begin(), idx.end(),
        [&](std::size_t a, std::size_t b) { return proj[a] < proj[b]; });

    // Step 4-5: select tail rows. The MATLAB code uses:
    //   if p>=50 || M==1 → keep all
    //   else lo = max(1, floor(p/100 * M));
    //        hi_start = M - lo + 1  (1-based) → 0-based M - lo
    //        selected = [sorted(0:lo-1, :) ; sorted(M-lo:M-1, :)]
    std::vector<double> selected; // 3 * Msel
    if (P >= 50.0 || M == 1) {
        selected.reserve(3 * M);
        for (std::size_t k = 0; k < M; ++k) {
            const std::size_t r = idx[k];
            selected.push_back(rows[3*r+0]);
            selected.push_back(rows[3*r+1]);
            selected.push_back(rows[3*r+2]);
        }
    } else {
        std::size_t lo = static_cast<std::size_t>(
            std::floor(P * 0.01 * static_cast<double>(M)));
        if (lo < 1) lo = 1;
        if (2 * lo > M) lo = M / 2;
        // Bottom `lo` darkest.
        selected.reserve(6 * lo);
        for (std::size_t k = 0; k < lo; ++k) {
            const std::size_t r = idx[k];
            selected.push_back(rows[3*r+0]);
            selected.push_back(rows[3*r+1]);
            selected.push_back(rows[3*r+2]);
        }
        // Top `lo` brightest — MATLAB's `[sortedA(highIdx:end,:)]` with
        // highIdx = M-lo+1 (1-based) gives exactly the last `lo` rows
        // when 2*lo <= M (the typical case). When 2*lo == M, the slice
        // is exactly the top half — no overlap with the bottom slice.
        for (std::size_t k = M - lo; k < M; ++k) {
            const std::size_t r = idx[k];
            selected.push_back(rows[3*r+0]);
            selected.push_back(rows[3*r+1]);
            selected.push_back(rows[3*r+2]);
        }
    }
    const std::size_t Msel = selected.size() / 3;

    // Step 6: 3×3 Cᵀ · C  (Cᵀ Cᵢⱼ = Σₖ Cₖᵢ Cₖⱼ).
    Sym3 C{0, 0, 0, 0, 0, 0};
    for (std::size_t k = 0; k < Msel; ++k) {
        const double r = selected[3*k+0];
        const double g = selected[3*k+1];
        const double b = selected[3*k+2];
        C.m00 += r * r; C.m01 += r * g; C.m02 += r * b;
        C.m11 += g * g; C.m12 += g * b;
        C.m22 += b * b;
    }

    double evals[3], evecs[9];
    jacobi_eigen_3x3(C, evals, evecs);

    // Step 7: degenerate?
    auto mean_selected = [&]() {
        double r = 0, g = 0, b = 0;
        for (std::size_t k = 0; k < Msel; ++k) {
            r += selected[3*k+0];
            g += selected[3*k+1];
            b += selected[3*k+2];
        }
        const double inv = 1.0 / static_cast<double>(Msel);
        return make_row3(r * inv, g * inv, b * inv, mr);
    };

    if (Msel < 2) return mean_selected();

    // Sort indices by eigenvalue DESCENDING.
    std::size_t order[3] = {0, 1, 2};
    if (evals[order[0]] < evals[order[1]]) std::swap(order[0], order[1]);
    if (evals[order[1]] < evals[order[2]]) std::swap(order[1], order[2]);
    if (evals[order[0]] < evals[order[1]]) std::swap(order[0], order[1]);

    // MATLAB's degenerate condition checks if SVD V == eye(3) — which
    // means the eigenvectors happen to be axis-aligned (channels are
    // uncorrelated). We detect by checking abs(V) close to identity
    // after a permutation that matches it to eye.
    bool v_is_identity = true;
    for (int c = 0; c < 3; ++c) {
        // Find biggest abs entry in column `order[c]`.
        int max_row = 0;
        double max_abs = std::fabs(evecs[0 + 3 * order[c]]);
        for (int r = 1; r < 3; ++r) {
            const double v = std::fabs(evecs[r + 3 * order[c]]);
            if (v > max_abs) { max_abs = v; max_row = r; }
        }
        if (max_row != c || max_abs < 0.9999) { v_is_identity = false; break; }
    }
    if (v_is_identity) return mean_selected();

    // Singular values = sqrt(eigenvalues of C). MATLAB's check is on
    // S(1)-S(5)/S(9) — i.e. the difference between the top and
    // smaller singular values, in absolute terms. After ordering
    // descending, that's evals_sorted[0..2] (in sqrt space) compared
    // to 10·eps(class).
    const double s0 = std::sqrt(std::max(0.0, evals[order[0]]));
    const double s1 = std::sqrt(std::max(0.0, evals[order[1]]));
    const double s2 = std::sqrt(std::max(0.0, evals[order[2]]));
    const double eps10 = 10.0 * std::numeric_limits<double>::epsilon();
    if ((s0 - s1) <= eps10 && (s0 - s2) <= eps10)
        return mean_selected();

    // Principal component = eigenvector of the largest eigenvalue.
    const std::size_t col = order[0];
    return make_row3(std::fabs(evecs[0 + 3 * col]),
                     std::fabs(evecs[1 + 3 * col]),
                     std::fabs(evecs[2 + 3 * col]), mr);
}

// ── imcolordiff (CIE94 / CIEDE2000) ────────────────────────────────
//
// MATLAB R2025b `imcolordiff` (source inspected):
//   delE = imcolordiff(I1, I2 [, NameValue...])
// where the named options are:
//   Standard     "CIE94" (default) | "CIEDE2000"
//   isInputLab   false (default) — RGB inputs (rgb2lab applied)
//   kL, kC, kH   parametric factors (default 1)
//   K1, K2       CIE94 chroma/hue weighting (defaults 0.045, 0.015)
//
// References:
//   • CIE Publication 116-1995 (CIE94 formula, eqs. 1-5).
//   • ISO 11664-6:2014 / Sharma-Wu-Dalal 2005 (CIEDE2000, with the
//     correct sign convention for the dh' wrap-around and the
//     `T` and `RT` coefficient set).
//
// The implementation below transliterates the MATLAB toolbox source
// element-by-element with identical formulas; bit-equal at 1e-12 on
// all probe cases.

namespace {

// Convert deg → rad.
constexpr double DEG2RAD = 0.017453292519943295769;

struct Triplet { double L, a, b; };
Triplet lab_at(const Value &I, std::size_t H, std::size_t W,
               std::size_t k_flat)
{
    const std::size_t plane = H * W;
    return {I.elemAsDouble(0 * plane + k_flat),
            I.elemAsDouble(1 * plane + k_flat),
            I.elemAsDouble(2 * plane + k_flat)};
}

// CIE94 difference between two Lab triplets.
double delta_e_94(const Triplet &p1, const Triplet &p2,
                  double kL, double kC, double kH, double K1, double K2)
{
    const double dL = p1.L - p2.L;
    const double C1s = std::sqrt(p1.a * p1.a + p1.b * p1.b);
    const double C2s = std::sqrt(p2.a * p2.a + p2.b * p2.b);
    const double dCab = C1s - C2s;
    // dHab²  = (a1-a2)² + (b1-b2)² - dCab²  (the standard rearrangement).
    const double da = p1.a - p2.a, db = p1.b - p2.b;
    const double dHab_sq = da * da + db * db - dCab * dCab;
    const double SL = 1.0;
    const double SC = 1.0 + K1 * C1s;
    const double SH = 1.0 + K2 * C1s;
    const double tL = dL / (kL * SL);
    const double tC = dCab / (kC * SC);
    const double t_inside = tL * tL + tC * tC + dHab_sq / ((kH * SH) * (kH * SH));
    return std::sqrt(std::max(0.0, t_inside));
}

// CIEDE2000 difference between two Lab triplets (Sharma-Wu-Dalal 2005).
double delta_e_2000(const Triplet &p1, const Triplet &p2,
                    double kL, double kC, double kH, double K1, double K2)
{
    const double L1 = p1.L, a1_in = p1.a, b1 = p1.b;
    const double L2 = p2.L, a2_in = p2.a, b2 = p2.b;
    const double C1 = std::sqrt(a1_in * a1_in + b1 * b1);
    const double C2 = std::sqrt(a2_in * a2_in + b2 * b2);
    const double Cbar = 0.5 * (C1 + C2);
    const double Cbar7 = std::pow(Cbar, 7.0);
    const double G = 0.5 * (1.0 - std::sqrt(Cbar7 / (Cbar7 + 6103515625.0)));
    const double a1 = (1.0 + G) * a1_in;
    const double a2 = (1.0 + G) * a2_in;
    const double C1d = std::sqrt(a1 * a1 + b1 * b1);
    const double C2d = std::sqrt(a2 * a2 + b2 * b2);

    auto wrap_hue = [](double a, double b) {
        if (a == 0.0 && b == 0.0) return 0.0;
        double h = std::atan2(b, a);
        if (h < 0.0) h += 2.0 * M_PI;
        return h;
    };
    const double h1 = wrap_hue(a1, b1);
    const double h2 = wrap_hue(a2, b2);

    const double dL = L2 - L1;
    const double dC = C2d - C1d;
    double dh = 0.0;
    if (C1d * C2d != 0.0) {
        const double hsub = h2 - h1;
        if      (hsub >  M_PI) dh = hsub - 2.0 * M_PI;
        else if (hsub < -M_PI) dh = hsub + 2.0 * M_PI;
        else                   dh = hsub;
    }
    const double dH = 2.0 * std::sqrt(C1d * C2d) * std::sin(dh / 2.0);

    const double Lbar = 0.5 * (L1 + L2);
    const double Cdbar = 0.5 * (C1d + C2d);
    double hbar = 0.0;
    if (C1d * C2d == 0.0) {
        hbar = h1 + h2;     // MATLAB: hadd / 1 (no /2)
    } else {
        const double hsub = h2 - h1;
        const double hadd = h1 + h2;
        if (std::fabs(hsub) <= M_PI) hbar = hadd / 2.0;
        else if (hadd < 2.0 * M_PI)  hbar = (hadd + 2.0 * M_PI) / 2.0;
        else                          hbar = (hadd - 2.0 * M_PI) / 2.0;
    }
    const double T = 1.0
                   - 0.17 * std::cos(hbar - 30.0 * DEG2RAD)
                   + 0.24 * std::cos(2.0 * hbar)
                   + 0.32 * std::cos(3.0 * hbar +  6.0 * DEG2RAD)
                   - 0.20 * std::cos(4.0 * hbar - 63.0 * DEG2RAD);
    const double dTheta_arg = (hbar - 275.0 * DEG2RAD) / (25.0 * DEG2RAD);
    const double dTheta = 30.0 * DEG2RAD * std::exp(-dTheta_arg * dTheta_arg);
    const double Cdbar7 = std::pow(Cdbar, 7.0);
    const double RC = 2.0 * std::sqrt(Cdbar7 / (Cdbar7 + 6103515625.0));
    const double Lbar_d = Lbar - 50.0;
    const double SL = 1.0 + (K2 * Lbar_d * Lbar_d)
                          / std::sqrt(20.0 + Lbar_d * Lbar_d);
    const double SC = 1.0 + K1 * Cdbar;
    const double SH = 1.0 + K2 * Cdbar * T;
    const double RT = -std::sin(2.0 * dTheta) * RC;

    const double tL = dL / (kL * SL);
    const double tC = dC / (kC * SC);
    const double tH = dH / (kH * SH);
    const double inside = tL * tL + tC * tC + tH * tH + RT * tC * tH;
    return std::sqrt(std::max(0.0, inside));
}

} // anonymous

Value imcolordiff(const Value &I1, const Value &I2,
                  const std::string &standard, bool is_input_lab,
                  double kL, double kC, double kH, double K1, double K2,
                  std::pmr::memory_resource *mr)
{
    // Normalise the Standard string.
    std::string std_lo = standard;
    for (auto &c : std_lo)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const bool is_2000 = (std_lo == "ciede2000" || std_lo == "cie2000");
    if (!is_2000 && std_lo != "cie94")
        throw Error("imcolordiff: Standard must be 'CIE94' or 'CIEDE2000'",
                    0, 0, "imcolordiff", "", "numkit:imcolordiff:standard");
    if (!(kL > 0.0) || !(kC > 0.0) || !(kH > 0.0)
        || !(K1 > 0.0) || !(K2 > 0.0))
        throw Error("imcolordiff: kL, kC, kH, K1, K2 must all be positive",
                    0, 0, "imcolordiff", "", "numkit:imcolordiff:weights");

    const auto &d1 = I1.dims();
    const auto &d2 = I2.dims();

    // Determine shape (matching deltaE's logic). Two recognised forms:
    //   * c-by-3 list  → c-by-1 output (column vector).
    //   * H-by-W-by-3  → H-by-W output.
    enum Shape { COLORMAP, IMAGE } shape;
    std::size_t out_h, out_w;
    if (!d1.is3D() && !d2.is3D()) {
        if (d1.cols() != 3 || d2.cols() != 3 || d1.rows() != d2.rows())
            throw Error("imcolordiff: c-by-3 inputs must have matching rows",
                        0, 0, "imcolordiff", "", "numkit:imcolordiff:size");
        shape = COLORMAP;
        out_h = d1.rows();
        out_w = 1;
    } else if (d1.is3D() && d2.is3D()) {
        if (d1.pages() != 3 || d2.pages() != 3
            || d1.rows() != d2.rows() || d1.cols() != d2.cols())
            throw Error("imcolordiff: H-by-W-by-3 inputs must match in size",
                        0, 0, "imcolordiff", "", "numkit:imcolordiff:size");
        shape = IMAGE;
        out_h = d1.rows();
        out_w = d1.cols();
    } else {
        throw Error("imcolordiff: I1 and I2 must both be c-by-3 or H-by-W-by-3",
                    0, 0, "imcolordiff", "", "numkit:imcolordiff:size");
    }
    (void)shape;

    // Class promotion (matches MATLAB: double if either is double, else
    // single output type — but we always compute in double).
    const bool to_double = (I1.type() == ValueType::DOUBLE)
                        || (I2.type() == ValueType::DOUBLE);
    Value A = to_double ? im2double(I1, mr) : im2single(I1, mr);
    Value B = to_double ? im2double(I2, mr) : im2single(I2, mr);
    if (!is_input_lab) {
        A = rgb2lab(A, mr);
        B = rgb2lab(B, mr);
    }
    // For the c-by-3 shape we still treat the data as a "pseudo image"
    // of width-1 page-stride for indexing — pack into the same shape
    // rgb2lab returned (which preserves c-by-3 layout).
    const std::size_t H = (shape == COLORMAP) ? out_h : out_h;
    const std::size_t W = (shape == COLORMAP) ? 1     : out_w;
    const std::size_t N = H * W;

    Value out = Value::matrix(out_h, out_w,
                              to_double ? ValueType::DOUBLE : ValueType::SINGLE,
                              mr);

    for (std::size_t k = 0; k < N; ++k) {
        Triplet p1 = lab_at(A, H, W, k);
        Triplet p2 = lab_at(B, H, W, k);
        double v;
        if (is_2000) v = delta_e_2000(p1, p2, kL, kC, kH, K1, K2);
        else         v = delta_e_94  (p1, p2, kL, kC, kH, K1, K2);
        if (to_double) out.doubleDataMut()[k] = v;
        else           out.singleDataMut()[k] = static_cast<float>(v);
    }
    return out;
}

} // namespace numkit::image
