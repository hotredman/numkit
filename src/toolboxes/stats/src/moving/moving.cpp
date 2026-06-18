// toolboxes/stats/src/moving/moving.cpp
// Sliding-window statistics: movmean / movmedian / movsum / movmin /
// movmax / movstd / movvar / movmad / movprod, plus smoothdata + hampel.
// Strategy: a single per-slice driver walks the chosen dim; per-window
// reduction is supplied as a callable. O(N·k) — not the asymptotically
// optimal bound, but matches the rest of the library's "correctness
// first, optimise after benches" policy.

#include <numkit/stats/moving/moving.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>            // createLike, createForDims (toolboxes/builtin/src/)
#include <numkit/ops/reductions.hpp>  // numkit::ops::firstNonSingletonDim, validateDim

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>

#include "moving_detail.hpp"

namespace numkit::stats {


// ── internal *_impl with full MovOpts ─────────────────────────────────


// ── public API (legacy dim-only sigs; defaults for extras) ────────────

Value movmean(const Value &x, Span<const size_t> k, int dim, std::pmr::memory_resource *mr)
{ MovOpts o; o.dim = dim; return movmean_impl(x, k, o, mr); }
Value movsum(const Value &x, Span<const size_t> k, int dim, std::pmr::memory_resource *mr)
{ MovOpts o; o.dim = dim; return movsum_impl(x, k, o, mr); }
Value movmin(const Value &x, Span<const size_t> k, int dim, std::pmr::memory_resource *mr)
{ MovOpts o; o.dim = dim; return movmin_impl(x, k, o, mr); }
Value movmax(const Value &x, Span<const size_t> k, int dim, std::pmr::memory_resource *mr)
{ MovOpts o; o.dim = dim; return movmax_impl(x, k, o, mr); }
Value movprod(const Value &x, Span<const size_t> k, int dim, std::pmr::memory_resource *mr)
{ MovOpts o; o.dim = dim; return movprod_impl(x, k, o, mr); }
Value movmedian(const Value &x, Span<const size_t> k, int dim, std::pmr::memory_resource *mr)
{ MovOpts o; o.dim = dim; return movmedian_impl(x, k, o, mr); }
Value movvar(const Value &x, Span<const size_t> k, int normFlag, int dim, std::pmr::memory_resource *mr)
{ MovOpts o; o.dim = dim; return movvar_impl(x, k, normFlag, o, mr); }
Value movstd(const Value &x, Span<const size_t> k, int normFlag, int dim, std::pmr::memory_resource *mr)
{ MovOpts o; o.dim = dim; return movstd_impl(x, k, normFlag, o, mr); }
Value movmad(const Value &x, Span<const size_t> k, int dim, std::pmr::memory_resource *mr)
{ MovOpts o; o.dim = dim; return movmad_impl(x, k, o, mr); }

// Gaussian-weighted moving average for smoothdata 'gaussian'. MATLAB R2025b
// uses kernel exp(-d^2/(2*sigma^2)) with sigma = windowLength/5, CENTRED on
// the current sample, with the default 'shrink' endpoints (truncate the
// kernel at the array edges and renormalise). NaNs are omitted from the
// weighted sum when opt.omit_nan. dim 1/2/3 + vector. The generic moving
// driver cannot align a weighted kernel at truncated edges (its reducer only
// sees the shrunk window, not the centre position), so this path walks slices
// directly. Inputs are DOUBLE-backed (mirrors the rest of the moving driver).
static Value smoothGaussianDim(const Value &x, const Window &w, double sigma,
                               int dim, const MovOpts &opt,
                               std::pmr::memory_resource *mr)
{
    if (x.isEmpty()) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (x.isScalar()) return Value::scalar(x.toScalar(), mr);

    const long kb = w.kb, kf = w.kf;
    const double inv2s2 = (sigma > 0.0) ? 1.0 / (2.0 * sigma * sigma) : 0.0;
    auto perSlice = [&](const double *src, size_t n, ptrdiff_t step, double *dst) {
        const long N = static_cast<long>(n);
        for (long i = 0; i < N; ++i) {
            const long lo = std::max(0L, i - kb), hi = std::min(N - 1, i + kf);
            double sw = 0.0, ssum = 0.0;
            for (long j = lo; j <= hi; ++j) {
                const double v = src[j * step];
                if (opt.omit_nan && std::isnan(v)) continue;
                const double dd = static_cast<double>(j - i);
                const double wt = std::exp(-dd * dd * inv2s2);
                ssum += wt * v;
                sw += wt;
            }
            dst[i * step] = (sw > 0.0) ? ssum / sw
                                       : std::numeric_limits<double>::quiet_NaN();
        }
    };

    auto out = createLike(x, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const double *src = x.doubleData();
    const auto &d = x.dims();
    if (d.isVector()) { perSlice(src, x.numel(), 1, dst); return out; }

    const size_t R = d.rows(), C = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1, pg = R * C;
    if (dim == 1) {
        for (size_t p = 0; p < P; ++p)
            for (size_t c = 0; c < C; ++c)
                perSlice(src + p * pg + c * R, R, 1, dst + p * pg + c * R);
    } else if (dim == 2) {
        for (size_t p = 0; p < P; ++p)
            for (size_t r = 0; r < R; ++r)
                perSlice(src + p * pg + r, C, static_cast<ptrdiff_t>(R), dst + p * pg + r);
    } else if (dim == 3 && d.is3D()) {
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r)
                perSlice(src + c * R + r, P, static_cast<ptrdiff_t>(pg), dst + c * R + r);
    } else {
        std::copy(src, src + x.numel(), dst);
    }
    return out;
}

// Savitzky-Golay projection matrix B = A·(A'A)⁻¹·A' (F×F, row-major), where A
// is the Vandermonde of [-m..m] to degree `deg`. Row m is the steady-state
// smoothing kernel; the boundary rows handle the F-point edge windows. Matches
// MATLAB sgolay/sgolayfilt exactly.
static void buildSGMatrix(int F, int deg, std::vector<double> &B)
{
    const int p1 = deg + 1;
    const int m  = (F - 1) / 2;
    std::vector<double> AtA(static_cast<size_t>(p1) * p1, 0.0);
    for (int a = 0; a < p1; ++a)
        for (int b = 0; b < p1; ++b) {
            double s = 0.0;
            for (int i = 0; i < F; ++i) s += std::pow(static_cast<double>(i - m), a + b);
            AtA[a * p1 + b] = s;
        }
    // Invert AtA via Gauss-Jordan on [AtA | I].
    const int W = 2 * p1;
    std::vector<double> aug(static_cast<size_t>(p1) * W, 0.0);
    for (int i = 0; i < p1; ++i) { for (int j = 0; j < p1; ++j) aug[i * W + j] = AtA[i * p1 + j]; aug[i * W + p1 + i] = 1.0; }
    for (int col = 0; col < p1; ++col) {
        int piv = col; double best = std::fabs(aug[col * W + col]);
        for (int r = col + 1; r < p1; ++r) { double v = std::fabs(aug[r * W + col]); if (v > best) { best = v; piv = r; } }
        if (piv != col) for (int j = 0; j < W; ++j) std::swap(aug[col * W + j], aug[piv * W + j]);
        const double d = aug[col * W + col];
        for (int j = 0; j < W; ++j) aug[col * W + j] /= d;
        for (int r = 0; r < p1; ++r) {
            if (r == col) continue;
            const double f = aug[r * W + col];
            if (f == 0.0) continue;
            for (int j = 0; j < W; ++j) aug[r * W + j] -= f * aug[col * W + j];
        }
    }
    B.assign(static_cast<size_t>(F) * F, 0.0);
    for (int i = 0; i < F; ++i)
        for (int j = 0; j < F; ++j) {
            double s = 0.0;
            for (int a = 0; a < p1; ++a) {
                const double ia = std::pow(static_cast<double>(i - m), a);
                for (int b = 0; b < p1; ++b)
                    s += ia * aug[a * W + p1 + b] * std::pow(static_cast<double>(j - m), b);
            }
            B[static_cast<size_t>(i) * F + j] = s;
        }
}

// Apply the SG projection over one slice (stride `step`): interior samples use
// the centre row; the first/last m samples use the boundary rows of the F-point
// edge window. Matches MATLAB sgolayfilt's edge handling.
static void sgolaySlice(const double *src, size_t n, ptrdiff_t step, double *dst,
                        const std::vector<double> &B, int F)
{
    const int N = static_cast<int>(n);
    const int m = (F - 1) / 2;
    for (int nn = 0; nn < N; ++nn) {
        int row, base;
        if (nn < m)            { row = nn;            base = 0;     }   // leading edge
        else if (nn >= N - m)  { row = nn - (N - F);  base = N - F; }   // trailing edge
        else                   { row = m;             base = nn - m; }  // interior
        double s = 0.0;
        for (int j = 0; j < F; ++j) s += B[static_cast<size_t>(row) * F + j] * src[static_cast<ptrdiff_t>(base + j) * step];
        dst[static_cast<ptrdiff_t>(nn) * step] = s;
    }
}

// Savitzky-Golay smoothing for smoothdata 'sgolay' (degree 2). Walks slices
// along `dim`; reduces the window to fit short slices.
static Value smoothSGDim(const Value &x, int F, int deg, int dim, std::pmr::memory_resource *mr)
{
    if (x.isEmpty()) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (x.isScalar()) return Value::scalar(x.toScalar(), mr);

    auto          out = createLike(x, ValueType::DOUBLE, mr);
    double *      dst = out.doubleDataMut();
    const double *src = x.doubleData();
    const auto &  d   = x.dims();

    const size_t sliceLen = d.isVector() ? x.numel()
                                         : (dim == 2 ? d.cols() : (dim == 3 ? (d.is3D() ? d.pages() : 1) : d.rows()));
    int Fe = F;
    if (Fe > static_cast<int>(sliceLen)) Fe = static_cast<int>(sliceLen);
    if (Fe % 2 == 0) --Fe;                       // SG window must be odd
    if (Fe < deg + 1) Fe = deg + 1 | 1;          // need > degree (odd)
    if (Fe < 1) Fe = 1;
    std::vector<double> B;
    if (Fe >= deg + 1) buildSGMatrix(Fe, deg, B);

    auto perSlice = [&](const double *s, size_t nlen, ptrdiff_t step, double *o) {
        if (Fe < deg + 1 || nlen < static_cast<size_t>(Fe)) {
            for (size_t i = 0; i < nlen; ++i) o[i * step] = s[i * step];  // too short → passthrough
            return;
        }
        sgolaySlice(s, nlen, step, o, B, Fe);
    };

    if (d.isVector()) { perSlice(src, x.numel(), 1, dst); return out; }
    const size_t R = d.rows(), C = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1, pg = R * C;
    if (dim == 1) {
        for (size_t p = 0; p < P; ++p)
            for (size_t c = 0; c < C; ++c)
                perSlice(src + p * pg + c * R, R, 1, dst + p * pg + c * R);
    } else if (dim == 2) {
        for (size_t p = 0; p < P; ++p)
            for (size_t r = 0; r < R; ++r)
                perSlice(src + p * pg + r, C, static_cast<ptrdiff_t>(R), dst + p * pg + r);
    } else if (dim == 3 && d.is3D()) {
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r)
                perSlice(src + c * R + r, P, static_cast<ptrdiff_t>(pg), dst + c * R + r);
    } else {
        std::copy(src, src + x.numel(), dst);
    }
    return out;
}

// Local tricube-weighted polynomial regression over one slice for smoothdata
// 'lowess' (deg 1) / 'loess' (deg 2). For each sample the F-point window is
// shifted to stay in range; weights are tricube of the in-window distance
// (window edges → weight 0); the fitted value is the constant term (the fit at
// the query point). Matches MATLAB exactly. Rank-deficient windows fall back to
// the weighted mean. P = deg+1 ≤ 3.
static void localRegressSlice(const double *src, size_t n, ptrdiff_t step, double *dst,
                              int F, int deg)
{
    const int N = static_cast<int>(n);
    const int m = (F - 1) / 2;
    const int P = deg + 1;
    for (int i = 0; i < N; ++i) {
        int lo = i - m;
        if (lo < 0)      lo = 0;
        if (lo > N - F)  lo = N - F;
        double dmax = 0.0;
        for (int j = lo; j < lo + F; ++j) { const double d = std::fabs(j - i); if (d > dmax) dmax = d; }
        if (dmax == 0.0) dmax = 1.0;

        double AtA[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
        double Atb[3] = {0, 0, 0};
        for (int j = lo; j < lo + F; ++j) {
            double u = std::fabs(j - i) / dmax;
            if (u > 1.0) u = 1.0;
            const double t1 = 1.0 - u * u * u;
            const double w  = t1 * t1 * t1;             // tricube
            if (w == 0.0) continue;
            double basis[3];
            basis[0] = 1.0;
            for (int p = 1; p < P; ++p) basis[p] = basis[p - 1] * (j - i);
            const double yj = src[static_cast<ptrdiff_t>(j) * step];
            for (int a = 0; a < P; ++a) {
                Atb[a] += w * basis[a] * yj;
                for (int b = 0; b < P; ++b) AtA[a * P + b] += w * basis[a] * basis[b];
            }
        }
        // Solve the P×P system; the value is x[0]. Gaussian elimination with a
        // pivot threshold; on rank-deficiency fall back to the weighted mean.
        double M[9], b[3], val;
        for (int t = 0; t < P * P; ++t) M[t] = AtA[t];
        for (int t = 0; t < P; ++t)     b[t] = Atb[t];
        bool ok = true;
        for (int col = 0; col < P && ok; ++col) {
            int piv = col; double best = std::fabs(M[col * P + col]);
            for (int r = col + 1; r < P; ++r) { const double v = std::fabs(M[r * P + col]); if (v > best) { best = v; piv = r; } }
            if (best < 1e-12) { ok = false; break; }
            if (piv != col) { for (int j = 0; j < P; ++j) std::swap(M[col * P + j], M[piv * P + j]); std::swap(b[col], b[piv]); }
            const double d = M[col * P + col];
            for (int j = 0; j < P; ++j) M[col * P + j] /= d;
            b[col] /= d;
            for (int r = 0; r < P; ++r) { if (r == col) continue; const double f = M[r * P + col]; for (int j = 0; j < P; ++j) M[r * P + j] -= f * M[col * P + j]; b[r] -= f * b[col]; }
        }
        val = ok ? b[0] : (AtA[0] > 0.0 ? Atb[0] / AtA[0] : src[static_cast<ptrdiff_t>(i) * step]);
        dst[static_cast<ptrdiff_t>(i) * step] = val;
    }
}

// smoothdata 'lowess'/'loess' driver — walks slices along `dim`.
static Value smoothLocalRegDim(const Value &x, int F, int deg, int dim, std::pmr::memory_resource *mr)
{
    if (x.isEmpty()) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (x.isScalar()) return Value::scalar(x.toScalar(), mr);

    auto          out = createLike(x, ValueType::DOUBLE, mr);
    double *      dst = out.doubleDataMut();
    const double *src = x.doubleData();
    const auto &  d   = x.dims();

    const size_t sliceLen = d.isVector() ? x.numel()
                                         : (dim == 2 ? d.cols() : (dim == 3 ? (d.is3D() ? d.pages() : 1) : d.rows()));
    int Fe = F;
    if (Fe > static_cast<int>(sliceLen)) Fe = static_cast<int>(sliceLen);
    if (Fe < 1) Fe = 1;

    auto perSlice = [&](const double *s, size_t nlen, ptrdiff_t step, double *o) {
        if (nlen < static_cast<size_t>(Fe) || Fe < 2) {
            for (size_t i = 0; i < nlen; ++i) o[i * step] = s[i * step];
            return;
        }
        localRegressSlice(s, nlen, step, o, Fe, deg);
    };

    if (d.isVector()) { perSlice(src, x.numel(), 1, dst); return out; }
    const size_t R = d.rows(), C = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1, pg = R * C;
    if (dim == 1) {
        for (size_t p = 0; p < P; ++p)
            for (size_t c = 0; c < C; ++c)
                perSlice(src + p * pg + c * R, R, 1, dst + p * pg + c * R);
    } else if (dim == 2) {
        for (size_t p = 0; p < P; ++p)
            for (size_t r = 0; r < R; ++r)
                perSlice(src + p * pg + r, C, static_cast<ptrdiff_t>(R), dst + p * pg + r);
    } else if (dim == 3 && d.is3D()) {
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r)
                perSlice(src + c * R + r, P, static_cast<ptrdiff_t>(pg), dst + c * R + r);
    } else {
        std::copy(src, src + x.numel(), dst);
    }
    return out;
}

// ── smoothdata ────────────────────────────────────────────────────────
Value smoothdata(const Value &x, const std::string &method, int k, int dim, std::pmr::memory_resource *mr)
{
    std::string m = method;
    for (auto &c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // Heuristic default window — MATLAB picks ~max(min(round(0.1*n), 10), 3).
    if (k <= 0) {
        const size_t n = (x.dims().isVector() || x.isScalar())
                          ? x.numel()
                          : x.dims().rows();
        long auto_k = static_cast<long>(std::round(0.1 * static_cast<double>(n)));
        if (auto_k < 3) auto_k = 3;
        if (auto_k > static_cast<long>(n)) auto_k = static_cast<long>(n);
        k = static_cast<int>(auto_k);
        if (k < 1) k = 1;
    }
    const size_t kArr[1] = { static_cast<size_t>(k) };
    Span<const size_t> kSpan(kArr, 1);

    MovOpts opt;
    opt.dim = dim;
    if (m == "movmean" || m.empty())
        return movmean_impl(x, kSpan, opt, mr);
    if (m == "movmedian")
        return movmedian_impl(x, kSpan, opt, mr);
    if (m == "gaussian") {
        // Gaussian-weighted moving average. MATLAB R2025b uses sigma =
        // windowLength/5 and centres the kernel on the CURRENT sample, with
        // 'shrink' endpoints. (Was sigma=(k-1)/4 + a mis-aligned kernel at
        // truncated edges -> wrong at the boundaries and interior.)
        const auto w = decodeWindow(kSpan, "smoothdata");
        const int d = resolveDim(x, dim, "smoothdata");
        const double sigma = (k > 0) ? static_cast<double>(k) / 5.0 : 0.2;
        return smoothGaussianDim(x, w, sigma, d, opt, mr);
    }
    if (m == "sgolay") {
        // Savitzky-Golay (degree 2). Matches MATLAB exactly for an explicit odd
        // window; the auto default window is approximate (MATLAB's is a
        // data-dependent heuristic that this shared k-default doesn't replicate).
        const int d = resolveDim(x, dim, "smoothdata");
        return smoothSGDim(x, k, /*deg=*/2, d, mr);
    }
    if (m == "lowess" || m == "loess") {
        // Local tricube-weighted regression — linear (lowess) / quadratic
        // (loess). Matches MATLAB exactly for an explicit window; auto default
        // window is approximate (same data-dependent-heuristic caveat as sgolay).
        const int d = resolveDim(x, dim, "smoothdata");
        return smoothLocalRegDim(x, k, m == "loess" ? 2 : 1, d, mr);
    }
    throw Error("smoothdata: method '" + method + "' not supported "
                 "(supported: 'movmean', 'movmedian', 'gaussian', 'sgolay', "
                 "'lowess', 'loess')",
                 0, 0, "smoothdata", "", "numkit:smoothdata:unsupportedMethod");
}

// ── hampel ────────────────────────────────────────────────────────────

// MATLAB-exact MAD→σ normal-consistency constant 1/norminv(0.75). MATLAB's
// hampel reports xsigma = 1.482602218505602*MAD and uses the same factor in
// its threshold, so we match it exactly (the looser 1.4826 misses xsigma by
// ~1e-5 and can flip borderline detections).
constexpr double kHampelMadToStd = 1.4826022185056018;

// Core single-pass hampel filter over a length-`n` vector. Always fills
// `dst`; when non-null, also fills `mask` (1 where the point was replaced),
// `median` (local window median), and `sigma` (local 1.4826·MAD estimate).
void hampelCore(const double *src, std::size_t n, int k, double nsigmas,
                       double *dst, std::uint8_t *mask, double *median,
                       double *sigma, std::pmr::memory_resource *mr)
{
    if (n == 0) return;
    ScratchArena scratch(mr);
    auto buf = ScratchVec<double>(static_cast<size_t>(2 * k + 1), &scratch);
    for (long i = 0; i < static_cast<long>(n); ++i) {
        const long lo = std::max<long>(0, i - k);
        const long hi = std::min<long>(static_cast<long>(n) - 1, i + k);
        const long len = hi - lo + 1;
        for (long j = 0; j < len; ++j)
            buf[static_cast<size_t>(j)] = src[lo + j];
        // Median + MAD on the same window.
        auto copy1 = ScratchVec<double>(static_cast<size_t>(len), &scratch);
        std::copy(buf.data(), buf.data() + len, copy1.data());
        const double med = winMedianInPlace(copy1.data(), static_cast<size_t>(len));
        auto devs = ScratchVec<double>(static_cast<size_t>(len), &scratch);
        for (long j = 0; j < len; ++j)
            devs[static_cast<size_t>(j)] = std::abs(buf[static_cast<size_t>(j)] - med);
        const double mad = winMedianInPlace(devs.data(), static_cast<size_t>(len));
        const double sig = kHampelMadToStd * mad;
        const bool isOut = std::abs(src[i] - med) > nsigmas * sig;
        dst[i] = isOut ? med : src[i];
        if (mask)   mask[static_cast<size_t>(i)]   = isOut ? 1 : 0;
        if (median) median[static_cast<size_t>(i)] = med;
        if (sigma)  sigma[static_cast<size_t>(i)]  = sig;
    }
}

void hampelValidate(const Value &x, int k, double nsigmas)
{
    if (k < 0)
        throw Error("hampel: k must be >= 0",
                     0, 0, "hampel", "", "numkit:hampel:badK");
    if (nsigmas <= 0)
        throw Error("hampel: nsigmas must be positive",
                     0, 0, "hampel", "", "numkit:hampel:badSigmas");
    if (!x.dims().isVector() && !x.isScalar())
        throw Error("hampel: vector input only (matrix form deferred)",
                     0, 0, "hampel", "", "numkit:hampel:notVector");
}

Value hampel(const Value &x, int k, double nsigmas, std::pmr::memory_resource *mr)
{
    hampelValidate(x, k, nsigmas);
    auto out = createLike(x, ValueType::DOUBLE, mr);
    const size_t n = x.numel();
    if (n == 0) return out;
    hampelCore(x.doubleData(), n, k, nsigmas, out.doubleDataMut(),
               nullptr, nullptr, nullptr, mr);
    return out;
}

} // namespace numkit::stats
