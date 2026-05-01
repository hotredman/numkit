// libs/stats/src/moving/moving.cpp
//
// Sliding-window statistics: movmean / movmedian / movsum / movmin /
// movmax / movstd / movvar / movmad / movprod, plus smoothdata + hampel.
//
// Strategy: a single per-slice driver walks the chosen dim; per-window
// reduction is supplied as a callable. O(N·k) — not the asymptotically
// optimal bound, but matches the rest of the library's "correctness
// first, optimise after benches" policy.

#include <numkit/stats/moving/moving.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"            // createLike, createForDims (libs/builtin/src/)
#include "reduction_helpers.hpp"  // numkit::builtin::detail::firstNonSingletonDim, validateDim

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>

namespace numkit::stats {

namespace {

using ::numkit::builtin::detail::firstNonSingletonDim;
using ::numkit::builtin::detail::validateDim;

// Resolve user-supplied dim (0 → first non-singleton, otherwise validate).
int resolveDim(const Value &x, int dim, const char *fn)
{
    if (dim == 0)
        return firstNonSingletonDim(x);
    return validateDim(x, dim, fn);
}

// Decode the `k` argument: scalar → centred [floor((k-1)/2), floor(k/2)];
// 2-element vector → [kb, kf].
struct Window {
    long kb;   // samples back (inclusive of centre? no — i-kb is the start)
    long kf;   // samples forward
};

Window decodeWindow(const Value &k, const char *fn)
{
    if (k.isScalar()) {
        const double v = k.toScalar();
        if (!(v >= 0) || std::floor(v) != v)
            throw Error(std::string(fn) + ": window size must be a non-negative integer",
                         0, 0, fn, "", std::string("m:") + fn + ":badK");
        const long n = static_cast<long>(v);
        // MATLAB: centred window — leading floor((k-1)/2), trailing floor(k/2).
        return {(n - 1) / 2, n / 2};
    }
    if (k.numel() == 2) {
        const double a = k.elemAsDouble(0);
        const double b = k.elemAsDouble(1);
        if (a < 0 || b < 0 || std::floor(a) != a || std::floor(b) != b)
            throw Error(std::string(fn) + ": [kb kf] must be non-negative integers",
                         0, 0, fn, "", std::string("m:") + fn + ":badK");
        return {static_cast<long>(a), static_cast<long>(b)};
    }
    throw Error(std::string(fn) + ": k must be a scalar or 2-element vector",
                 0, 0, fn, "", std::string("m:") + fn + ":badK");
}

// Allocate a same-shape DOUBLE output via createLike.
Value allocSameShape(std::pmr::memory_resource *mr, const Value &x)
{
    return createLike(x, ValueType::DOUBLE, mr);
}

// Apply per-window reducer F over a 1-D run [src, src+n) with stride
// `step` (in elements). Output written to dst with the same stride.
// F has signature `double(const double *win, size_t winLen)`.
template <typename F>
void runMoving(const double *src, size_t n, ptrdiff_t step,
               double *dst, const Window &w, ScratchArena &scratch, F &&fn)
{
    if (n == 0) return;
    auto buf = ScratchVec<double>(static_cast<size_t>(w.kb + w.kf + 1), &scratch);
    for (long i = 0; i < static_cast<long>(n); ++i) {
        const long lo = std::max<long>(0, i - w.kb);
        const long hi = std::min<long>(static_cast<long>(n) - 1, i + w.kf);
        const long len = hi - lo + 1;
        if (len <= 0) {
            dst[i * step] = std::numeric_limits<double>::quiet_NaN();
            continue;
        }
        for (long j = 0; j < len; ++j)
            buf[static_cast<size_t>(j)] = src[(lo + j) * step];
        dst[i * step] = fn(buf.data(), static_cast<size_t>(len));
    }
}

// Driver with explicit dim. Handles vector / 2-D / 3-D.
template <typename F>
Value movingDriverDim(std::pmr::memory_resource *mr, const Value &x,
                      const Window &w, int dim, F &&fn)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (x.isScalar()) {
        auto out = Value::scalar(0.0, mr);
        const double v = x.toScalar();
        out.doubleDataMut()[0] = fn(&v, 1);
        return out;
    }
    auto out = allocSameShape(mr, x);
    ScratchArena scratch(mr);
    const double *src = x.doubleData();
    double *dst = out.doubleDataMut();

    if (x.dims().isVector()) {
        runMoving(src, x.numel(), /*step=*/1, dst, w, scratch, fn);
        return out;
    }

    const auto &d = x.dims();
    const size_t R = d.rows(), C = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1;
    const size_t pageStride = R * C;

    if (dim == 1) {
        // Down columns (column-major: stride = 1, length = R, per col & page).
        for (size_t p = 0; p < P; ++p)
            for (size_t c = 0; c < C; ++c) {
                const double *colSrc = src + p * pageStride + c * R;
                double       *colDst = dst + p * pageStride + c * R;
                runMoving(colSrc, R, /*step=*/1, colDst, w, scratch, fn);
            }
    } else if (dim == 2) {
        // Across rows (stride = R, length = C, per row & page).
        for (size_t p = 0; p < P; ++p)
            for (size_t r = 0; r < R; ++r) {
                const double *rowSrc = src + p * pageStride + r;
                double       *rowDst = dst + p * pageStride + r;
                runMoving(rowSrc, C, /*step=*/static_cast<ptrdiff_t>(R),
                          rowDst, w, scratch, fn);
            }
    } else if (dim == 3 && d.is3D()) {
        // Across pages (stride = R*C, length = P, per (r, c)).
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                const double *pgSrc = src + c * R + r;
                double       *pgDst = dst + c * R + r;
                runMoving(pgSrc, P, /*step=*/static_cast<ptrdiff_t>(pageStride),
                          pgDst, w, scratch, fn);
            }
    } else {
        // Trailing-singleton or out-of-rank: identity copy.
        std::copy(src, src + x.numel(), dst);
    }
    return out;
}

// ── per-window reducers ───────────────────────────────────────────────

double winMean(const double *w, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) s += w[i];
    return s / static_cast<double>(n);
}

double winSum(const double *w, size_t n)
{
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) s += w[i];
    return s;
}

double winMin(const double *w, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double m = w[0];
    for (size_t i = 1; i < n; ++i)
        if (w[i] < m) m = w[i];
    return m;
}

double winMax(const double *w, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double m = w[0];
    for (size_t i = 1; i < n; ++i)
        if (w[i] > m) m = w[i];
    return m;
}

double winProd(const double *w, size_t n)
{
    double p = 1.0;
    for (size_t i = 0; i < n; ++i) p *= w[i];
    return p;
}

// nth_element on a scratch copy (window already lives in caller's buf).
double winMedianInPlace(double *w, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    if (n == 1) return w[0];
    const size_t mid = n / 2;
    std::nth_element(w, w + mid, w + n);
    if (n % 2 == 1)
        return w[mid];
    const double upper = w[mid];
    double lower = w[0];
    for (size_t i = 1; i < mid; ++i)
        if (w[i] > lower) lower = w[i];
    return 0.5 * (upper + lower);
}

double winVar(const double *w, size_t n, int normFlag)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    if (n == 1) return (normFlag == 1) ? 0.0 : std::numeric_limits<double>::quiet_NaN();
    double s = 0.0;
    for (size_t i = 0; i < n; ++i) s += w[i];
    const double mean = s / static_cast<double>(n);
    double sq = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double d = w[i] - mean;
        sq += d * d;
    }
    const double denom = (normFlag == 1) ? static_cast<double>(n)
                                          : static_cast<double>(n - 1);
    return sq / denom;
}

// Median absolute deviation: median(|x - median(x)|).
double winMadInPlace(double *w, size_t n, ScratchArena &scratch)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    auto copy = ScratchVec<double>(n, &scratch);
    std::copy(w, w + n, copy.data());
    const double med = winMedianInPlace(copy.data(), n);
    auto devs = ScratchVec<double>(n, &scratch);
    for (size_t i = 0; i < n; ++i) devs[i] = std::abs(w[i] - med);
    return winMedianInPlace(devs.data(), n);
}

} // namespace

// ── public API ────────────────────────────────────────────────────────

Value movmean(std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim)
{
    const auto w = decodeWindow(k, "movmean");
    const int d = resolveDim(x, dim, "movmean");
    return movingDriverDim(mr, x, w, d,
        [](const double *win, size_t n) { return winMean(win, n); });
}

Value movsum(std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim)
{
    const auto w = decodeWindow(k, "movsum");
    const int d = resolveDim(x, dim, "movsum");
    return movingDriverDim(mr, x, w, d,
        [](const double *win, size_t n) { return winSum(win, n); });
}

Value movmin(std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim)
{
    const auto w = decodeWindow(k, "movmin");
    const int d = resolveDim(x, dim, "movmin");
    return movingDriverDim(mr, x, w, d,
        [](const double *win, size_t n) { return winMin(win, n); });
}

Value movmax(std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim)
{
    const auto w = decodeWindow(k, "movmax");
    const int d = resolveDim(x, dim, "movmax");
    return movingDriverDim(mr, x, w, d,
        [](const double *win, size_t n) { return winMax(win, n); });
}

Value movprod(std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim)
{
    const auto w = decodeWindow(k, "movprod");
    const int d = resolveDim(x, dim, "movprod");
    return movingDriverDim(mr, x, w, d,
        [](const double *win, size_t n) { return winProd(win, n); });
}

Value movmedian(std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim)
{
    const auto w = decodeWindow(k, "movmedian");
    const int d = resolveDim(x, dim, "movmedian");
    // The reducer needs to mutate its window — the runMoving copy buffer
    // is owned by the driver, but we wrote it as `const`. Make a local
    // mutable copy per call (the windows are short).
    return movingDriverDim(mr, x, w, d,
        [](const double *win, size_t n) {
            double tmp[1024];   // typical k <= a few hundred
            if (n <= sizeof(tmp) / sizeof(tmp[0])) {
                std::copy(win, win + n, tmp);
                return winMedianInPlace(tmp, n);
            }
            std::vector<double> heap(win, win + n);
            return winMedianInPlace(heap.data(), n);
        });
}

Value movvar(std::pmr::memory_resource *mr, const Value &x, const Value &k,
             int normFlag, int dim)
{
    if (normFlag != 0 && normFlag != 1)
        throw Error("movvar: normFlag must be 0 or 1",
                     0, 0, "movvar", "", "m:movvar:badNormFlag");
    const auto w = decodeWindow(k, "movvar");
    const int d = resolveDim(x, dim, "movvar");
    return movingDriverDim(mr, x, w, d,
        [normFlag](const double *win, size_t n) { return winVar(win, n, normFlag); });
}

Value movstd(std::pmr::memory_resource *mr, const Value &x, const Value &k,
             int normFlag, int dim)
{
    auto v = movvar(mr, x, k, normFlag, dim);
    double *p = v.doubleDataMut();
    const size_t n = v.numel();
    for (size_t i = 0; i < n; ++i)
        p[i] = std::sqrt(p[i]);
    return v;
}

Value movmad(std::pmr::memory_resource *mr, const Value &x, const Value &k, int dim)
{
    const auto w = decodeWindow(k, "movmad");
    const int d = resolveDim(x, dim, "movmad");
    return movingDriverDim(mr, x, w, d,
        [mr](const double *win, size_t n) -> double {
            // Independent per-window arena — small windows fit the inline
            // 64 KiB easily; cleared by ScratchArena dtor at function exit.
            ScratchArena local(mr);
            auto buf = ScratchVec<double>(n, &local);
            std::copy(win, win + n, buf.data());
            return winMadInPlace(buf.data(), n, local);
        });
}

// ── smoothdata ────────────────────────────────────────────────────────
Value smoothdata(std::pmr::memory_resource *mr, const Value &x,
                 const std::string &method, int k, int dim)
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
    auto kVal = Value::scalar(static_cast<double>(k), mr);

    if (m == "movmean" || m.empty())
        return movmean(mr, x, kVal, dim);
    if (m == "movmedian")
        return movmedian(mr, x, kVal, dim);
    if (m == "gaussian") {
        // Gaussian-weighted moving mean — use sigma = (k-1)/4 (MATLAB heuristic).
        const auto w = decodeWindow(kVal, "smoothdata");
        const int d = resolveDim(x, dim, "smoothdata");
        const double sigma = (k > 1) ? static_cast<double>(k - 1) / 4.0 : 1.0;
        const long kb = w.kb;
        return movingDriverDim(mr, x, w, d,
            [sigma, kb](const double *win, size_t n) {
                double sw = 0.0, ssum = 0.0;
                for (size_t i = 0; i < n; ++i) {
                    const double dx = static_cast<double>(static_cast<long>(i) - kb);
                    const double wt = std::exp(-0.5 * (dx / sigma) * (dx / sigma));
                    ssum += wt * win[i];
                    sw   += wt;
                }
                return (sw > 0) ? ssum / sw : std::numeric_limits<double>::quiet_NaN();
            });
    }
    throw Error("smoothdata: method '" + method + "' not supported "
                 "(supported: 'movmean', 'movmedian', 'gaussian')",
                 0, 0, "smoothdata", "", "m:smoothdata:unsupportedMethod");
}

// ── hampel ────────────────────────────────────────────────────────────
Value hampel(std::pmr::memory_resource *mr, const Value &x, int k, double nsigmas)
{
    if (k < 0)
        throw Error("hampel: k must be >= 0",
                     0, 0, "hampel", "", "m:hampel:badK");
    if (nsigmas <= 0)
        throw Error("hampel: nsigmas must be positive",
                     0, 0, "hampel", "", "m:hampel:badSigmas");
    if (!x.dims().isVector() && !x.isScalar())
        throw Error("hampel: vector input only (matrix form deferred)",
                     0, 0, "hampel", "", "m:hampel:notVector");

    constexpr double kMadToStd = 1.4826;
    auto out = createLike(x, ValueType::DOUBLE, mr);
    const size_t n = x.numel();
    const double *src = x.doubleData();
    double *dst = out.doubleDataMut();
    if (n == 0) return out;

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
        const double sigma = kMadToStd * mad;
        if (std::abs(src[i] - med) > nsigmas * sigma)
            dst[i] = med;
        else
            dst[i] = src[i];
    }
    return out;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

static int dimFromArg(Span<const Value> args, size_t pos)
{
    return (args.size() > pos) ? static_cast<int>(args[pos].toScalar()) : 0;
}

void movmean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmean: requires at least 2 arguments (x, k)",
                     0, 0, "movmean", "", "m:movmean:nargin");
    outs[0] = movmean(ctx.engine->resource(), args[0], args[1], dimFromArg(args, 2));
}

void movsum_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movsum: requires at least 2 arguments (x, k)",
                     0, 0, "movsum", "", "m:movsum:nargin");
    outs[0] = movsum(ctx.engine->resource(), args[0], args[1], dimFromArg(args, 2));
}

void movmin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmin: requires at least 2 arguments (x, k)",
                     0, 0, "movmin", "", "m:movmin:nargin");
    outs[0] = movmin(ctx.engine->resource(), args[0], args[1], dimFromArg(args, 2));
}

void movmax_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmax: requires at least 2 arguments (x, k)",
                     0, 0, "movmax", "", "m:movmax:nargin");
    outs[0] = movmax(ctx.engine->resource(), args[0], args[1], dimFromArg(args, 2));
}

void movprod_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movprod: requires at least 2 arguments (x, k)",
                     0, 0, "movprod", "", "m:movprod:nargin");
    outs[0] = movprod(ctx.engine->resource(), args[0], args[1], dimFromArg(args, 2));
}

void movmedian_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmedian: requires at least 2 arguments (x, k)",
                     0, 0, "movmedian", "", "m:movmedian:nargin");
    outs[0] = movmedian(ctx.engine->resource(), args[0], args[1], dimFromArg(args, 2));
}

void movvar_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movvar: requires at least 2 arguments (x, k)",
                     0, 0, "movvar", "", "m:movvar:nargin");
    const int normFlag = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    const int dim = dimFromArg(args, 3);
    outs[0] = movvar(ctx.engine->resource(), args[0], args[1], normFlag, dim);
}

void movstd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movstd: requires at least 2 arguments (x, k)",
                     0, 0, "movstd", "", "m:movstd:nargin");
    const int normFlag = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    const int dim = dimFromArg(args, 3);
    outs[0] = movstd(ctx.engine->resource(), args[0], args[1], normFlag, dim);
}

void movmad_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmad: requires at least 2 arguments (x, k)",
                     0, 0, "movmad", "", "m:movmad:nargin");
    outs[0] = movmad(ctx.engine->resource(), args[0], args[1], dimFromArg(args, 2));
}

void smoothdata_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("smoothdata: requires at least 1 argument",
                     0, 0, "smoothdata", "", "m:smoothdata:nargin");
    std::string method = "movmean";
    int k = 0;
    if (args.size() >= 2) {
        if (args[1].isChar() || args[1].isString())
            method = args[1].toString();
        else
            k = static_cast<int>(args[1].toScalar());
    }
    if (args.size() >= 3) {
        if (k == 0 && (args[2].isScalar() || args[2].numel() == 1))
            k = static_cast<int>(args[2].toScalar());
    }
    outs[0] = smoothdata(ctx.engine->resource(), args[0], method, k);
}

void hampel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hampel: requires at least 1 argument",
                     0, 0, "hampel", "", "m:hampel:nargin");
    const int k = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 3;
    const double nsigmas = (args.size() >= 3) ? args[2].toScalar() : 3.0;
    outs[0] = hampel(ctx.engine->resource(), args[0], k, nsigmas);
}

} // namespace detail

} // namespace numkit::stats
