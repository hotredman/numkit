// libs/image/src/segment/segment.cpp
//
// Lightweight segmentation utilities — no DL, no fancy numerics.
// Each entry is O(N) on the input. Composes with the cycle-22
// bwlabel / regionprops infrastructure.

#include <numkit/image/segment/segment.hpp>
#include <numkit/image/filter/filter.hpp>
#include <numkit/image/geom/geom.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <queue>
#include <utility>
#include <vector>
#include <vector>

namespace numkit::image {

namespace {

bool nonZero(double v) { return v != 0.0; }

void requireSameShape(const Value &A, const Value &B, const char *fn) {
    if (A.numel() != B.numel() ||
        A.dims().rows() != B.dims().rows() ||
        A.dims().cols() != B.dims().cols())
        throw Error(std::string(fn) + ": inputs must have the same shape",
                    0, 0, fn, "", "m:image:segment:shape");
}

} // anonymous

Value dice(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    requireSameShape(A, B, "dice");
    const size_t N = A.numel();
    long long inter = 0, sumA = 0, sumB = 0;
    for (size_t i = 0; i < N; ++i) {
        const bool a = nonZero(A.elemAsDouble(i));
        const bool b = nonZero(B.elemAsDouble(i));
        if (a) ++sumA;
        if (b) ++sumB;
        if (a && b) ++inter;
    }
    const long long denom = sumA + sumB;
    const double d = (denom == 0) ? 1.0
                                  : 2.0 * double(inter) / double(denom);
    return Value::scalar(d, mr);
}

Value jaccard(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    requireSameShape(A, B, "jaccard");
    const size_t N = A.numel();
    long long inter = 0, uni = 0;
    for (size_t i = 0; i < N; ++i) {
        const bool a = nonZero(A.elemAsDouble(i));
        const bool b = nonZero(B.elemAsDouble(i));
        if (a && b) ++inter;
        if (a || b) ++uni;
    }
    const double j = (uni == 0) ? 1.0
                                : double(inter) / double(uni);
    return Value::scalar(j, mr);
}

Value boundarymask(const Value &L, int conn, std::pmr::memory_resource *mr)
{
    if (conn != 4) conn = 8;
    const int H = static_cast<int>(L.dims().rows());
    const int W = static_cast<int>(L.dims().cols());
    Value out = Value::matrix(static_cast<size_t>(H),
                              static_cast<size_t>(W),
                              ValueType::LOGICAL, mr);
    if (H == 0 || W == 0) return out;

    // Read L as int (round); column-major access via elemAsDouble.
    auto labAt = [&](int r, int c) -> long long {
        return static_cast<long long>(std::round(
            L.elemAsDouble(static_cast<size_t>(c) * static_cast<size_t>(H) +
                            static_cast<size_t>(r))));
    };

    std::uint8_t *od = out.logicalDataMut();
    static const int dr8[8] = { -1,-1,-1, 0, 0, 1, 1, 1 };
    static const int dc8[8] = { -1, 0, 1,-1, 1,-1, 0, 1 };
    static const int dr4[4] = { -1, 1, 0, 0 };
    static const int dc4[4] = {  0, 0,-1, 1 };
    const int *dr = (conn == 4) ? dr4 : dr8;
    const int *dc = (conn == 4) ? dc4 : dc8;
    const int K   = (conn == 4) ? 4    : 8;

    for (int c = 0; c < W; ++c)
        for (int r = 0; r < H; ++r) {
            const long long me = labAt(r, c);
            // Background pixel (label 0) is never a boundary in the
            // MATLAB sense; for a pure binary mask we treat the
            // foreground perimeter.
            bool isBoundary = false;
            for (int k = 0; k < K; ++k) {
                const int nr = r + dr[k];
                const int nc = c + dc[k];
                if (nr < 0 || nr >= H || nc < 0 || nc >= W) {
                    if (me != 0) { isBoundary = true; break; }
                    continue;
                }
                const long long ne = labAt(nr, nc);
                if (ne != me) { isBoundary = true; break; }
            }
            // Background-only neighbourhood: not a boundary.
            if (me == 0) isBoundary = false;
            od[static_cast<size_t>(c) * static_cast<size_t>(H) +
               static_cast<size_t>(r)] = isBoundary ? 1u : 0u;
        }
    return out;
}

Value label2idx(const Value &L, std::pmr::memory_resource *mr)
{
    const size_t N = L.numel();
    long long maxLab = 0;
    for (size_t i = 0; i < N; ++i) {
        const long long v = static_cast<long long>(
            std::round(L.elemAsDouble(i)));
        if (v > maxLab) maxLab = v;
    }
    Value cell = Value::cell(static_cast<size_t>(maxLab), 1, mr);
    if (maxLab == 0) return cell;

    // First pass: count per label.
    std::vector<size_t> counts(static_cast<size_t>(maxLab) + 1, 0);
    for (size_t i = 0; i < N; ++i) {
        const long long v = static_cast<long long>(
            std::round(L.elemAsDouble(i)));
        if (v > 0) ++counts[static_cast<size_t>(v)];
    }
    // Allocate per-label column vectors (1-based indices).
    std::vector<Value> bufs;
    bufs.reserve(static_cast<size_t>(maxLab));
    std::vector<double *> wptr(static_cast<size_t>(maxLab) + 1, nullptr);
    std::vector<size_t> cur(static_cast<size_t>(maxLab) + 1, 0);
    for (long long k = 1; k <= maxLab; ++k) {
        Value v = Value::matrix(counts[static_cast<size_t>(k)], 1,
                                ValueType::DOUBLE, mr);
        wptr[static_cast<size_t>(k)] = (counts[static_cast<size_t>(k)] > 0)
                                       ? v.doubleDataMut() : nullptr;
        bufs.push_back(std::move(v));
    }
    // Second pass: fill.
    for (size_t i = 0; i < N; ++i) {
        const long long v = static_cast<long long>(
            std::round(L.elemAsDouble(i)));
        if (v > 0) {
            wptr[static_cast<size_t>(v)][cur[static_cast<size_t>(v)]] =
                static_cast<double>(i + 1); // MATLAB 1-based
            ++cur[static_cast<size_t>(v)];
        }
    }
    for (long long k = 1; k <= maxLab; ++k)
        cell.cellAt(static_cast<size_t>(k - 1)) =
            std::move(bufs[static_cast<size_t>(k - 1)]);
    return cell;
}

Value grayconnected(const Value &I, int row, int col, double tol, std::pmr::memory_resource *mr)
{
    const int H = static_cast<int>(I.dims().rows());
    const int W = static_cast<int>(I.dims().cols());
    Value out = Value::matrix(static_cast<size_t>(H),
                              static_cast<size_t>(W),
                              ValueType::LOGICAL, mr);
    if (H == 0 || W == 0) return out;

    // 1-based MATLAB → 0-based.
    const int r0 = row - 1;
    const int c0 = col - 1;
    if (r0 < 0 || r0 >= H || c0 < 0 || c0 >= W)
        throw Error("grayconnected: seed out of bounds",
                    0, 0, "grayconnected", "", "m:grayconnected:seed");

    // Auto-pick tolerance if caller passed a negative sentinel.
    if (tol < 0.0) {
        switch (I.type()) {
            case ValueType::UINT8:  tol = 32.0;            break;
            case ValueType::INT8:   tol = 32.0;            break;
            case ValueType::UINT16: tol = 32.0 * 256.0;    break;
            case ValueType::INT16:  tol = 32.0 * 256.0;    break;
            default:                tol = 0.125;            break; // [0, 1] floats
        }
    }

    auto idxAt = [&](int r, int c) {
        return static_cast<size_t>(c) * static_cast<size_t>(H) +
               static_cast<size_t>(r);
    };
    const double seedVal = I.elemAsDouble(idxAt(r0, c0));
    std::uint8_t *od = out.logicalDataMut();
    std::vector<std::uint8_t> visited(static_cast<size_t>(H) *
                                       static_cast<size_t>(W), 0);

    // BFS with an explicit deque (vector + read head).
    std::vector<std::pair<int, int>> q;
    q.reserve(static_cast<size_t>(H) * static_cast<size_t>(W) / 4 + 4);
    q.emplace_back(r0, c0);
    visited[static_cast<size_t>(r0) * static_cast<size_t>(W) +
            static_cast<size_t>(c0)] = 1;
    od[idxAt(r0, c0)] = 1;
    static const int dr8[8] = { -1,-1,-1, 0, 0, 1, 1, 1 };
    static const int dc8[8] = { -1, 0, 1,-1, 1,-1, 0, 1 };

    size_t head = 0;
    while (head < q.size()) {
        const auto [r, c] = q[head++];
        for (int k = 0; k < 8; ++k) {
            const int nr = r + dr8[k];
            const int nc = c + dc8[k];
            if (nr < 0 || nr >= H || nc < 0 || nc >= W) continue;
            const size_t fi = static_cast<size_t>(nr) *
                                static_cast<size_t>(W) +
                              static_cast<size_t>(nc);
            if (visited[fi]) continue;
            const double v = I.elemAsDouble(idxAt(nr, nc));
            if (std::abs(v - seedVal) > tol) continue;
            visited[fi] = 1;
            od[idxAt(nr, nc)] = 1;
            q.emplace_back(nr, nc);
        }
    }
    return out;
}

// ── graydiffweight (FMM intensity-difference weights) ──────────────
//
// MATLAB R2025b graydiffweight.m → call sequence:
//   d = |I − refGrayVal|
//   if cutoff < Inf: isSuppressed = (d > cutoff)
//   d_scaled = imlinscale(d, [1e-3, 1])    — linear (min, max)→(1e-3, 1)
//   if cutoff: d_scaled(isSuppressed) = 1
//   W = 1 ./ (d_scaled .^ (1 / rolloffFactor))
// Output class is single if input was single, otherwise double.
// (The 4 input signatures collapse to a single scalar `refGrayVal`
// here; the adapter computes the mean over MASK / (C, R) / (C, R, P)
// before dispatching to this typed entry-point.)
Value graydiffweight(const Value &I, double ref_gray_val,
                     double rolloff_factor, double cutoff,
                     std::pmr::memory_resource *mr)
{
    if (!(rolloff_factor > 0.0))
        throw Error("graydiffweight: RolloffFactor must be positive",
                    0, 0, "graydiffweight", "", "m:graydiffweight:rolloff");
    if (!(cutoff >= 0.0))
        throw Error("graydiffweight: GrayDifferenceCutoff must be "
                    "non-negative",
                    0, 0, "graydiffweight", "", "m:graydiffweight:cutoff");

    const ValueType outT = (I.type() == ValueType::SINGLE)
                          ? ValueType::SINGLE : ValueType::DOUBLE;
    const auto &d = I.dims();
    const std::size_t N = I.numel();
    Value W = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), outT, mr)
        : Value::matrix(d.rows(), d.cols(), outT, mr);
    if (N == 0) return W;

    // Pass 1 — compute |I − ref| in DOUBLE temporary; track min/max.
    std::pmr::vector<double> diff(N, mr);
    double dmin = std::numeric_limits<double>::infinity();
    double dmax = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < N; ++i) {
        const double v = std::fabs(I.elemAsDouble(i) - ref_gray_val);
        diff[i] = v;
        if (v < dmin) dmin = v;
        if (v > dmax) dmax = v;
    }

    // Linear scale to [1e-3, 1].
    const double lo = 1e-3, hi = 1.0;
    const double range = dmax - dmin;
    const double slope = (range > 0.0) ? (hi - lo) / range : 0.0;

    const double inv_rolloff = 1.0 / rolloff_factor;
    const bool finite_cutoff = std::isfinite(cutoff);

    auto store = [&](std::size_t i, double v) {
        if (outT == ValueType::DOUBLE) W.doubleDataMut()[i] = v;
        else                            W.singleDataMut()[i] = static_cast<float>(v);
    };

    for (std::size_t i = 0; i < N; ++i) {
        double s;
        if (range > 0.0) s = lo + slope * (diff[i] - dmin);
        else             s = lo;     // constant-image edge case
        if (finite_cutoff && diff[i] > cutoff) s = hi;
        const double w = 1.0 / std::pow(s, inv_rolloff);
        store(i, w);
    }
    return W;
}

// ── gradientweight (FMM gradient-based pixel weights) ──────────────
//
// MATLAB R2025b gradientweight.m / images.internal.imgradientdog
// algorithm:
//   r        = ceil(2*sigma)             (per axis)
//   x        = -r:r
//   hx(x)    = -x * exp(-x^2 / (2*sigma_x^2))
//   norm     = sum(hx(1..r))             (positive half — MATLAB 1-idx)
//   hx       = hx / norm
//   hy       = same with sigma_y, oriented vertical (column)
//   Gx       = imfilter(I, hx, 'replicate')
//   Gy       = imfilter(I, hy, 'replicate')
//   W = hypot(Gx, Gy)
//   W = imlinscale(W, [0 1])
//   W = W ^ (1 / rolloff_factor)
//   W = (1 - W) / (1 + W)
//   W(W < cutoff) = 1e-3
// Output class: single if input single, else double.
// 2-D only here (MATLAB calls imgradientdog3 for 3-D volumes).
Value gradientweight(const Value &I, double sigma_x, double sigma_y,
                     double rolloff_factor, double weight_cutoff,
                     std::pmr::memory_resource *mr)
{
    if (!(sigma_x > 0.0) || !std::isfinite(sigma_x)
     || !(sigma_y > 0.0) || !std::isfinite(sigma_y))
        throw Error("gradientweight: sigma must be positive and finite",
                    0, 0, "gradientweight", "", "m:gradientweight:sigma");
    if (!(rolloff_factor > 0.0) || !std::isfinite(rolloff_factor))
        throw Error("gradientweight: RolloffFactor must be positive "
                    "and finite",
                    0, 0, "gradientweight", "", "m:gradientweight:rolloff");
    if (!(weight_cutoff >= 1e-3 && weight_cutoff <= 1.0))
        throw Error("gradientweight: WeightCutoff must be in [1e-3, 1]",
                    0, 0, "gradientweight", "", "m:gradientweight:cutoff");
    if (I.dims().is3D())
        throw Error("gradientweight: 3-D inputs not supported "
                    "(slice and call per page)",
                    0, 0, "gradientweight", "", "m:gradientweight:dim");

    const ValueType outT = (I.type() == ValueType::SINGLE)
                          ? ValueType::SINGLE : ValueType::DOUBLE;
    const std::size_t H = I.dims().rows();
    const std::size_t W_ = I.dims().cols();
    const std::size_t N = H * W_;

    // Empty: return same-shape DOUBLE empty.
    if (N == 0) return Value::matrix(H, W_, outT, mr);

    // Constant-image fast-path: |max - min| <= eps(maxabs)*1000 → W = 1.
    double minI = std::numeric_limits<double>::infinity();
    double maxI = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < N; ++i) {
        const double v = I.elemAsDouble(i);
        if (v < minI) minI = v;
        if (v > maxI) maxI = v;
    }
    const double absMax = std::max(std::fabs(minI), std::fabs(maxI));
    const double e = std::nextafter(absMax,
                          std::numeric_limits<double>::infinity()) - absMax;
    if (maxI - e * 1000.0 <= minI) {
        Value Wones = Value::matrix(H, W_, outT, mr);
        if (outT == ValueType::DOUBLE) {
            double *p = Wones.doubleDataMut();
            for (std::size_t i = 0; i < N; ++i) p[i] = 1.0;
        } else {
            float *p = Wones.singleDataMut();
            for (std::size_t i = 0; i < N; ++i) p[i] = 1.0f;
        }
        return Wones;
    }

    // Build DoG kernel along a single axis with given sigma. `norm_count`
    // is the number of left-half elements summed for normalisation
    // (MATLAB uses `filtRadius(1)` for BOTH hx and hy — so for hy on
    // anisotropic σ, this is the x-axis radius, not the y-axis radius.
    // We replicate the MATLAB R2025b behaviour exactly).
    auto build_dog = [&](double sigma, int norm_count, std::size_t &len) {
        const int r = static_cast<int>(std::ceil(2.0 * sigma));
        len = 2 * static_cast<std::size_t>(r) + 1;
        std::pmr::vector<double> h(len, 0.0, mr);
        double norm = 0.0;
        for (int k = 0; k < static_cast<int>(len); ++k) {
            const int x = k - r;
            h[k] = -static_cast<double>(x)
                 * std::exp(-static_cast<double>(x * x)
                            / (2.0 * sigma * sigma));
            // MATLAB: norm = sum(h(1..norm_count)) -- 1-indexed
            // (k = 0..norm_count-1 in 0-indexed, the left half).
            if (k < norm_count) norm += h[k];
        }
        if (norm == 0.0) norm = 1.0;
        for (std::size_t k = 0; k < len; ++k) h[k] /= norm;
        return h;
    };

    const int rx = static_cast<int>(std::ceil(2.0 * sigma_x));
    std::size_t hxLen = 0, hyLen = 0;
    auto hxBuf = build_dog(sigma_x, rx, hxLen);
    auto hyBuf = build_dog(sigma_y, rx, hyLen);     // MATLAB-bug-compatible

    // Cast I to DOUBLE for filtering (matches MATLAB internal).
    Value Id;
    if (I.type() == ValueType::DOUBLE) {
        Id = I;
    } else {
        Id = Value::matrix(H, W_, ValueType::DOUBLE, mr);
        double *p = Id.doubleDataMut();
        for (std::size_t i = 0; i < N; ++i) p[i] = I.elemAsDouble(i);
    }

    // hx as 1×Nx row vector; hy as Ny×1 column vector.
    Value hx = Value::matrix(1, hxLen, ValueType::DOUBLE, mr);
    for (std::size_t k = 0; k < hxLen; ++k)
        hx.doubleDataMut()[k] = hxBuf[k];
    Value hy = Value::matrix(hyLen, 1, ValueType::DOUBLE, mr);
    for (std::size_t k = 0; k < hyLen; ++k)
        hy.doubleDataMut()[k] = hyBuf[k];

    // Gx, Gy via imfilter (replicate boundary, same size, correlation).
    Value Gx = imfilter(Id, hx, PadMode::Replicate, 0.0,
                        /*full=*/false, /*flip_kernel=*/false, mr);
    Value Gy = imfilter(Id, hy, PadMode::Replicate, 0.0,
                        /*full=*/false, /*flip_kernel=*/false, mr);

    // Gmag = hypot(Gx, Gy); track min/max for imlinscale.
    std::pmr::vector<double> Gmag(N, 0.0, mr);
    double gmin = std::numeric_limits<double>::infinity();
    double gmax = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < N; ++i) {
        const double gx = Gx.elemAsDouble(i);
        const double gy = Gy.elemAsDouble(i);
        const double g = std::hypot(gx, gy);
        Gmag[i] = g;
        if (g < gmin) gmin = g;
        if (g > gmax) gmax = g;
    }
    // imlinscale to [0, 1].
    const double gabsMin = std::min(std::fabs(gmin), std::fabs(gmax));
    const double e2 = std::nextafter(gabsMin,
                          std::numeric_limits<double>::infinity()) - gabsMin;
    const bool spread_ok = (gmax - gmin) > e2;
    const double slope = spread_ok ? 1.0 / (gmax - gmin) : 0.0;

    const double inv_rolloff = 1.0 / rolloff_factor;
    const double floorOfW    = 1e-3;

    Value Wout = Value::matrix(H, W_, outT, mr);
    auto store = [&](std::size_t i, double v) {
        if (outT == ValueType::DOUBLE) Wout.doubleDataMut()[i] = v;
        else                            Wout.singleDataMut()[i] = static_cast<float>(v);
    };
    for (std::size_t i = 0; i < N; ++i) {
        double w = spread_ok ? slope * (Gmag[i] - gmin) : 0.0;
        // W = W^(1/rolloff); W = (1-W)/(1+W); cutoff
        w = std::pow(w, inv_rolloff);
        w = (1.0 - w) / (1.0 + w);
        if (w < weight_cutoff) w = floorOfW;
        store(i, w);
    }
    return Wout;
}

// ── regionfill (Laplacian inpainting) ──────────────────────────────
//
// MATLAB R2025b regionfill.m algorithm:
//
//   1. maskPerimeter = (dilate(mask, cross) & !mask)
//        — the 1-pixel ring of known boundary values around each hole.
//   2. For each interior masked pixel (r, c):
//        4 J(r,c) − J_N − J_S − J_W − J_E = sum of perimeter neighbours
//      Pixels on the image border get a 3-neighbour stencil; corners
//      get 2.
//   3. Stack the equations into a sparse SPD linear system Ax = b
//      indexed by mask pixel and solve.
//
// MATLAB uses sparse-direct (UMFPACK via `\`); we use unpreconditioned
// conjugate gradient at tol 1e-12 against the implicit stencil matrix
// (no need to materialise A — apply Laplacian directly via the mask).
// CG converges in O(√n) iterations for the 2-D Laplacian, so a typical
// 1000-unknown mask takes ~30 iterations.
//
// References: Gonzalez & Woods §3.4 (Laplacian);
//             Press et al., *Numerical Recipes* §2.7.
Value regionfill(const Value &I, const Value &mask,
                 std::pmr::memory_resource *mr)
{
    if (I.dims().is3D())
        throw Error("regionfill: I must be 2-D",
                    0, 0, "regionfill", "", "m:regionfill:mustBe2D");
    const std::size_t H = I.dims().rows();
    const std::size_t W = I.dims().cols();
    if (H < 3 || W < 3)
        throw Error("regionfill: I must be at least 3x3",
                    0, 0, "regionfill", "",
                    "m:regionfill:mustBeLargerThan2by2");
    if (mask.dims().rows() != H || mask.dims().cols() != W
     || mask.dims().is3D())
        throw Error("regionfill: MASK must be 2-D and the same size as I",
                    0, 0, "regionfill", "",
                    "m:regionfill:mustBeSameSizeAsI");
    const std::size_t N = H * W;

    // Convert mask to plain uint8 array (0/1) in column-major (matches
    // I's storage).
    std::pmr::vector<std::uint8_t> M(N, 0, mr);
    if (mask.isLogical()) {
        const std::uint8_t *src = mask.logicalData();
        for (std::size_t i = 0; i < N; ++i) M[i] = src[i] ? 1 : 0;
    } else {
        for (std::size_t i = 0; i < N; ++i) {
            const double v = mask.elemAsDouble(i);
            if (std::isnan(v))
                throw Error("regionfill: MASK cannot contain NaN",
                            0, 0, "regionfill", "",
                            "m:regionfill:maskNaN");
            M[i] = (v != 0.0) ? 1 : 0;
        }
    }

    // Cast I to DOUBLE for the solve; remember original class for output.
    const ValueType inT = I.type();
    std::pmr::vector<double> Idata(N, 0.0, mr);
    for (std::size_t i = 0; i < N; ++i) Idata[i] = I.elemAsDouble(i);

    // Helper: column-major (r, c) → linear index.
    auto lin = [&](std::size_t r, std::size_t c) { return c * H + r; };

    // Compute mask perimeter: pixels (r,c) where M[r,c]==0 AND at least
    // one of the 4 N/S/W/E neighbours is masked.
    std::pmr::vector<std::uint8_t> P(N, 0, mr);
    for (std::size_t c = 0; c < W; ++c) {
        for (std::size_t r = 0; r < H; ++r) {
            const std::size_t k = lin(r, c);
            if (M[k]) continue;
            bool touches = false;
            if (r > 0      && M[lin(r - 1, c)]) touches = true;
            if (r + 1 < H  && M[lin(r + 1, c)]) touches = true;
            if (c > 0      && M[lin(r, c - 1)]) touches = true;
            if (c + 1 < W  && M[lin(r, c + 1)]) touches = true;
            P[k] = touches ? 1 : 0;
        }
    }

    // Build mask-pixel-to-unknown index map.
    std::pmr::vector<std::int32_t> idxOf(N, -1, mr);
    std::pmr::vector<std::size_t>  pixOf(mr);
    pixOf.reserve(N / 4);
    for (std::size_t i = 0; i < N; ++i) {
        if (M[i]) {
            idxOf[i] = static_cast<std::int32_t>(pixOf.size());
            pixOf.push_back(i);
        }
    }
    const std::size_t nU = pixOf.size();

    // Output: start as copy of I (as DOUBLE so we can write the solve).
    std::pmr::vector<double> Jout(Idata.begin(), Idata.end(), mr);

    if (nU == 0) {
        // No mask pixels → J = I unchanged.
        Value J = Value::matrix(H, W, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < N; ++i) J.doubleDataMut()[i] = Jout[i];
        if (inT == ValueType::DOUBLE) return J;
        Value Jc = Value::matrix(H, W, inT, mr);
        for (std::size_t i = 0; i < N; ++i) {
            const double v = Jout[i];
            switch (inT) {
                case ValueType::SINGLE: Jc.singleDataMut()[i] = static_cast<float>(v); break;
                case ValueType::UINT8:  Jc.uint8DataMut()[i]  = static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0)); break;
                case ValueType::UINT16: Jc.uint16DataMut()[i] = static_cast<std::uint16_t>(std::clamp(v, 0.0, 65535.0)); break;
                case ValueType::INT8:   Jc.int8DataMut()[i]   = static_cast<std::int8_t>(std::clamp(v, -128.0, 127.0)); break;
                case ValueType::INT16:  Jc.int16DataMut()[i]  = static_cast<std::int16_t>(std::clamp(v, -32768.0, 32767.0)); break;
                case ValueType::INT32:  Jc.int32DataMut()[i]  = static_cast<std::int32_t>(v); break;
                default: Jc.doubleDataMut()[i] = v; break;
            }
        }
        return Jc;
    }

    // Pre-compute per-unknown stencil data:
    //   numNbrs[i]    = # of on-grid neighbours (2/3/4)
    //   rhs[i]        = sum of perimeter-neighbour intensities
    //   nbrs[i]       = up to 4 unknown indices of N/S/W/E (or -1 if
    //                   off-grid or non-mask).
    std::pmr::vector<std::int32_t> numNbrs(nU, 0, mr);
    std::pmr::vector<double>        rhs(nU, 0.0, mr);
    // Flat array of 4 neighbours per unknown.
    std::pmr::vector<std::int32_t>  nbrs(nU * 4, -1, mr);

    auto add_dir = [&](std::size_t i, std::size_t nr, std::size_t nc,
                       bool valid, std::size_t slot) {
        if (!valid) return;
        const std::size_t nk = lin(nr, nc);
        ++numNbrs[i];
        if (M[nk]) {
            nbrs[i * 4 + slot] = idxOf[nk];
        } else if (P[nk]) {
            rhs[i] += Idata[nk];
        }
        // else: in-grid, non-mask, non-perimeter — known but contributes 0.
    };

    for (std::size_t i = 0; i < nU; ++i) {
        const std::size_t k = pixOf[i];
        const std::size_t c = k / H;
        const std::size_t r = k % H;
        add_dir(i, r - 1, c, r > 0,      0); // N
        add_dir(i, r + 1, c, r + 1 < H,  1); // S
        add_dir(i, r, c - 1, c > 0,      2); // W
        add_dir(i, r, c + 1, c + 1 < W,  3); // E
    }

    // Apply A = stencil matrix (no materialisation):
    //   y[i] = numNbrs[i] * x[i] - sum_{j in nbrs[i]} x[j]
    auto matvec = [&](const std::pmr::vector<double> &x,
                      std::pmr::vector<double> &y) {
        for (std::size_t i = 0; i < nU; ++i) {
            double s = static_cast<double>(numNbrs[i]) * x[i];
            const std::int32_t *nb = &nbrs[i * 4];
            if (nb[0] >= 0) s -= x[nb[0]];
            if (nb[1] >= 0) s -= x[nb[1]];
            if (nb[2] >= 0) s -= x[nb[2]];
            if (nb[3] >= 0) s -= x[nb[3]];
            y[i] = s;
        }
    };

    // Conjugate gradient solver, tol = 1e-12 (relative).
    std::pmr::vector<double> x(nU, 0.0, mr);
    std::pmr::vector<double> r(nU, 0.0, mr);
    std::pmr::vector<double> p(nU, 0.0, mr);
    std::pmr::vector<double> Ap(nU, 0.0, mr);

    for (std::size_t i = 0; i < nU; ++i) { r[i] = rhs[i]; p[i] = rhs[i]; }
    double rdotr = 0.0;
    double bnorm = 0.0;
    for (std::size_t i = 0; i < nU; ++i) {
        rdotr += r[i] * r[i];
        bnorm += rhs[i] * rhs[i];
    }
    const double bn = std::sqrt(bnorm);
    const double tol = (bn > 0.0)
                         ? (1e-12 * bn) * (1e-12 * bn)
                         : 1e-24;

    const std::size_t maxiter = nU * 4 + 200;  // safety cap
    for (std::size_t it = 0; it < maxiter; ++it) {
        if (rdotr <= tol) break;
        matvec(p, Ap);
        double pAp = 0.0;
        for (std::size_t i = 0; i < nU; ++i) pAp += p[i] * Ap[i];
        if (pAp <= 0.0) break;
        const double alpha = rdotr / pAp;
        for (std::size_t i = 0; i < nU; ++i) {
            x[i] += alpha * p[i];
            r[i] -= alpha * Ap[i];
        }
        double rdotr_new = 0.0;
        for (std::size_t i = 0; i < nU; ++i) rdotr_new += r[i] * r[i];
        const double beta = rdotr_new / rdotr;
        for (std::size_t i = 0; i < nU; ++i)
            p[i] = r[i] + beta * p[i];
        rdotr = rdotr_new;
    }

    // Write solution back to Jout.
    for (std::size_t i = 0; i < nU; ++i) Jout[pixOf[i]] = x[i];

    // Build typed output.
    Value J = Value::matrix(H, W, inT, mr);
    if (inT == ValueType::DOUBLE) {
        double *p_ = J.doubleDataMut();
        for (std::size_t i = 0; i < N; ++i) p_[i] = Jout[i];
    } else if (inT == ValueType::SINGLE) {
        float *p_ = J.singleDataMut();
        for (std::size_t i = 0; i < N; ++i) p_[i] = static_cast<float>(Jout[i]);
    } else {
        // Integer classes: round + saturate.
        auto saturate = [&](double v, double lo, double hi) {
            v = std::round(v);
            if (v < lo) v = lo;
            if (v > hi) v = hi;
            return v;
        };
        for (std::size_t i = 0; i < N; ++i) {
            const double v = Jout[i];
            switch (inT) {
                case ValueType::UINT8:  J.uint8DataMut()[i]  = static_cast<std::uint8_t>(saturate(v, 0.0, 255.0)); break;
                case ValueType::UINT16: J.uint16DataMut()[i] = static_cast<std::uint16_t>(saturate(v, 0.0, 65535.0)); break;
                case ValueType::UINT32: J.uint32DataMut()[i] = static_cast<std::uint32_t>(saturate(v, 0.0, 4294967295.0)); break;
                case ValueType::INT8:   J.int8DataMut()[i]   = static_cast<std::int8_t>(saturate(v, -128.0, 127.0)); break;
                case ValueType::INT16:  J.int16DataMut()[i]  = static_cast<std::int16_t>(saturate(v, -32768.0, 32767.0)); break;
                case ValueType::INT32:  J.int32DataMut()[i]  = static_cast<std::int32_t>(saturate(v, -2147483648.0, 2147483647.0)); break;
                default: J.doubleDataMut()[i] = v; break;
            }
        }
    }
    return J;
}

// ── poly2mask (polygon → binary mask scan conversion) ─────────────
//
// Empirically reverse-engineered from MATLAB R2025b's closed-source
// images.internal.builtins.poly2mask:
//
//   Pixel (r, c) ∈ BW iff its centre (c, r) is "inside" the polygon
//   under the following even-odd ray-casting variant:
//
//     For each non-horizontal edge (x1, y1) → (x2, y2):
//       if centre_y ∈ (min(y1,y2), max(y1,y2)]   ← half-open below,
//                                                   closed above
//          xi = x1 + (centre_y - y1) * (x2 - x1) / (y2 - y1)
//          if centre_x > xi                      ← strict greater
//             count ^= 1
//
//   The (ylo, yhi] interval ensures shared-vertex edges contribute
//   exactly once. The strict-greater x-test combined with the
//   half-open y rule reproduces MATLAB's bit-equal coverage of:
//     • integer-vertex rectangles aligned with the pixel grid,
//     • diagonal edges crossing pixel centres,
//     • degenerate (zero-area) sub-pixel polygons,
//     • polygons touching the image border.
//
// References: classic scanline polygon fill
//   Foley/van Dam et al., *Computer Graphics: P&P* §3.5;
//   X11 XFillPolygon half-open horizontal-edge convention.
Value poly2mask(const Value &X, const Value &Y,
                std::size_t M, std::size_t N,
                std::pmr::memory_resource *mr)
{
    const std::size_t nx = X.numel();
    const std::size_t ny = Y.numel();
    if (nx != ny)
        throw Error("poly2mask: X and Y must have the same length",
                    0, 0, "poly2mask", "",
                    "m:poly2mask:vectorSizeMismatch");

    Value BW = Value::matrix(M, N, ValueType::LOGICAL, mr);
    if (nx == 0 || M == 0 || N == 0) return BW;  // empty → all-false

    // Convert vertices to DOUBLE; auto-close if needed.
    std::pmr::vector<double> vx(mr), vy(mr);
    vx.reserve(nx + 1);
    vy.reserve(nx + 1);
    for (std::size_t i = 0; i < nx; ++i) {
        vx.push_back(X.elemAsDouble(i));
        vy.push_back(Y.elemAsDouble(i));
    }
    if (vx.front() != vx.back() || vy.front() != vy.back()) {
        vx.push_back(vx.front());
        vy.push_back(vy.front());
    }
    const std::size_t nv = vx.size();

    // For each pixel, run the half-open even-odd ray-cast.
    std::uint8_t *bw = BW.logicalDataMut();
    for (std::size_t c = 0; c < N; ++c) {
        const double cx = static_cast<double>(c + 1);   // 1-based centre x
        for (std::size_t r = 0; r < M; ++r) {
            const double cy = static_cast<double>(r + 1);
            int crossings = 0;
            for (std::size_t i = 0; i + 1 < nv; ++i) {
                const double y1 = vy[i], y2 = vy[i + 1];
                if (y1 == y2) continue;             // horizontal, skip
                const double ylo = std::min(y1, y2);
                const double yhi = std::max(y1, y2);
                if (cy <= ylo || cy > yhi) continue;
                const double x1 = vx[i], x2 = vx[i + 1];
                const double xi = x1 + (cy - y1) * (x2 - x1) / (y2 - y1);
                if (cx > xi) crossings ^= 1;
            }
            // Column-major linear index: c * M + r.
            bw[c * M + r] = static_cast<std::uint8_t>(crossings);
        }
    }
    return BW;
}

// ── roipoly (programmatic polygon ROI mask) ───────────────────────
//
// MATLAB R2025b roipoly.m algorithm (non-interactive forms):
//   1. Auto-close polygon: append (xi(1), yi(1)) if not already closed.
//   2. Map world → pixel:
//        roix = axes2pix(N, [xdata_lo xdata_hi], xi)
//        roiy = axes2pix(M, [ydata_lo ydata_hi], yi)
//   3. BW = poly2mask(roix, roiy, M, N).
//
// The interactive (figure-based) syntaxes are out of scope — they
// require a mouse and have no headless semantics.
Value roipoly(double xdata_lo, double xdata_hi,
              double ydata_lo, double ydata_hi,
              std::size_t M, std::size_t N,
              const Value &xi, const Value &yi,
              std::pmr::memory_resource *mr)
{
    if (xi.numel() != yi.numel())
        throw Error("roipoly: xi and yi must have the same length",
                    0, 0, "roipoly", "", "m:roipoly:xiyiMustBeSameLength");

    // Auto-close polygon.
    const std::size_t n0 = xi.numel();
    std::pmr::vector<double> xc(mr), yc(mr);
    xc.reserve(n0 + 1);
    yc.reserve(n0 + 1);
    for (std::size_t i = 0; i < n0; ++i) {
        xc.push_back(xi.elemAsDouble(i));
        yc.push_back(yi.elemAsDouble(i));
    }
    if (n0 > 0 && (xc.front() != xc.back() || yc.front() != yc.back())) {
        xc.push_back(xc.front());
        yc.push_back(yc.front());
    }

    // Build extent vectors for axes2pix.
    Value xExt = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    xExt.doubleDataMut()[0] = xdata_lo;
    xExt.doubleDataMut()[1] = xdata_hi;
    Value yExt = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    yExt.doubleDataMut()[0] = ydata_lo;
    yExt.doubleDataMut()[1] = ydata_hi;

    // Wrap closed vertices as Value rows for axes2pix.
    const std::size_t nc = xc.size();
    Value xClosed = Value::matrix(nc, 1, ValueType::DOUBLE, mr);
    Value yClosed = Value::matrix(nc, 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < nc; ++i) {
        xClosed.doubleDataMut()[i] = xc[i];
        yClosed.doubleDataMut()[i] = yc[i];
    }
    Value roix = axes2pix(static_cast<double>(N), xExt, xClosed, mr);
    Value roiy = axes2pix(static_cast<double>(M), yExt, yClosed, mr);
    return poly2mask(roix, roiy, M, N, mr);
}

// ── graydist (gray-weighted geodesic distance transform) ────────
//
// MATLAB R2025b graydist algorithm:
//   T(seed) = 0
//   Edge cost p→q: χ(p,q) · (I(p) + I(q)) / 2
//     χ depends on method:
//       chessboard:        8-conn, χ = 1 for all neighbours
//       cityblock:         4-conn, χ = 1 (orth only)
//       quasi-euclidean:   8-conn, χ = 1 (orth), χ = √2 (diag)
//   T(p) = min over neighbours q: T(q) + cost(q→p)
//   Solved by Dijkstra with binary heap.
//
// Reference: Soille, *Morphological Image Analysis*, 2nd ed.,
//   Springer, §4.4 (chamfer geodesic distance transform).
Value graydist(const Value &I, const Value &seeds,
               const std::string &method,
               std::pmr::memory_resource *mr)
{
    if (I.dims().is3D())
        throw Error("graydist: I must be 2-D",
                    0, 0, "graydist", "", "m:graydist:dim");
    const bool is_cb = (method == "cityblock");
    const bool is_chess = (method == "chessboard");
    const bool is_qe = (method == "quasi-euclidean");
    if (!is_cb && !is_chess && !is_qe)
        throw Error("graydist: METHOD must be 'cityblock', "
                    "'chessboard', or 'quasi-euclidean'",
                    0, 0, "graydist", "", "m:graydist:method");

    const std::size_t H = I.dims().rows();
    const std::size_t W = I.dims().cols();
    const std::size_t N = H * W;

    // Output class: DOUBLE for DOUBLE input, SINGLE otherwise.
    const ValueType outT = (I.type() == ValueType::DOUBLE)
                            ? ValueType::DOUBLE : ValueType::SINGLE;
    Value T = Value::matrix(H, W, outT, mr);

    // Initialise with +Inf.
    if (outT == ValueType::DOUBLE) {
        double *p = T.doubleDataMut();
        for (std::size_t i = 0; i < N; ++i)
            p[i] = std::numeric_limits<double>::infinity();
    } else {
        float *p = T.singleDataMut();
        for (std::size_t i = 0; i < N; ++i)
            p[i] = std::numeric_limits<float>::infinity();
    }
    if (N == 0 || seeds.numel() == 0) return T;

    // Working DOUBLE distance buffer (precision; cast back at end).
    std::pmr::vector<double> dist(N, std::numeric_limits<double>::infinity(), mr);
    // Read image into a double array (uniform access).
    std::pmr::vector<double> Idata(N, 0.0, mr);
    for (std::size_t i = 0; i < N; ++i) Idata[i] = I.elemAsDouble(i);

    // Min-heap as (dist, index) pairs. Use std::priority_queue.
    using PQNode = std::pair<double, std::size_t>;
    auto cmp = [](const PQNode &a, const PQNode &b) { return a.first > b.first; };
    std::priority_queue<PQNode, std::vector<PQNode>, decltype(cmp)> pq(cmp);

    // Seed pixels: T = 0, push.
    for (std::size_t k = 0; k < seeds.numel(); ++k) {
        const double sv = seeds.elemAsDouble(k);
        if (!(sv >= 1) || sv != std::floor(sv))
            throw Error("graydist: seed indices must be positive integers",
                        0, 0, "graydist", "", "m:graydist:seed");
        const std::size_t idx = static_cast<std::size_t>(sv) - 1;
        if (idx >= N)
            throw Error("graydist: seed index out of bounds",
                        0, 0, "graydist", "", "m:graydist:seedOOB");
        dist[idx] = 0.0;
        pq.emplace(0.0, idx);
    }

    // Neighbour offsets in (dr, dc) and chamfer weight.
    struct Step { int dr, dc; double chamfer; };
    std::array<Step, 8> steps8{};
    int nSteps = 0;
    if (is_cb) {
        steps8 = {Step{-1, 0, 1.0}, Step{1, 0, 1.0},
                  Step{0, -1, 1.0}, Step{0, 1, 1.0},
                  Step{}, Step{}, Step{}, Step{}};
        nSteps = 4;
    } else if (is_chess) {
        steps8 = {Step{-1, 0, 1.0}, Step{1, 0, 1.0},
                  Step{0, -1, 1.0}, Step{0, 1, 1.0},
                  Step{-1, -1, 1.0}, Step{-1, 1, 1.0},
                  Step{1, -1, 1.0},  Step{1, 1, 1.0}};
        nSteps = 8;
    } else {
        const double s2 = std::sqrt(2.0);
        steps8 = {Step{-1, 0, 1.0}, Step{1, 0, 1.0},
                  Step{0, -1, 1.0}, Step{0, 1, 1.0},
                  Step{-1, -1, s2}, Step{-1, 1, s2},
                  Step{1, -1, s2},  Step{1, 1, s2}};
        nSteps = 8;
    }

    // Dijkstra main loop.
    while (!pq.empty()) {
        auto [d, k] = pq.top();
        pq.pop();
        if (d > dist[k]) continue;  // stale heap entry
        const std::size_t r = k % H;
        const std::size_t c = k / H;
        const double Iq = Idata[k];
        for (int s = 0; s < nSteps; ++s) {
            const long nr = static_cast<long>(r) + steps8[s].dr;
            const long nc = static_cast<long>(c) + steps8[s].dc;
            if (nr < 0 || nc < 0
             || static_cast<std::size_t>(nr) >= H
             || static_cast<std::size_t>(nc) >= W) continue;
            const std::size_t nk = static_cast<std::size_t>(nc) * H
                                 + static_cast<std::size_t>(nr);
            const double Ip = Idata[nk];
            const double w = steps8[s].chamfer * (Iq + Ip) * 0.5;
            const double nd = d + w;
            if (nd < dist[nk]) {
                dist[nk] = nd;
                pq.emplace(nd, nk);
            }
        }
    }

    // Write back to T with class cast.
    if (outT == ValueType::DOUBLE) {
        double *p = T.doubleDataMut();
        for (std::size_t i = 0; i < N; ++i) p[i] = dist[i];
    } else {
        float *p = T.singleDataMut();
        for (std::size_t i = 0; i < N; ++i) p[i] = static_cast<float>(dist[i]);
    }
    return T;
}

Value imoverlay(const Value &I, const Value &BW, const Value &color, std::pmr::memory_resource *mr)
{
    if (color.numel() != 3)
        throw Error("imoverlay: color must be a 1×3 RGB triple",
                    0, 0, "imoverlay", "", "m:imoverlay:color");
    const size_t H = I.dims().rows();
    const size_t W = I.dims().cols();
    if (BW.dims().rows() != H || BW.dims().cols() != W)
        throw Error("imoverlay: BW must match the H × W of I",
                    0, 0, "imoverlay", "", "m:imoverlay:shape");

    // Detect input layout: H × W (grayscale) or H × W × 3 (RGB).
    bool isRGB;
    if (I.numel() == H * W) isRGB = false;
    else if (I.numel() == H * W * 3) isRGB = true;
    else
        throw Error("imoverlay: I must be H × W or H × W × 3",
                    0, 0, "imoverlay", "", "m:imoverlay:shape");

    // Read color. Auto-detect float (0..1) vs byte (0..255) by max
    // value: if all three channels ≤ 1.0 we assume 0..1, else 0..255.
    double cIn[3] = { color.elemAsDouble(0),
                       color.elemAsDouble(1),
                       color.elemAsDouble(2) };
    const bool floatColour = (cIn[0] <= 1.0 && cIn[1] <= 1.0 &&
                               cIn[2] <= 1.0);
    int cByte[3];
    for (int k = 0; k < 3; ++k) {
        double v = floatColour ? cIn[k] * 255.0 : cIn[k];
        if (v < 0.0) v = 0.0;
        if (v > 255.0) v = 255.0;
        cByte[k] = static_cast<int>(std::lround(v));
    }

    // Helper: read I element as 0..255 byte.
    const ValueType srcT = I.type();
    auto pixelByte = [&](size_t y, size_t x, int chan) {
        size_t idx;
        if (isRGB)
            idx = static_cast<size_t>(chan) * H * W + x * H + y;
        else
            idx = x * H + y;
        const double v = I.elemAsDouble(idx);
        double w = v;
        if (srcT != ValueType::UINT8 && srcT != ValueType::UINT16 &&
            srcT != ValueType::INT8  && srcT != ValueType::INT16) {
            // Floating point: assume [0, 1].
            w = v * 255.0;
        } else if (srcT == ValueType::UINT16) {
            w = v / 257.0;
        } else if (srcT == ValueType::INT16) {
            w = (v + 32768.0) / 257.0;
        }
        if (w < 0.0) w = 0.0;
        if (w > 255.0) w = 255.0;
        return static_cast<std::uint8_t>(std::lround(w));
    };

    Value out = Value::matrix3d(H, W, 3, ValueType::UINT8, mr);
    std::uint8_t *od = out.uint8DataMut();
    const size_t plane = H * W;
    for (size_t y = 0; y < H; ++y)
        for (size_t x = 0; x < W; ++x) {
            const bool flag = (BW.elemAsDouble(x * H + y) != 0.0);
            for (int c = 0; c < 3; ++c) {
                const size_t outIdx = static_cast<size_t>(c) * plane +
                                       x * H + y;
                if (flag)
                    od[outIdx] = static_cast<std::uint8_t>(cByte[c]);
                else
                    od[outIdx] = pixelByte(y, x, isRGB ? c : 0);
            }
        }
    return out;
}

namespace detail {

void imoverlay_reg(Span<const Value> a, size_t, Span<Value> o,
                   CallContext &c)
{
    if (a.size() < 3)
        throw Error("imoverlay: requires (I, BW, color)",
                    0, 0, "imoverlay", "", "m:imoverlay:nargin");
    o[0] = imoverlay(a[0], a[1], a[2], c.engine->resource());
}

void grayconnected_reg(Span<const Value> a, size_t, Span<Value> o,
                       CallContext &c)
{
    if (a.size() < 3)
        throw Error("grayconnected: requires (I, row, col [, tol])",
                    0, 0, "grayconnected", "", "m:grayconnected:nargin");
    const int row = static_cast<int>(a[1].toScalar());
    const int col = static_cast<int>(a[2].toScalar());
    double tol = -1.0;
    if (a.size() >= 4 && !a[3].isEmpty()) tol = a[3].toScalar();
    o[0] = grayconnected(a[0], row, col, tol, c.engine->resource());
}

void dice_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("dice: requires (BW1, BW2)",
                    0, 0, "dice", "", "m:dice:nargin");
    o[0] = dice(a[0], a[1], c.engine->resource());
}

void jaccard_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("jaccard: requires (BW1, BW2)",
                    0, 0, "jaccard", "", "m:jaccard:nargin");
    o[0] = jaccard(a[0], a[1], c.engine->resource());
}

void boundarymask_reg(Span<const Value> a, size_t, Span<Value> o,
                      CallContext &c)
{
    if (a.empty())
        throw Error("boundarymask: requires (L_or_BW [, conn])",
                    0, 0, "boundarymask", "", "m:boundarymask:nargin");
    const int conn = (a.size() >= 2 && !a[1].isEmpty())
                     ? static_cast<int>(a[1].toScalar()) : 8;
    o[0] = boundarymask(a[0], conn, c.engine->resource());
}

// graydiffweight adapter — handles all 4 input signatures plus the
// 'RolloffFactor' / 'GrayDifferenceCutoff' name-value pairs by
// computing the scalar reference value upfront and dispatching to
// the typed entry-point.
void graydiffweight_reg(Span<const Value> a, size_t, Span<Value> o,
                        CallContext &c)
{
    if (a.size() < 2)
        throw Error("graydiffweight: requires (I, refGrayVal | MASK | "
                    "C, R [, P]) [, NV...]",
                    0, 0, "graydiffweight", "", "m:graydiffweight:nargin");
    auto *mr = c.engine->resource();
    const Value &I = a[0];
    const auto &d = I.dims();
    const std::size_t N = I.numel();
    const std::size_t H = d.rows();
    const std::size_t W_ = d.cols();
    const std::size_t P = d.is3D() ? d.pages() : 1;

    // Identify which signature is in play, then determine where the
    // name-value pairs start.
    double ref_gray_val = 0.0;
    std::size_t nv_start = 2;

    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    const bool a1_str = is_string(a[1]);

    if (!a1_str && a[1].isLogical()) {
        // (I, MASK) — mean of I over MASK = true.
        if (a[1].numel() != N)
            throw Error("graydiffweight: MASK must match I in shape",
                        0, 0, "graydiffweight", "", "m:graydiffweight:mask");
        long double sum = 0.0L; std::size_t cnt = 0;
        const std::uint8_t *m = a[1].logicalData();
        for (std::size_t i = 0; i < N; ++i)
            if (m[i]) { sum += I.elemAsDouble(i); ++cnt; }
        if (cnt == 0)
            throw Error("graydiffweight: MASK must contain at least one true",
                        0, 0, "graydiffweight", "", "m:graydiffweight:emptyMask");
        ref_gray_val = static_cast<double>(sum / static_cast<long double>(cnt));
        nv_start = 2;
    }
    else if (!a1_str && a[1].numel() == 1 &&
             (a.size() < 3 || a[2].isChar() || a[2].isString())) {
        // (I, refGrayVal) — scalar reference.
        ref_gray_val = a[1].toScalar();
        nv_start = 2;
    }
    else if (!a1_str && a.size() >= 3 && !is_string(a[2])) {
        // (I, C, R [, P]) — mean over indexed pixels.
        const Value &C = a[1], &R = a[2];
        const bool has_P = (a.size() >= 4 && !is_string(a[3]));
        const Value *Pi = has_P ? &a[3] : nullptr;
        if (C.numel() != R.numel() || (Pi && Pi->numel() != C.numel()))
            throw Error("graydiffweight: C, R [, P] must have equal length",
                        0, 0, "graydiffweight", "", "m:graydiffweight:crp");
        long double sum = 0.0L; std::size_t cnt = 0;
        for (std::size_t i = 0; i < C.numel(); ++i) {
            const std::size_t c1 = static_cast<std::size_t>(C.elemAsDouble(i));
            const std::size_t r1 = static_cast<std::size_t>(R.elemAsDouble(i));
            const std::size_t p1 = Pi ? static_cast<std::size_t>(Pi->elemAsDouble(i))
                                       : 1;
            if (c1 < 1 || c1 > W_ || r1 < 1 || r1 > H || p1 < 1 || p1 > P)
                throw Error("graydiffweight: (C, R [, P]) index out of range",
                            0, 0, "graydiffweight", "",
                            "m:graydiffweight:idx");
            // Column-major linear: r-1 + H*(c-1) + H*W*(p-1).
            const std::size_t lin = (r1 - 1) + H * (c1 - 1)
                                  + H * W_ * (p1 - 1);
            sum += I.elemAsDouble(lin);
            ++cnt;
        }
        ref_gray_val = static_cast<double>(sum / static_cast<long double>(cnt));
        nv_start = has_P ? 4 : 3;
    }
    else {
        throw Error("graydiffweight: 2nd argument must be a numeric "
                    "scalar, a logical MASK, or numeric C [, R [, P]]",
                    0, 0, "graydiffweight", "", "m:graydiffweight:arg2");
    }

    // Name-value pairs.
    double rolloff = 0.5;
    double cutoff  = std::numeric_limits<double>::infinity();
    std::size_t i = nv_start;
    while (i + 1 < a.size()) {
        if (!is_string(a[i]))
            throw Error("graydiffweight: expected NV-pair name string",
                        0, 0, "graydiffweight", "",
                        "m:graydiffweight:badNvArg");
        std::string name = a[i].toString();
        std::string nlo = name;
        for (auto &ch : nlo)
            ch = static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        // Allow MATLAB-style abbreviation.
        if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 4), "roll") == 0)
            rolloff = a[i + 1].toScalar();
        else if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 4), "gray") == 0)
            cutoff = a[i + 1].toScalar();
        else
            throw Error("graydiffweight: unknown option '" + name + "'",
                        0, 0, "graydiffweight", "",
                        "m:graydiffweight:unknownNv");
        i += 2;
    }
    if (i < a.size())
        throw Error("graydiffweight: trailing unpaired NV argument",
                    0, 0, "graydiffweight", "",
                    "m:graydiffweight:unpaired");

    o[0] = graydiffweight(I, ref_gray_val, rolloff, cutoff, mr);
}

// gradientweight adapter — parses (I [, sigma] [, NV...]).
//   sigma: scalar (replicated) or 2-element [sigma_x sigma_y]; default 1.5.
//   'RolloffFactor': positive scalar, default 3.
//   'WeightCutoff':  scalar in [1e-3, 1], default 0.25.
void gradientweight_reg(Span<const Value> a, size_t, Span<Value> o,
                        CallContext &c)
{
    if (a.empty())
        throw Error("gradientweight: requires (I [, sigma] [, NV...])",
                    0, 0, "gradientweight", "", "m:gradientweight:nargin");
    auto *mr = c.engine->resource();
    const Value &I = a[0];

    double sigma_x = 1.5, sigma_y = 1.5;
    double rolloff = 3.0;
    double cutoff  = 0.25;

    std::size_t nv_start = 1;
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    // Optional sigma argument.
    if (a.size() >= 2 && !is_string(a[1])) {
        const Value &s = a[1];
        const std::size_t ns = s.numel();
        if (ns == 1) {
            sigma_x = sigma_y = s.toScalar();
        } else if (ns == 2) {
            sigma_x = s.elemAsDouble(0);
            sigma_y = s.elemAsDouble(1);
        } else {
            throw Error("gradientweight: sigma must be a scalar or "
                        "2-element vector",
                        0, 0, "gradientweight", "",
                        "m:gradientweight:sigmaSize");
        }
        nv_start = 2;
    }

    // Name-value pairs.
    std::size_t i = nv_start;
    while (i + 1 < a.size()) {
        if (!is_string(a[i]))
            throw Error("gradientweight: expected NV-pair name string",
                        0, 0, "gradientweight", "",
                        "m:gradientweight:badNvArg");
        std::string name = a[i].toString();
        std::string nlo = name;
        for (auto &ch : nlo)
            ch = static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        // MATLAB-style abbreviation: "RolloffFactor" / "WeightCutoff".
        if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 4), "roll") == 0)
            rolloff = a[i + 1].toScalar();
        else if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 6), "weight") == 0
              || nlo.compare(0, std::min<std::size_t>(nlo.size(), 3), "cut") == 0)
            cutoff = a[i + 1].toScalar();
        else
            throw Error("gradientweight: unknown option '" + name + "'",
                        0, 0, "gradientweight", "",
                        "m:gradientweight:unknownNv");
        i += 2;
    }
    if (i < a.size())
        throw Error("gradientweight: trailing unpaired NV argument",
                    0, 0, "gradientweight", "",
                    "m:gradientweight:unpaired");

    o[0] = gradientweight(I, sigma_x, sigma_y, rolloff, cutoff, mr);
}

// regionfill adapter — both (I, MASK) and (I, X, Y) polygon forms.
void regionfill_reg(Span<const Value> a, size_t, Span<Value> o,
                    CallContext &c)
{
    if (a.size() < 2)
        throw Error("regionfill: requires (I, MASK) or (I, X, Y)",
                    0, 0, "regionfill", "", "m:regionfill:nargin");
    auto *mr = c.engine->resource();
    if (a.size() == 2) {
        o[0] = regionfill(a[0], a[1], mr);
        return;
    }
    // (I, X, Y) form — build mask via poly2mask using I's H/W.
    const Value &I = a[0];
    const std::size_t H = I.dims().rows();
    const std::size_t W = I.dims().cols();
    Value mask = poly2mask(a[1], a[2], H, W, mr);
    o[0] = regionfill(I, mask, mr);
}

// roipoly adapter — handles the 4 programmatic signatures and the
// 1/2/3/4/5-output forms. Interactive (1 / 2 / 0-arg) variants throw.
void roipoly_reg(Span<const Value> a, size_t nargout, Span<Value> o,
                 CallContext &c)
{
    if (a.size() < 3 || a.size() > 6)
        throw Error("roipoly: interactive forms not supported; use "
                    "(A, xi, yi) | (M, N, xi, yi) | (x, y, A, xi, yi) "
                    "| (x, y, M, N, xi, yi)",
                    0, 0, "roipoly", "", "m:roipoly:nargin");
    auto *mr = c.engine->resource();

    // Helper: pull [lo, hi] from a Value that's either a 2-elem extent
    // vector or a scalar (treated as a degenerate extent).
    auto extent = [&](const Value &v, double dflt_lo, double dflt_hi,
                      double &lo, double &hi) {
        if (v.numel() == 0) { lo = dflt_lo; hi = dflt_hi; }
        else if (v.numel() == 1) {
            lo = hi = v.toScalar();
        } else {
            lo = v.elemAsDouble(0);
            hi = v.elemAsDouble(v.numel() - 1);
        }
    };

    double xlo = 0, xhi = 0, ylo = 0, yhi = 0;
    std::size_t M = 0, N = 0;
    Value xi, yi;

    switch (a.size()) {
        case 3: {
            // (A, xi, yi)
            const Value &A = a[0];
            M = A.dims().rows();
            N = A.dims().cols();
            xlo = 1.0;  xhi = static_cast<double>(N);
            ylo = 1.0;  yhi = static_cast<double>(M);
            xi = a[1]; yi = a[2];
            break;
        }
        case 4: {
            // (M, N, xi, yi)
            const double Md = a[0].toScalar();
            const double Nd = a[1].toScalar();
            if (Md < 0 || Nd < 0 || Md != std::floor(Md) || Nd != std::floor(Nd))
                throw Error("roipoly: M and N must be non-negative integers",
                            0, 0, "roipoly", "", "m:roipoly:mn");
            M = static_cast<std::size_t>(Md);
            N = static_cast<std::size_t>(Nd);
            xlo = 1.0;  xhi = static_cast<double>(N);
            ylo = 1.0;  yhi = static_cast<double>(M);
            xi = a[2]; yi = a[3];
            break;
        }
        case 5: {
            // (x, y, A, xi, yi)
            const Value &A = a[2];
            M = A.dims().rows();
            N = A.dims().cols();
            extent(a[0], 1.0, static_cast<double>(N), xlo, xhi);
            extent(a[1], 1.0, static_cast<double>(M), ylo, yhi);
            xi = a[3]; yi = a[4];
            break;
        }
        case 6: {
            // (x, y, M, N, xi, yi)
            const double Md = a[2].toScalar();
            const double Nd = a[3].toScalar();
            if (Md < 0 || Nd < 0 || Md != std::floor(Md) || Nd != std::floor(Nd))
                throw Error("roipoly: M and N must be non-negative integers",
                            0, 0, "roipoly", "", "m:roipoly:mn");
            M = static_cast<std::size_t>(Md);
            N = static_cast<std::size_t>(Nd);
            extent(a[0], 1.0, static_cast<double>(N), xlo, xhi);
            extent(a[1], 1.0, static_cast<double>(M), ylo, yhi);
            xi = a[4]; yi = a[5];
            break;
        }
    }

    // Build the auto-closed xi/yi for the multi-output forms (matches
    // MATLAB's behaviour: outputs the closed polygon as a column vector).
    const std::size_t n0 = xi.numel();
    std::pmr::vector<double> xc(mr), yc(mr);
    xc.reserve(n0 + 1);
    yc.reserve(n0 + 1);
    for (std::size_t i = 0; i < n0; ++i) {
        xc.push_back(xi.elemAsDouble(i));
        yc.push_back(yi.elemAsDouble(i));
    }
    if (n0 > 0 && (xc.front() != xc.back() || yc.front() != yc.back())) {
        xc.push_back(xc.front());
        yc.push_back(yc.front());
    }
    const std::size_t nc = xc.size();
    Value xiOut = Value::matrix(nc, 1, ValueType::DOUBLE, mr);
    Value yiOut = Value::matrix(nc, 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < nc; ++i) {
        xiOut.doubleDataMut()[i] = xc[i];
        yiOut.doubleDataMut()[i] = yc[i];
    }

    Value BW = roipoly(xlo, xhi, ylo, yhi, M, N, xi, yi, mr);

    // Output dispatch.
    Value xDataOut = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    xDataOut.doubleDataMut()[0] = xlo; xDataOut.doubleDataMut()[1] = xhi;
    Value yDataOut = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    yDataOut.doubleDataMut()[0] = ylo; yDataOut.doubleDataMut()[1] = yhi;

    switch (nargout) {
        case 0: case 1: o[0] = std::move(BW); break;
        case 2: o[0] = std::move(BW); o[1] = std::move(xiOut); break;
        case 3: o[0] = std::move(BW); o[1] = std::move(xiOut);
                o[2] = std::move(yiOut); break;
        case 4: o[0] = std::move(xDataOut); o[1] = std::move(yDataOut);
                o[2] = std::move(BW); o[3] = std::move(xiOut); break;
        case 5: o[0] = std::move(xDataOut); o[1] = std::move(yDataOut);
                o[2] = std::move(BW); o[3] = std::move(xiOut);
                o[4] = std::move(yiOut); break;
        default:
            throw Error("roipoly: too many output arguments (max 5)",
                        0, 0, "roipoly", "", "m:roipoly:tooManyOutputs");
    }
}

// graydist adapter — handles the 4 input forms + optional METHOD.
void graydist_reg(Span<const Value> a, size_t, Span<Value> o,
                  CallContext &c)
{
    if (a.size() < 2)
        throw Error("graydist: requires (A, mask | ind | C, R "
                    "[, method])",
                    0, 0, "graydist", "", "m:graydist:nargin");
    auto *mr = c.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    // Strip trailing method string if present.
    std::string method = "chessboard";
    std::size_t nargs = a.size();
    if (is_string(a[nargs - 1])) {
        method = a[nargs - 1].toString();
        std::string lo;
        for (char ch : method)
            lo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        method = lo;
        --nargs;
    }

    const Value &A = a[0];
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();

    // Build 1-based linear seed indices.
    std::pmr::vector<double> indices(mr);
    if (nargs == 2) {
        const Value &arg2 = a[1];
        if (arg2.isLogical()) {
            // Mask: same size as A.
            if (arg2.numel() != H * W)
                throw Error("graydist: MASK must be the same size as A",
                            0, 0, "graydist", "", "m:graydist:maskSize");
            const std::uint8_t *m = arg2.logicalData();
            for (std::size_t i = 0; i < arg2.numel(); ++i)
                if (m[i]) indices.push_back(static_cast<double>(i + 1));
        } else {
            // Linear indices.
            indices.reserve(arg2.numel());
            for (std::size_t i = 0; i < arg2.numel(); ++i)
                indices.push_back(arg2.elemAsDouble(i));
        }
    } else if (nargs == 3) {
        // (A, C, R) — column-then-row coordinates.
        const Value &C = a[1];
        const Value &R = a[2];
        if (C.numel() != R.numel())
            throw Error("graydist: C and R must have equal length",
                        0, 0, "graydist", "", "m:graydist:cr");
        indices.reserve(C.numel());
        for (std::size_t i = 0; i < C.numel(); ++i) {
            const std::size_t cc = static_cast<std::size_t>(C.elemAsDouble(i));
            const std::size_t rr = static_cast<std::size_t>(R.elemAsDouble(i));
            if (cc < 1 || cc > W || rr < 1 || rr > H)
                throw Error("graydist: (C, R) out of bounds",
                            0, 0, "graydist", "", "m:graydist:crBounds");
            // Column-major 1-based linear index.
            const std::size_t lin = (cc - 1) * H + rr;  // 1-based
            indices.push_back(static_cast<double>(lin));
        }
    } else {
        throw Error("graydist: too many positional arguments",
                    0, 0, "graydist", "", "m:graydist:nargin");
    }

    Value seedVec = Value::matrix(indices.size(), 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < indices.size(); ++i)
        seedVec.doubleDataMut()[i] = indices[i];
    o[0] = graydist(A, seedVec, method, mr);
}

void poly2mask_reg(Span<const Value> a, size_t, Span<Value> o,
                   CallContext &c)
{
    if (a.size() < 4)
        throw Error("poly2mask: requires (X, Y, M, N)",
                    0, 0, "poly2mask", "", "m:poly2mask:nargin");
    auto *mr = c.engine->resource();
    const double Md = a[2].toScalar();
    const double Nd = a[3].toScalar();
    if (!std::isfinite(Md) || !std::isfinite(Nd)
     || Md < 0.0 || Nd < 0.0
     || Md != std::floor(Md) || Nd != std::floor(Nd))
        throw Error("poly2mask: M and N must be non-negative integers",
                    0, 0, "poly2mask", "", "m:poly2mask:mn");
    o[0] = poly2mask(a[0], a[1],
                     static_cast<std::size_t>(Md),
                     static_cast<std::size_t>(Nd), mr);
}

void label2idx_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("label2idx: requires (L)",
                    0, 0, "label2idx", "", "m:label2idx:nargin");
    o[0] = label2idx(a[0], c.engine->resource());
}

} // namespace detail

} // namespace numkit::image
