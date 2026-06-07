// libs/stats/src/moving/moving.cpp
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

#include "helpers.hpp"            // createLike, createForDims (libs/builtin/src/)
#include "reduction_helpers.hpp"  // numkit::builtin::detail::firstNonSingletonDim, validateDim

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
    throw Error("smoothdata: method '" + method + "' not supported "
                 "(supported: 'movmean', 'movmedian', 'gaussian')",
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
