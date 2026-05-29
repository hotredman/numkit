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

// Decode the `k` argument: scalar → MATLAB backward-leaning window
// [floor(k/2), floor((k-1)/2)] (even k centred on current+previous; odd k
// symmetric); 2-element vector → [kb, kf].
struct Window {
    long kb;   // samples back (inclusive of centre? no — i-kb is the start)
    long kf;   // samples forward
};

Window decodeWindow(Span<const size_t> k, const char *fn)
{
    auto badK = [&]() {
        throw Error(std::string(fn) + ": Window length must be a finite, "
                    "positive, real scalar or 2-element vector of finite, "
                    "nonnegative, real scalars.",
                    0, 0, fn, "", std::string("numkit:") + fn + ":badK");
    };
    if (k.size() == 1) {
        const size_t n = k[0];
        if (n == 0) badK();
        // MATLAB: a scalar window of even length is centred about the CURRENT
        // and PREVIOUS elements — it leans BACKWARD: kb = floor(k/2),
        // kf = floor((k-1)/2). e.g. movsum([1 2 3 4],2) = [1 3 5 7] (window
        // [i-1, i]). Odd k is symmetric (kb == kf). numkit previously leaned
        // forward (kb/kf swapped), diverging from MATLAB for even windows.
        return {static_cast<long>(n / 2), static_cast<long>((n - 1) / 2)};
    }
    if (k.size() == 2) {
        return {static_cast<long>(k[0]), static_cast<long>(k[1])};
    }
    badK();
    return {0, 0}; // unreachable
}

// Adapter / smoothdata path: extract k from a Value (scalar or 2-vec) into
// a ScratchVec<size_t>, with the same validation that decodeWindow used to
// do directly. Returns by value (live for the caller's scratch lifetime).
ScratchVec<size_t> decodeWindowValueToScratch(const Value &k, const char *fn,
                                               ScratchArena &scratch)
{
    auto badK = [&]() {
        throw Error(std::string(fn) + ": Window length must be a finite, "
                    "positive, real scalar or 2-element vector of finite, "
                    "nonnegative, real scalars.",
                    0, 0, fn, "", std::string("numkit:") + fn + ":badK");
    };
    ScratchVec<size_t> out(&scratch);
    if (k.isScalar()) {
        const double v = k.toScalar();
        if (!std::isfinite(v) || v <= 0 || std::floor(v) != v) badK();
        out.push_back(static_cast<size_t>(v));
        return out;
    }
    if (k.numel() == 2) {
        const double a = k.elemAsDouble(0);
        const double b = k.elemAsDouble(1);
        if (!std::isfinite(a) || !std::isfinite(b) ||
            a < 0 || b < 0 || std::floor(a) != a || std::floor(b) != b) badK();
        out.push_back(static_cast<size_t>(a));
        out.push_back(static_cast<size_t>(b));
        return out;
    }
    badK();
    return out; // unreachable
}

// ── Mov-extras options (nanflag + Endpoints + SamplePoints) ───────────
//
// Parses the trailing args of mov*(x, k, ...). Supports MATLAB R2025b:
//   * positional `dim` (numeric scalar, before any string)
//   * positional `nanflag` ∈ {"includemissing","includenan",
//                              "omitmissing","omitnan"}; default "includenan"
//   * Name-Value pairs: `Endpoints` ∈ {"shrink","discard","fill"} | scalar
//   * SamplePoints: NOT IMPLEMENTED (throws if requested) — see TODO.
enum class EndpointMode { Shrink, Discard, Fill, Scalar };

struct MovOpts {
    int dim = 0;
    bool omit_nan = false;       // default = includenan
    EndpointMode ep = EndpointMode::Shrink;
    double ep_fill = std::numeric_limits<double>::quiet_NaN();
};

inline std::string toLower(std::string s)
{
    for (auto &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

MovOpts parseMovExtras(Span<const Value> args, size_t start, const char *fn)
{
    MovOpts o;
    size_t i = start;

    // Optional positional `dim`: numeric scalar that's not a string and
    // appears before any string argument.
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty() && args[i].isScalar()) {
        o.dim = static_cast<int>(args[i].toScalar());
        ++i;
    }

    // Optional positional `nanflag`.
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string s = toLower(args[i].toString());
        if (s == "omitnan" || s == "omitmissing") {
            o.omit_nan = true; ++i;
        } else if (s == "includenan" || s == "includemissing") {
            o.omit_nan = false; ++i;
        }
        // else fall through — could be 'Endpoints' Name-Value pair.
    }

    // Name-Value pairs.
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error(std::string(fn) + ": expected a Name-Value pair",
                        0, 0, fn, "", std::string("numkit:") + fn + ":nv");
        const std::string name = toLower(args[i].toString());
        const Value &val = args[i + 1];
        if (name == "endpoints") {
            if (val.isChar() || val.isString()) {
                const std::string s = toLower(val.toString());
                if      (s == "shrink")  o.ep = EndpointMode::Shrink;
                else if (s == "discard") o.ep = EndpointMode::Discard;
                else if (s == "fill")    { o.ep = EndpointMode::Fill;
                                           o.ep_fill = std::numeric_limits<double>::quiet_NaN(); }
                else
                    throw Error(std::string(fn) + ": Endpoints must be "
                                "'shrink', 'discard', 'fill' or a scalar",
                                0, 0, fn, "", std::string("numkit:") + fn + ":ep");
            } else if (val.isScalar()) {
                o.ep = EndpointMode::Scalar;
                o.ep_fill = val.toScalar();
            } else {
                throw Error(std::string(fn) + ": Endpoints must be a string "
                            "or numeric scalar",
                            0, 0, fn, "", std::string("numkit:") + fn + ":ep");
            }
        } else if (name == "samplepoints") {
            throw Error(std::string(fn) + ": 'SamplePoints' is not yet "
                        "supported in numkit (parity gap; see audit findings)",
                        0, 0, fn, "", std::string("numkit:") + fn + ":samplePts");
        } else if (name == "datavariables" || name == "replacevalues") {
            throw Error(std::string(fn) + ": '" + name + "' is for table/"
                        "timetable inputs (numkit does not implement those)",
                        0, 0, fn, "", std::string("numkit:") + fn + ":tableOnly");
        } else {
            throw Error(std::string(fn) + ": unknown Name-Value '" + name + "'",
                        0, 0, fn, "", std::string("numkit:") + fn + ":nv");
        }
        i += 2;
    }
    if (i != args.size())
        throw Error(std::string(fn) + ": dangling argument at position " +
                    std::to_string(i + 1),
                    0, 0, fn, "", std::string("numkit:") + fn + ":nargin");
    return o;
}

// Allocate a same-shape DOUBLE output via createLike.
Value allocSameShape(const Value &x, std::pmr::memory_resource *mr)
{
    return createLike(x, ValueType::DOUBLE, mr);
}

// Apply per-window reducer F over a 1-D run [src, src+n) with stride
// `step` (in elements). Output written to dst with the same stride.
// F has signature `double(const double *win, size_t winLen)`.
//
// `ep` controls how out-of-range window positions are filled:
//   Shrink — drop missing positions, the window shrinks at edges
//   Fill   — pad missing positions with NaN (window keeps full length)
//   Scalar — pad missing positions with `ep_fill`
//   Discard — handled in driver, never reaches here
//
// `omit_nan == true` filters NaN out of the (possibly already padded)
// window before the reducer; an empty filtered window collapses to NaN.
template <typename F>
void runMoving(const double *src, size_t n, ptrdiff_t step,
               double *dst, const Window &w, ScratchArena &scratch,
               bool omit_nan, EndpointMode ep, double ep_fill, F &&fn)
{
    if (n == 0) return;
    const long N = static_cast<long>(n);
    const long fullLen = w.kb + w.kf + 1;
    auto buf = ScratchVec<double>(static_cast<size_t>(fullLen), &scratch);
    for (long i = 0; i < N; ++i) {
        size_t bufN = 0;
        for (long j = -w.kb; j <= w.kf; ++j) {
            const long pos = i + j;
            double v;
            if (pos >= 0 && pos < N) {
                v = src[pos * step];
            } else if (ep == EndpointMode::Fill) {
                v = std::numeric_limits<double>::quiet_NaN();
            } else if (ep == EndpointMode::Scalar) {
                v = ep_fill;
            } else {
                continue;   // Shrink — drop the missing position
            }
            if (omit_nan && std::isnan(v)) continue;
            buf[bufN++] = v;
        }
        dst[i * step] = (bufN == 0) ? std::numeric_limits<double>::quiet_NaN()
                                    : fn(buf.data(), bufN);
    }
}

// Driver with explicit dim + MovOpts. Handles vector / 2-D / 3-D.
//
// For Endpoints == Discard, output is shorter along the operating dim by
// (kb + kf) elements. For Shrink/Fill/Scalar the output keeps the same
// shape as input.
template <typename F>
Value movingDriverDim(const Value &x, const Window &w, int dim, const MovOpts &opt, F &&fn, std::pmr::memory_resource *mr)
{
    if (x.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (x.isScalar()) {
        auto out = Value::scalar(0.0, mr);
        const double v = x.toScalar();
        // Single-element window: 'Discard' would empty out — but a scalar
        // input is a degenerate case; mirror Shrink for consistency.
        out.doubleDataMut()[0] = (opt.omit_nan && std::isnan(v))
                                     ? std::numeric_limits<double>::quiet_NaN()
                                     : fn(&v, 1);
        return out;
    }
    ScratchArena scratch(mr);
    const double *src = x.doubleData();

    auto runDiscard = [&](const double *colSrc, size_t n, ptrdiff_t step,
                          double *colDst) {
        // Discard: emit only positions i where the window is fully in-bounds,
        // i.e. i in [kb, n-1-kf]. Output length = n - (kb+kf) (or 0).
        const long kb = w.kb, kf = w.kf;
        const long fullLen = kb + kf + 1;
        if (static_cast<long>(n) < fullLen) return;
        auto buf = ScratchVec<double>(static_cast<size_t>(fullLen), &scratch);
        long outI = 0;
        for (long i = kb; i + kf < static_cast<long>(n); ++i) {
            size_t bufN = 0;
            if (opt.omit_nan) {
                for (long j = -kb; j <= kf; ++j) {
                    const double v = colSrc[(i + j) * step];
                    if (!std::isnan(v)) buf[bufN++] = v;
                }
            } else {
                for (long j = -kb; j <= kf; ++j)
                    buf[static_cast<size_t>(j + kb)] = colSrc[(i + j) * step];
                bufN = static_cast<size_t>(fullLen);
            }
            colDst[outI * step] = (bufN == 0)
                                      ? std::numeric_limits<double>::quiet_NaN()
                                      : fn(buf.data(), bufN);
            ++outI;
        }
    };

    // Compute output dims for Discard mode.
    auto reducedLen = [&](size_t L) -> size_t {
        const long delta = w.kb + w.kf;
        if (static_cast<long>(L) <= delta) return 0;
        return L - static_cast<size_t>(delta);
    };

    if (x.dims().isVector()) {
        const size_t N = x.numel();
        size_t outN = N;
        if (opt.ep == EndpointMode::Discard) outN = reducedLen(N);
        const size_t r = (x.dims().rows() > 1) ? outN : 1;
        const size_t c = (x.dims().rows() > 1) ? 1    : outN;
        Value out = Value::matrix(r, c, ValueType::DOUBLE, mr);
        if (outN == 0) return out;
        double *dst = out.doubleDataMut();
        if (opt.ep == EndpointMode::Discard) {
            runDiscard(src, N, /*step=*/1, dst);
        } else {
            runMoving(src, N, /*step=*/1, dst, w, scratch,
                      opt.omit_nan, opt.ep, opt.ep_fill, fn);
        }
        return out;
    }

    const auto &d = x.dims();
    const size_t R = d.rows(), C = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1;
    const size_t pageStride = R * C;

    // Determine output dims.
    size_t outR = R, outC = C, outP = P;
    if (opt.ep == EndpointMode::Discard) {
        if      (dim == 1) outR = reducedLen(R);
        else if (dim == 2) outC = reducedLen(C);
        else if (dim == 3) outP = reducedLen(P);
    }
    Value out;
    if (d.is3D()) out = Value::matrix3d(outR, outC, outP, ValueType::DOUBLE, mr);
    else          out = Value::matrix(outR, outC, ValueType::DOUBLE, mr);
    if (out.numel() == 0) return out;
    double *dst = out.doubleDataMut();
    const size_t outPageStride = outR * outC;

    auto run = [&](const double *colSrc, size_t inLen, ptrdiff_t step,
                   double *colDst) {
        if (opt.ep == EndpointMode::Discard) runDiscard(colSrc, inLen, step, colDst);
        else                                 runMoving(colSrc, inLen, step, colDst, w, scratch,
                                                       opt.omit_nan, opt.ep, opt.ep_fill, fn);
    };

    if (dim == 1) {
        for (size_t p = 0; p < P; ++p)
            for (size_t c = 0; c < C; ++c) {
                const double *colSrc = src + p * pageStride + c * R;
                double       *colDst = dst + p * outPageStride + c * outR;
                run(colSrc, R, /*step=*/1, colDst);
            }
    } else if (dim == 2) {
        for (size_t p = 0; p < P; ++p)
            for (size_t r = 0; r < R; ++r) {
                const double *rowSrc = src + p * pageStride + r;
                double       *rowDst = dst + p * outPageStride + r;
                run(rowSrc, C, /*step=*/static_cast<ptrdiff_t>(R), rowDst);
            }
    } else if (dim == 3 && d.is3D()) {
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                const double *pgSrc = src + c * R + r;
                double       *pgDst = dst + c * outR + r;
                run(pgSrc, P, /*step=*/static_cast<ptrdiff_t>(pageStride), pgDst);
            }
    } else {
        // Trailing-singleton or out-of-rank: identity copy (legacy behaviour).
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

// ── internal *_impl with full MovOpts ─────────────────────────────────

namespace {

Value movmean_impl(const Value &x, Span<const size_t> k, const MovOpts &opt, std::pmr::memory_resource *mr)
{
    const auto w = decodeWindow(k, "movmean");
    const int d = resolveDim(x, opt.dim, "movmean");
    return movingDriverDim(x, w, d, opt, [](const double *win, size_t n) { return winMean(win, n); }, mr);
}

Value movsum_impl(const Value &x, Span<const size_t> k, const MovOpts &opt, std::pmr::memory_resource *mr)
{
    const auto w = decodeWindow(k, "movsum");
    const int d = resolveDim(x, opt.dim, "movsum");
    return movingDriverDim(x, w, d, opt, [](const double *win, size_t n) { return winSum(win, n); }, mr);
}

Value movmin_impl(const Value &x, Span<const size_t> k, const MovOpts &opt, std::pmr::memory_resource *mr)
{
    const auto w = decodeWindow(k, "movmin");
    const int d = resolveDim(x, opt.dim, "movmin");
    return movingDriverDim(x, w, d, opt, [](const double *win, size_t n) { return winMin(win, n); }, mr);
}

Value movmax_impl(const Value &x, Span<const size_t> k, const MovOpts &opt, std::pmr::memory_resource *mr)
{
    const auto w = decodeWindow(k, "movmax");
    const int d = resolveDim(x, opt.dim, "movmax");
    return movingDriverDim(x, w, d, opt, [](const double *win, size_t n) { return winMax(win, n); }, mr);
}

Value movprod_impl(const Value &x, Span<const size_t> k, const MovOpts &opt, std::pmr::memory_resource *mr)
{
    const auto w = decodeWindow(k, "movprod");
    const int d = resolveDim(x, opt.dim, "movprod");
    return movingDriverDim(x, w, d, opt, [](const double *win, size_t n) { return winProd(win, n); }, mr);
}

Value movmedian_impl(const Value &x, Span<const size_t> k, const MovOpts &opt, std::pmr::memory_resource *mr)
{
    const auto w = decodeWindow(k, "movmedian");
    const int d = resolveDim(x, opt.dim, "movmedian");
    return movingDriverDim(x, w, d, opt, [](const double *win, size_t n) {
            double tmp[1024];   // typical k <= a few hundred
            if (n <= sizeof(tmp) / sizeof(tmp[0])) {
                std::copy(win, win + n, tmp);
                return winMedianInPlace(tmp, n);
            }
            std::vector<double> heap(win, win + n);
            return winMedianInPlace(heap.data(), n);
        }, mr);
}

Value movvar_impl(const Value &x, Span<const size_t> k, int normFlag, const MovOpts &opt, std::pmr::memory_resource *mr)
{
    if (normFlag != 0 && normFlag != 1)
        throw Error("movvar: normFlag must be 0 or 1",
                     0, 0, "movvar", "", "numkit:movvar:badNormFlag");
    const auto w = decodeWindow(k, "movvar");
    const int d = resolveDim(x, opt.dim, "movvar");
    return movingDriverDim(x, w, d, opt, [normFlag](const double *win, size_t n) { return winVar(win, n, normFlag); }, mr);
}

Value movstd_impl(const Value &x, Span<const size_t> k, int normFlag, const MovOpts &opt, std::pmr::memory_resource *mr)
{
    auto v = movvar_impl(x, k, normFlag, opt, mr);
    double *p = v.doubleDataMut();
    const size_t n = v.numel();
    for (size_t i = 0; i < n; ++i)
        p[i] = std::sqrt(p[i]);
    return v;
}

Value movmad_impl(const Value &x, Span<const size_t> k, const MovOpts &opt, std::pmr::memory_resource *mr)
{
    const auto w = decodeWindow(k, "movmad");
    const int d = resolveDim(x, opt.dim, "movmad");
    return movingDriverDim(x, w, d, opt, [mr](const double *win, size_t n) -> double {
            ScratchArena local(mr);
            auto buf = ScratchVec<double>(n, &local);
            std::copy(win, win + n, buf.data());
            return winMadInPlace(buf.data(), n, local);
        }, mr);
}

} // anonymous

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
        // Gaussian-weighted moving mean — use sigma = (k-1)/4 (MATLAB heuristic).
        const auto w = decodeWindow(kSpan, "smoothdata");
        const int d = resolveDim(x, dim, "smoothdata");
        const double sigma = (k > 1) ? static_cast<double>(k - 1) / 4.0 : 1.0;
        const long kb = w.kb;
        return movingDriverDim(x, w, d, opt, [sigma, kb](const double *win, size_t n) {
                double sw = 0.0, ssum = 0.0;
                for (size_t i = 0; i < n; ++i) {
                    const double dx = static_cast<double>(static_cast<long>(i) - kb);
                    const double wt = std::exp(-0.5 * (dx / sigma) * (dx / sigma));
                    ssum += wt * win[i];
                    sw   += wt;
                }
                return (sw > 0) ? ssum / sw : std::numeric_limits<double>::quiet_NaN();
            }, mr);
    }
    throw Error("smoothdata: method '" + method + "' not supported "
                 "(supported: 'movmean', 'movmedian', 'gaussian')",
                 0, 0, "smoothdata", "", "numkit:smoothdata:unsupportedMethod");
}

// ── hampel ────────────────────────────────────────────────────────────
Value hampel(const Value &x, int k, double nsigmas, std::pmr::memory_resource *mr)
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
//
// All mov*_reg adapters share the same trailing-argument grammar:
//   mov*(x, k [, dim] [, nanflag] [, Name, Value]...)
// where nanflag is one of {includemissing|includenan|omitmissing|omitnan}
// and Name-Value pairs are {Endpoints} (SamplePoints currently throws).
//
// movvar / movstd insert one extra positional `normFlag` (numeric scalar
// 0 or 1) between `k` and the optional trailing dim/nanflag/Name-Value.

namespace detail {

void movmean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmean: requires at least 2 arguments (x, k)",
                     0, 0, "movmean", "", "numkit:movmean:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmean", scratch);
    auto opt = parseMovExtras(args, 2, "movmean");
    outs[0] = movmean_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movsum_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movsum: requires at least 2 arguments (x, k)",
                     0, 0, "movsum", "", "numkit:movsum:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movsum", scratch);
    auto opt = parseMovExtras(args, 2, "movsum");
    outs[0] = movsum_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movmin_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmin: requires at least 2 arguments (x, k)",
                     0, 0, "movmin", "", "numkit:movmin:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmin", scratch);
    auto opt = parseMovExtras(args, 2, "movmin");
    outs[0] = movmin_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movmax_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmax: requires at least 2 arguments (x, k)",
                     0, 0, "movmax", "", "numkit:movmax:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmax", scratch);
    auto opt = parseMovExtras(args, 2, "movmax");
    outs[0] = movmax_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movprod_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movprod: requires at least 2 arguments (x, k)",
                     0, 0, "movprod", "", "numkit:movprod:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movprod", scratch);
    auto opt = parseMovExtras(args, 2, "movprod");
    outs[0] = movprod_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movmedian_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmedian: requires at least 2 arguments (x, k)",
                     0, 0, "movmedian", "", "numkit:movmedian:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmedian", scratch);
    auto opt = parseMovExtras(args, 2, "movmedian");
    outs[0] = movmedian_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void movvar_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movvar: requires at least 2 arguments (x, k)",
                     0, 0, "movvar", "", "numkit:movvar:nargin");
    int normFlag = 0;
    size_t extras_start = 2;
    if (args.size() >= 3 && !args[2].isChar() && !args[2].isString()
        && args[2].isScalar()) {
        // Could be normFlag or dim — disambiguate: normFlag is 0 or 1.
        const double v = args[2].toScalar();
        if (v == 0.0 || v == 1.0) {
            normFlag = static_cast<int>(v);
            extras_start = 3;
        }
    }
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movvar", scratch);
    auto opt = parseMovExtras(args, extras_start, "movvar");
    outs[0] = movvar_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), normFlag, opt, mr);
}

void movstd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movstd: requires at least 2 arguments (x, k)",
                     0, 0, "movstd", "", "numkit:movstd:nargin");
    int normFlag = 0;
    size_t extras_start = 2;
    if (args.size() >= 3 && !args[2].isChar() && !args[2].isString()
        && args[2].isScalar()) {
        const double v = args[2].toScalar();
        if (v == 0.0 || v == 1.0) {
            normFlag = static_cast<int>(v);
            extras_start = 3;
        }
    }
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movstd", scratch);
    auto opt = parseMovExtras(args, extras_start, "movstd");
    outs[0] = movstd_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), normFlag, opt, mr);
}

void movmad_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("movmad: requires at least 2 arguments (x, k)",
                     0, 0, "movmad", "", "numkit:movmad:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto kBuf = decodeWindowValueToScratch(args[1], "movmad", scratch);
    auto opt = parseMovExtras(args, 2, "movmad");
    outs[0] = movmad_impl(args[0], Span<const size_t>(kBuf.data(), kBuf.size()), opt, mr);
}

void smoothdata_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("smoothdata: requires at least 1 argument",
                     0, 0, "smoothdata", "", "numkit:smoothdata:nargin");
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
    outs[0] = smoothdata(args[0], method, k, 0, ctx.engine->resource());
}

void hampel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hampel: requires at least 1 argument",
                     0, 0, "hampel", "", "numkit:hampel:nargin");
    const int k = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 3;
    const double nsigmas = (args.size() >= 3) ? args[2].toScalar() : 3.0;
    outs[0] = hampel(args[0], k, nsigmas, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
