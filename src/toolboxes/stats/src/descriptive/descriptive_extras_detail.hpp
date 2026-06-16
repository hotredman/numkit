// toolboxes/.../descriptive_extras_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by descriptive_extras.cpp + descriptive_extras_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include <numkit/ops/helpers.hpp>            // createForDims/createMatrix/DimsArg (engine-free)
#include <numkit/ops/reductions.hpp>  // numkit::builtin::detail dim-infra (engine-free, ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

using namespace ::numkit::ops;

// result structs (Value-based) + file-scope worker forward-decls (defs in
// descriptive_extras.cpp, external) — the reg adapters call these.
struct EcdfFull { Value f, x, flo, fup; };
struct KsdensityFull { Value f, xi, bw; };

Value corr_xx(const Value &X, std::pmr::memory_resource *mr);
Value partialcorr_xx(const Value &X, std::pmr::memory_resource *mr);
Value rmmissing_of(const Value &x, std::pmr::memory_resource *mr);
Value detrend_of(const Value &x, int order, std::pmr::memory_resource *mr);
Value range_of(const Value &x, int dim, std::pmr::memory_resource *mr);
Value geomean_of(const Value &x, int dim, bool omitnan, std::pmr::memory_resource *mr);
Value harmmean_of(const Value &x, int dim, bool omitnan, std::pmr::memory_resource *mr);
Value mad_of(const Value &x, int flag, int dim, std::pmr::memory_resource *mr);
Value moment_of(const Value &x, int order, int dim, std::pmr::memory_resource *mr);
Value standardizeMissing_of(const Value &x, double sentinel, std::pmr::memory_resource *mr);
Value fillmissing_of(const Value &x, const std::string &method, double constVal, std::pmr::memory_resource *mr);
Value partialcorr_of(const Value &X, const Value &Y, const Value &Z, std::pmr::memory_resource *mr);
Value partialcorr_xz(const Value &X, const Value &Z, std::pmr::memory_resource *mr);
Value detrendBP_of(const Value &x, const std::vector<double> &bpUser, std::pmr::memory_resource *mr);
Value filloutliers_of(const Value &x, const Value &fillArg, const std::string &detect, double thresholdFactor, double loP, double hiP, std::pmr::memory_resource *mr);
Value trimmean_of(const Value &x, double pct, int dim, bool useFloor, std::pmr::memory_resource *mr);
EcdfFull ecdf_full(const Value &y, const Value *freq, const std::string &function_mode, double alpha, bool want_bounds, std::pmr::memory_resource *mr);
KsdensityFull ksdensity_full(const Value &x, const Value &pts, double bw_user, const std::string &kernel_name, const std::string &function_mode, size_t numpoints, const Value *weights, std::pmr::memory_resource *mr);

namespace {

using ::numkit::ops::applyAlongDim;
using ::numkit::ops::firstNonSingletonDim;
using ::numkit::ops::validateDim;
using ::numkit::ops::applyAlongDimWithIndex;
using ::numkit::ops::resolveDim;   // 0 -> first non-singleton, else validate

double sliceMin(const double *s, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double m = s[0];
    for (size_t i = 1; i < n; ++i)
        if (s[i] < m || std::isnan(m)) m = s[i];
    return m;
}

double sliceMax(const double *s, size_t n)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    double m = s[0];
    for (size_t i = 1; i < n; ++i)
        if (s[i] > m || std::isnan(m)) m = s[i];
    return m;
}

// Linear-interpolation quantile of a slice (MATLAB default, type 7).
// MATLAB R2025b default ("midpoint" / R2007a / Type-5):
//   positions (k-0.5)/N for k=1..N → q = p*N + 0.5, clamped to [1, N].
double sliceQuantile(double *s, size_t n, double p)
{
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    std::sort(s, s + n);
    if (n == 1) return s[0];
    const double q = p * static_cast<double>(n) + 0.5;
    if (q <= 1.0) return s[0];
    if (q >= static_cast<double>(n)) return s[n - 1];
    const size_t lo = static_cast<size_t>(std::floor(q)) - 1;
    const double frac = q - std::floor(q);
    return s[lo] + frac * (s[lo + 1] - s[lo]);
}

} // namespace
namespace {

// k-largest along a generic dim. Output keeps the input shape but with
// the chosen dim shrunk to k. We allocate via createMatrix on the
// 2D / 3D fast path, and fall back to matrixND otherwise.
// When `idxOut` is non-null it receives the 1-based indices (along the
// operating dimension) of the returned elements, like MATLAB's
// [M, I] = mink/maxk(...). Ties keep the lower original index (stable sort).
Value topKAlongDim(const Value &x, int dim, int kReq, bool ascending, const char *fn, std::pmr::memory_resource *mr, Value *idxOut = nullptr, bool byAbs = false)
{
    if (kReq < 0)
        throw Error(std::string(fn) + ": k must be non-negative",
                     0, 0, fn, "", std::string("numkit:") + fn + ":badK");
    if (x.isEmpty()) {
        if (idxOut) *idxOut = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    }

    // Vector / scalar — single-slice fast path.
    if (x.dims().isVector() || x.isScalar()) {
        const size_t n = x.numel();
        const size_t k = std::min<size_t>(static_cast<size_t>(kReq), n);
        std::vector<double> vals(n);
        std::vector<size_t> ord(n);
        for (size_t i = 0; i < n; ++i) { vals[i] = x.elemAsDouble(i); ord[i] = i; }
        std::stable_sort(ord.begin(), ord.end(),
                  [&](size_t ia, size_t ib) {
                      const double a = byAbs ? std::fabs(vals[ia]) : vals[ia];
                      const double b = byAbs ? std::fabs(vals[ib]) : vals[ib];
                      if (std::isnan(a)) return false;
                      if (std::isnan(b)) return true;
                      return ascending ? (a < b) : (a > b);
                  });
        // Output orientation matches input row/col-vector orientation.
        const bool isRow = (x.dims().rows() == 1);
        auto out = isRow
                    ? Value::matrix(1, k, ValueType::DOUBLE, mr)
                    : Value::matrix(k, 1, ValueType::DOUBLE, mr);
        for (size_t i = 0; i < k; ++i) out.doubleDataMut()[i] = vals[ord[i]];
        if (idxOut) {
            *idxOut = isRow ? Value::matrix(1, k, ValueType::DOUBLE, mr)
                            : Value::matrix(k, 1, ValueType::DOUBLE, mr);
            for (size_t i = 0; i < k; ++i)
                idxOut->doubleDataMut()[i] = static_cast<double>(ord[i] + 1);
        }
        return out;
    }

    const int d = resolveDim(x, dim, fn);
    const auto &dd = x.dims();
    if (dd.ndim() >= 4)
        throw Error(std::string(fn) + ": ND (rank>=4) input not yet supported",
                     0, 0, fn, "", std::string("numkit:") + fn + ":nd");

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const size_t pageStride = R * C;
    size_t Ro = R, Co = C, Po = P;
    size_t sliceLen = 0;
    if (d == 1)      { sliceLen = R; Ro = std::min<size_t>(static_cast<size_t>(kReq), R); }
    else if (d == 2) { sliceLen = C; Co = std::min<size_t>(static_cast<size_t>(kReq), C); }
    else if (d == 3) { sliceLen = P; Po = std::min<size_t>(static_cast<size_t>(kReq), P); }
    else {
        // Out-of-rank → identity copy.
        return createLike(x, ValueType::DOUBLE, mr);
    }

    auto out = dd.is3D()
                ? Value::matrix3d(Ro, Co, Po, ValueType::DOUBLE, mr)
                : Value::matrix(Ro, Co, ValueType::DOUBLE, mr);
    if (idxOut)
        *idxOut = dd.is3D()
                    ? Value::matrix3d(Ro, Co, Po, ValueType::DOUBLE, mr)
                    : Value::matrix(Ro, Co, ValueType::DOUBLE, mr);
    const double *src = x.doubleData();
    double *dst = out.doubleDataMut();
    double *idst = idxOut ? idxOut->doubleDataMut() : nullptr;
    std::vector<double> buf(sliceLen);
    std::vector<size_t> ord(sliceLen);

    // Sort the index permutation of the current slice (stable → ties keep the
    // lower position), leaving the sorted order in `ord`.
    auto sortSlice = [&]() {
        for (size_t i = 0; i < sliceLen; ++i) ord[i] = i;
        std::stable_sort(ord.begin(), ord.end(),
            [&](size_t ia, size_t ib) {
                const double a = byAbs ? std::fabs(buf[ia]) : buf[ia];
                const double b = byAbs ? std::fabs(buf[ib]) : buf[ib];
                if (std::isnan(a)) return false;
                if (std::isnan(b)) return true;
                return ascending ? (a < b) : (a > b);
            });
    };

    if (d == 1) {
        const size_t outPageStride = Ro * Co;
        for (size_t p = 0; p < P; ++p)
            for (size_t c = 0; c < C; ++c) {
                for (size_t r = 0; r < R; ++r)
                    buf[r] = src[p * pageStride + c * R + r];
                sortSlice();
                for (size_t i = 0; i < Ro; ++i) {
                    dst[p * outPageStride + c * Ro + i] = buf[ord[i]];
                    if (idst) idst[p * outPageStride + c * Ro + i] =
                                  static_cast<double>(ord[i] + 1);
                }
            }
    } else if (d == 2) {
        const size_t outPageStride = Ro * Co;
        for (size_t p = 0; p < P; ++p)
            for (size_t r = 0; r < R; ++r) {
                for (size_t c = 0; c < C; ++c)
                    buf[c] = src[p * pageStride + c * R + r];
                sortSlice();
                for (size_t i = 0; i < Co; ++i) {
                    dst[p * outPageStride + i * Ro + r] = buf[ord[i]];
                    if (idst) idst[p * outPageStride + i * Ro + r] =
                                  static_cast<double>(ord[i] + 1);
                }
            }
    } else { // d == 3
        const size_t outPageStride = Ro * Co;
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                for (size_t p = 0; p < P; ++p)
                    buf[p] = src[p * pageStride + c * R + r];
                sortSlice();
                for (size_t i = 0; i < Po; ++i) {
                    dst[i * outPageStride + c * Ro + r] = buf[ord[i]];
                    if (idst) idst[i * outPageStride + c * Ro + r] =
                                  static_cast<double>(ord[i] + 1);
                }
            }
    }
    return out;
}

} // namespace
namespace {

// Residualise `wCol` (length m) on a column-major design matrix `C`
// of size m × pC (already includes intercept if needed). Returns the
// residual vector via `out`. Uses (C'C)\C'·w via Gauss elimination.
void residualiseColumn_local(const double *C, std::size_t m,
                              std::size_t pC, const double *MM,
                              const double *wCol, double *out,
                              std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<double> b(pC, 0.0, &scratch);
    for (std::size_t i = 0; i < pC; ++i)
        for (std::size_t k = 0; k < m; ++k)
            b[i] += C[k + i * m] * wCol[k];
    // Solve MM · coef = b. MM is column-major pC×pC.
    ScratchVec<double> Mc(MM, MM + pC * pC, &scratch);
    bool singular = false;
    for (std::size_t kc = 0; kc < pC && !singular; ++kc) {
        std::size_t piv = kc;
        double pmax = std::fabs(Mc[kc + kc * pC]);
        for (std::size_t r = kc + 1; r < pC; ++r) {
            const double v = std::fabs(Mc[r + kc * pC]);
            if (v > pmax) { pmax = v; piv = r; }
        }
        if (pmax == 0.0) { singular = true; break; }
        if (piv != kc) {
            for (std::size_t j = 0; j < pC; ++j)
                std::swap(Mc[kc + j * pC], Mc[piv + j * pC]);
            std::swap(b[kc], b[piv]);
        }
        const double pivVal = Mc[kc + kc * pC];
        for (std::size_t r = kc + 1; r < pC; ++r) {
            const double f = Mc[r + kc * pC] / pivVal;
            for (std::size_t j = kc; j < pC; ++j)
                Mc[r + j * pC] -= f * Mc[kc + j * pC];
            b[r] -= f * b[kc];
        }
    }
    if (singular) {
        std::fill(b.begin(), b.end(), 0.0);
    } else {
        for (std::size_t kk = pC; kk-- > 0;) {
            double s = b[kk];
            for (std::size_t j = kk + 1; j < pC; ++j)
                s -= Mc[kk + j * pC] * b[j];
            b[kk] = s / Mc[kk + kk * pC];
        }
    }
    for (std::size_t k = 0; k < m; ++k) {
        double pred = 0.0;
        for (std::size_t i = 0; i < pC; ++i)
            pred += C[k + i * m] * b[i];
        out[k] = wCol[k] - pred;
    }
}

double pearsonOnResiduals(const double *a, const double *b, std::size_t m)
{
    // Residuals from a regression with an intercept have mean ≈ 0, but
    // we still subtract the mean defensively for numerical safety.
    double ma = 0.0, mb = 0.0;
    for (std::size_t k = 0; k < m; ++k) { ma += a[k]; mb += b[k]; }
    ma /= static_cast<double>(m);
    mb /= static_cast<double>(m);
    double sab = 0.0, saa = 0.0, sbb = 0.0;
    for (std::size_t k = 0; k < m; ++k) {
        const double da = a[k] - ma;
        const double db = b[k] - mb;
        sab += da * db;
        saa += da * da;
        sbb += db * db;
    }
    if (saa <= 0.0 || sbb <= 0.0) return std::nan("");
    return sab / std::sqrt(saa * sbb);
}

} // namespace
namespace {

// Fit polynomial of order `order` to (xidx, y) via normal equations.
// Returns coefficients [a_order, a_{order-1}, ..., a_0] suitable for
// evalPoly Horner.
//
// PMR HARD RULE: scratch via the supplied scratch arena. The vector
// types are ScratchVec to preserve the per-call arena lifetime.
ScratchVec<double> fitPolyLS(const double *xidx, std::size_t n, const double *y, int order, std::pmr::memory_resource *scratch_mr)
{
    const std::size_t k = static_cast<std::size_t>(order) + 1;
    ScratchVec<double> M(k * k, 0.0, scratch_mr);
    ScratchVec<double> b(k, 0.0, scratch_mr);
    ScratchVec<double> powSums(2 * k - 1, 0.0, scratch_mr);
    for (std::size_t i = 0; i < n; ++i) {
        double xp = 1.0;
        for (std::size_t p = 0; p < powSums.size(); ++p) {
            powSums[p] += xp;
            xp *= xidx[i];
        }
    }
    for (std::size_t i = 0; i < k; ++i)
        for (std::size_t j = 0; j < k; ++j)
            M[i + j * k] = powSums[(order - static_cast<int>(i)) +
                                   (order - static_cast<int>(j))];
    for (std::size_t i = 0; i < n; ++i) {
        double xp = 1.0;
        for (int p = 0; p < static_cast<int>(k); ++p) {
            b[order - p] += y[i] * xp;
            xp *= xidx[i];
        }
    }
    // Gaussian elimination with partial pivot.
    for (std::size_t kc = 0; kc < k; ++kc) {
        std::size_t piv = kc;
        double pmax = std::fabs(M[kc + kc * k]);
        for (std::size_t r = kc + 1; r < k; ++r) {
            const double v = std::fabs(M[r + kc * k]);
            if (v > pmax) { pmax = v; piv = r; }
        }
        if (pmax == 0.0) {
            ScratchVec<double> nanVec(k, std::numeric_limits<double>::quiet_NaN(),
                                       scratch_mr);
            return nanVec;
        }
        if (piv != kc) {
            for (std::size_t j = 0; j < k; ++j)
                std::swap(M[kc + j * k], M[piv + j * k]);
            std::swap(b[kc], b[piv]);
        }
        const double pivVal = M[kc + kc * k];
        for (std::size_t r = kc + 1; r < k; ++r) {
            const double f = M[r + kc * k] / pivVal;
            for (std::size_t j = kc; j < k; ++j)
                M[r + j * k] -= f * M[kc + j * k];
            b[r] -= f * b[kc];
        }
    }
    ScratchVec<double> a(k, 0.0, scratch_mr);
    for (std::size_t kk = k; kk-- > 0;) {
        double s = b[kk];
        for (std::size_t j = kk + 1; j < k; ++j) s -= M[kk + j * k] * a[j];
        a[kk] = s / M[kk + kk * k];
    }
    return a;
}

double evalPoly(const ScratchVec<double> &a, double x)
{
    double r = 0.0;
    for (double c : a) r = r * x + c;
    return r;
}

void detrendColumn(const double *src, double *dst, std::size_t n, int order, std::pmr::memory_resource *scratch_mr)
{
    if (n == 0) return;
    if (order < 0) order = 1;
    ScratchVec<double> xidx(n, scratch_mr);
    for (std::size_t i = 0; i < n; ++i)
        xidx[i] = static_cast<double>(i);
    auto coefs = fitPolyLS(xidx.data(), n, src, order, scratch_mr);
    for (std::size_t i = 0; i < n; ++i)
        dst[i] = src[i] - evalPoly(coefs, xidx[i]);
}

// Solve the k×k linear system (row-major G) G·c = rhs by Gaussian
// elimination with partial pivoting; solution overwrites rhs. k is tiny
// (the number of detrend basis columns). Returns false if singular.
bool solveSmallDense(double *G, double *rhs, std::size_t k)
{
    for (std::size_t col = 0; col < k; ++col) {
        std::size_t piv = col;
        double best = std::fabs(G[col * k + col]);
        for (std::size_t r = col + 1; r < k; ++r) {
            const double v = std::fabs(G[r * k + col]);
            if (v > best) { best = v; piv = r; }
        }
        if (best == 0.0) return false;
        if (piv != col) {
            for (std::size_t j = 0; j < k; ++j)
                std::swap(G[col * k + j], G[piv * k + j]);
            std::swap(rhs[col], rhs[piv]);
        }
        const double d = G[col * k + col];
        for (std::size_t r = col + 1; r < k; ++r) {
            const double f = G[r * k + col] / d;
            if (f == 0.0) continue;
            for (std::size_t j = col; j < k; ++j)
                G[r * k + j] -= f * G[col * k + j];
            rhs[r] -= f * rhs[col];
        }
    }
    for (std::size_t i = k; i-- > 0;) {
        double s = rhs[i];
        for (std::size_t j = i + 1; j < k; ++j) s -= G[i * k + j] * rhs[j];
        rhs[i] = s / G[i * k + i];
    }
    return true;
}

// Detrend one length-N column against a prebuilt design matrix `a`
// (column-major, N×k): subtract the least-squares fit a·(a\src).
// Used by the piecewise-linear (breakpoint) detrend path.
void detrendColumnBP(const double *a, std::size_t N, std::size_t k,
                     const double *src, double *dst,
                     std::pmr::memory_resource *mr)
{
    ScratchArena sc(mr);
    ScratchVec<double> G(k * k, 0.0, &sc), rhs(k, 0.0, &sc);
    for (std::size_t i = 0; i < k; ++i) {
        for (std::size_t j = 0; j < k; ++j) {
            double s = 0.0;
            for (std::size_t r = 0; r < N; ++r) s += a[i * N + r] * a[j * N + r];
            G[i * k + j] = s;
        }
        double s = 0.0;
        for (std::size_t r = 0; r < N; ++r) s += a[i * N + r] * src[r];
        rhs[i] = s;
    }
    if (!solveSmallDense(G.data(), rhs.data(), k)) {
        for (std::size_t r = 0; r < N; ++r) dst[r] = src[r];
        return;
    }
    for (std::size_t r = 0; r < N; ++r) {
        double fit = 0.0;
        for (std::size_t i = 0; i < k; ++i) fit += a[i * N + r] * rhs[i];
        dst[r] = src[r] - fit;
    }
}

} // anonymous namespace
namespace {

// Forward-declare the per-column fillmissing kernel (defined later in
// this TU). We need it for the previous/next/nearest/linear fill paths.
void fill_one_column(double *p, std::size_t len, const std::string &method,
                     double constVal);

struct FoDetect {
    std::vector<uint8_t> mask;  // 1 where outlier
    double center, lo, hi;      // threshold center, L, U
};

FoDetect detect_one_column(const double *x, std::size_t n,
                           const std::string &method, double tf)
{
    FoDetect r;
    r.mask.assign(n, 0);
    r.center = std::numeric_limits<double>::quiet_NaN();
    r.lo = -std::numeric_limits<double>::infinity();
    r.hi =  std::numeric_limits<double>::infinity();
    if (n == 0) return r;

    if (method == "mean") {
        double s = 0.0, ss = 0.0;
        std::size_t k = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (std::isnan(x[i])) continue;
            s += x[i]; ss += x[i]*x[i]; ++k;
        }
        if (k < 2) return r;
        const double m = s / double(k);
        const double v = (ss - double(k) * m * m) / double(k - 1);
        const double sd = std::sqrt(std::max(0.0, v));
        const double t = tf * sd;
        r.center = m; r.lo = m - t; r.hi = m + t;
    } else if (method == "quartiles") {
        std::vector<double> buf;
        buf.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            if (!std::isnan(x[i])) buf.push_back(x[i]);
        if (buf.size() < 4) return r;
        std::sort(buf.begin(), buf.end());
        // MATLAB uses the linear-interpolation 'lower' definition for
        // quartiles by default (R-default '7' percentile rule).
        auto quant = [&](double p) {
            const double h = (double(buf.size()) - 1.0) * p;
            const std::size_t f = static_cast<std::size_t>(std::floor(h));
            const double frac = h - double(f);
            const std::size_t g = std::min(f + 1, buf.size() - 1);
            return buf[f] + frac * (buf[g] - buf[f]);
        };
        const double q1 = quant(0.25);
        const double q3 = quant(0.75);
        const double iqr = q3 - q1;
        r.center = 0.5 * (q1 + q3);
        r.lo = q1 - tf * 0.5 * iqr;   // tf = 3 → 1.5·IQR (MATLAB default tf=1.5)
        r.hi = q3 + tf * 0.5 * iqr;
    } else {  // "median" (default)
        std::vector<double> buf;
        buf.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            if (!std::isnan(x[i])) buf.push_back(x[i]);
        if (buf.empty()) return r;
        std::sort(buf.begin(), buf.end());
        auto medOf = [](std::vector<double> &v) {
            const std::size_t k = v.size();
            return (k % 2 == 1) ? v[k / 2]
                                 : 0.5 * (v[k / 2 - 1] + v[k / 2]);
        };
        const double med = medOf(buf);
        std::vector<double> dev;
        dev.reserve(buf.size());
        for (double v : buf) dev.push_back(std::fabs(v - med));
        std::sort(dev.begin(), dev.end());
        const double mad = medOf(dev);
        // MATLAB-exact normal-consistency constant: 1/norminv(0.75)
        // = -1/(sqrt(2)*erfcinv(3/2)) ≈ 1.4826022185056. Using the
        // looser 1.4826 misses MATLAB's 'clip' threshold by ~1e-5.
        constexpr double kMADc = 1.4826022185056;
        const double scaled = mad * kMADc;
        const double t = tf * scaled;
        r.center = med; r.lo = med - t; r.hi = med + t;
    }
    for (std::size_t i = 0; i < n; ++i)
        r.mask[i] = (std::isnan(x[i]) ? 0
                  : ((x[i] < r.lo || x[i] > r.hi) ? 1 : 0));
    return r;
}

// Moving-window detector for isoutlier 'movmedian' / 'movmean'. For each
// element a local window [i-hb, i+hf] (truncated at the column ends, NaNs
// excluded) yields a local center and scale:
//   * movmedian — center = local median, scale = 1.4826·(local MAD);
//   * movmean   — center = local mean,   scale = local sample std (n-1).
// The element is an outlier when |x_i - center| > tf * scale. NaNs are
// never flagged. Returns the per-column mask (1 == outlier).
std::vector<uint8_t> detect_moving_column(const double *x, std::size_t n,
                                          bool isMedian, long hb, long hf,
                                          double tf)
{
    std::vector<uint8_t> mask(n, 0);
    if (n == 0) return mask;
    auto medOf = [](std::vector<double> &v) {
        const std::size_t k = v.size();
        return (k % 2 == 1) ? v[k / 2] : 0.5 * (v[k / 2 - 1] + v[k / 2]);
    };
    std::vector<double> win;
    win.reserve(static_cast<std::size_t>(hb + hf + 1));
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(x[i])) continue;
        long lo = static_cast<long>(i) - hb; if (lo < 0) lo = 0;
        long hi = static_cast<long>(i) + hf;
        if (hi > static_cast<long>(n) - 1) hi = static_cast<long>(n) - 1;
        win.clear();
        for (long j = lo; j <= hi; ++j)
            if (!std::isnan(x[static_cast<std::size_t>(j)]))
                win.push_back(x[static_cast<std::size_t>(j)]);
        if (win.empty()) continue;
        double center, scale;
        if (isMedian) {
            std::vector<double> w = win;
            std::sort(w.begin(), w.end());
            center = medOf(w);
            std::vector<double> dev;
            dev.reserve(win.size());
            for (double v : win) dev.push_back(std::fabs(v - center));
            std::sort(dev.begin(), dev.end());
            scale = 1.4826022185056 * medOf(dev);
        } else {
            double s = 0.0;
            for (double v : win) s += v;
            const double m = s / double(win.size());
            double ss = 0.0;
            for (double v : win) ss += (v - m) * (v - m);
            center = m;
            scale = (win.size() >= 2)
                        ? std::sqrt(ss / double(win.size() - 1)) : 0.0;
        }
        mask[i] = (std::fabs(x[i] - center) > tf * scale) ? 1 : 0;
    }
    return mask;
}

// Percentiles detector (isoutlier/filloutliers/rmoutliers 'percentiles'
// method): elements below the loP-th or above the hiP-th percentile are
// outliers, using MATLAB's prctile convention (sorted positions at
// 100*(k-0.5)/n, clamped at the ends, linear interp). center = midpoint
// of the two percentile bounds (matches MATLAB's 'center' fill).
FoDetect detect_percentile_column(const double *x, std::size_t n,
                                  double loP, double hiP)
{
    FoDetect r;
    r.mask.assign(n, 0);
    r.center = std::numeric_limits<double>::quiet_NaN();
    r.lo = -std::numeric_limits<double>::infinity();
    r.hi =  std::numeric_limits<double>::infinity();
    if (n == 0) return r;
    std::vector<double> buf;
    buf.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isnan(x[i])) buf.push_back(x[i]);
    if (buf.empty()) return r;
    std::sort(buf.begin(), buf.end());
    auto prc = [&](double p) {
        const double q = p / 100.0 * double(buf.size()) - 0.5;
        if (q <= 0.0) return buf.front();
        if (q >= double(buf.size() - 1)) return buf.back();
        const std::size_t f = static_cast<std::size_t>(std::floor(q));
        const double fr = q - double(f);
        return buf[f] + fr * (buf[f + 1] - buf[f]);
    };
    r.lo = prc(loP);
    r.hi = prc(hiP);
    r.center = 0.5 * (r.lo + r.hi);
    for (std::size_t i = 0; i < n; ++i)
        r.mask[i] = (std::isnan(x[i]) ? 0
                  : ((x[i] < r.lo || x[i] > r.hi) ? 1 : 0));
    return r;
}

// Apply a per-column fill given the column data, outlier mask, and
// detection thresholds. p is overwritten in place.
void apply_fill(double *p, std::size_t n,
                const std::vector<uint8_t> &mask, double centerVal,
                double lo, double hi, const std::string &fill,
                double constVal, bool fill_is_constant)
{
    if (n == 0) return;
    if (fill_is_constant) {
        for (std::size_t i = 0; i < n; ++i)
            if (mask[i]) p[i] = constVal;
        return;
    }
    if (fill == "center") {
        for (std::size_t i = 0; i < n; ++i)
            if (mask[i]) p[i] = centerVal;
        return;
    }
    if (fill == "clip") {
        for (std::size_t i = 0; i < n; ++i)
            if (mask[i]) p[i] = (p[i] > hi ? hi : (p[i] < lo ? lo : p[i]));
        return;
    }
    // For previous/next/nearest/linear, treat outliers as NaN, run
    // the fillmissing per-column path, then restore non-outlier
    // values unchanged.
    std::vector<double> work(p, p + n);
    for (std::size_t i = 0; i < n; ++i) if (mask[i]) work[i] = std::numeric_limits<double>::quiet_NaN();
    fill_one_column(work.data(), n, fill, 0.0);  // constVal unused
    for (std::size_t i = 0; i < n; ++i)
        if (mask[i]) p[i] = work[i];
}

} // anonymous
namespace {

// Fill a single column of length `len` (stride 1 in column-major
// storage) according to the named method.
void fill_one_column(double *p, std::size_t len, const std::string &method,
                     double constVal)
{
    if (len == 0) return;
    auto is_nan = [&](std::size_t i) { return std::isnan(p[i]); };

    if (method == "constant") {
        for (std::size_t i = 0; i < len; ++i)
            if (is_nan(i)) p[i] = constVal;
        return;
    }
    if (method == "previous") {
        double last_good = std::numeric_limits<double>::quiet_NaN();
        bool have = false;
        for (std::size_t i = 0; i < len; ++i) {
            if (!is_nan(i)) { last_good = p[i]; have = true; }
            else if (have) p[i] = last_good;
        }
        return;
    }
    if (method == "next") {
        double next_good = std::numeric_limits<double>::quiet_NaN();
        bool have = false;
        for (std::size_t ii = len; ii-- > 0;) {
            if (!is_nan(ii)) { next_good = p[ii]; have = true; }
            else if (have) p[ii] = next_good;
        }
        return;
    }
    if (method == "nearest") {
        // For each NaN, fill with whichever of (previous-good,
        // next-good) is closer in index. Ties → NEXT wins
        // (matches MATLAB R2025b, probed).
        std::vector<std::size_t> good_idx;
        good_idx.reserve(len);
        for (std::size_t i = 0; i < len; ++i)
            if (!is_nan(i)) good_idx.push_back(i);
        if (good_idx.empty()) return;
        std::size_t k = 0;
        for (std::size_t i = 0; i < len; ++i) {
            if (!is_nan(i)) continue;
            while (k < good_idx.size() && good_idx[k] <= i) ++k;
            const bool has_next = (k < good_idx.size());
            const bool has_prev = (k > 0);
            if (!has_prev) { p[i] = p[good_idx[k]]; continue; }
            if (!has_next) { p[i] = p[good_idx[k - 1]]; continue; }
            const std::size_t pi = good_idx[k - 1];
            const std::size_t ni = good_idx[k];
            // Tie → next.
            p[i] = ((i - pi) < (ni - i)) ? p[pi] : p[ni];
        }
        return;
    }
    if (method == "linear") {
        // Internal NaN runs: linearly interpolate between the
        // flanking good values. Leading / trailing NaN runs: linearly
        // extrapolate using the slope of the closest interior good-
        // value pair. With < 2 good values, extrapolation slope is
        // undefined → leave leading/trailing NaNs in place (matches
        // MATLAB R2025b probed behaviour).
        std::vector<std::size_t> good_idx;
        good_idx.reserve(len);
        for (std::size_t i = 0; i < len; ++i)
            if (!is_nan(i)) good_idx.push_back(i);
        const std::size_t G = good_idx.size();
        if (G == 0) return;
        // Interior linear interp.
        for (std::size_t k = 0; k + 1 < G; ++k) {
            const std::size_t a = good_idx[k];
            const std::size_t b = good_idx[k + 1];
            if (b == a + 1) continue;
            const double va = p[a];
            const double vb = p[b];
            const double slope = (vb - va) / double(b - a);
            for (std::size_t j = a + 1; j < b; ++j)
                p[j] = va + slope * double(j - a);
        }
        if (G < 2) return;
        // Leading NaNs: extrapolate via slope of first two good values.
        if (good_idx.front() > 0) {
            const std::size_t a = good_idx[0];
            const std::size_t b = good_idx[1];
            const double va = p[a];
            const double slope = (p[b] - va) / double(b - a);
            for (std::size_t i = 0; i < a; ++i)
                p[i] = va - slope * double(a - i);
        }
        // Trailing NaNs: slope of last two good values.
        if (good_idx.back() + 1 < len) {
            const std::size_t a = good_idx[G - 2];
            const std::size_t b = good_idx[G - 1];
            const double vb = p[b];
            const double slope = (vb - p[a]) / double(b - a);
            for (std::size_t i = b + 1; i < len; ++i)
                p[i] = vb + slope * double(i - b);
        }
        return;
    }
    // Other methods handled by caller (mean/median use whole column).
}

// fillmissing 'EndValues' post-processing. The 'EndValues' option
// governs ONLY the *endpoint* missing entries — those before the first
// original non-missing value and those after the last. Interior missing
// runs are always filled by the method itself. The default 'extrap' is a
// no-op (the method already extrapolates / leaves the endpoints per its
// own nature, which matches MATLAB R2025b).
enum class FmEndMode { Extrap, None, Const, Nearest };

void apply_end_values_column(const double *orig, double *out, std::size_t len,
                             FmEndMode mode, double endVal)
{
    if (len == 0 || mode == FmEndMode::Extrap) return;
    std::size_t first = 0, last = 0;
    bool any = false;
    for (std::size_t i = 0; i < len; ++i) {
        if (!std::isnan(orig[i])) {
            if (!any) { first = i; any = true; }
            last = i;
        }
    }
    auto set_end = [&](std::size_t i) {
        switch (mode) {
        case FmEndMode::None:
            out[i] = std::numeric_limits<double>::quiet_NaN();
            break;
        case FmEndMode::Const:
            out[i] = endVal;
            break;
        case FmEndMode::Nearest:
            out[i] = any ? ((i < first) ? orig[first] : orig[last])
                         : std::numeric_limits<double>::quiet_NaN();
            break;
        default:
            break;
        }
    };
    if (!any) {                                   // all-missing column
        for (std::size_t i = 0; i < len; ++i) set_end(i);
        return;
    }
    for (std::size_t i = 0; i < first; ++i) set_end(i);        // leading
    for (std::size_t i = last + 1; i < len; ++i) set_end(i);   // trailing
}

} // anonymous
namespace {
// Per-kernel pdf K(u) and cdf F(u). All normalized so ∫K = 1 and the
// kernel is supported on [-1, 1] for finite-support kernels (or all of
// R for normal). Bandwidth is applied externally.
enum class KsKernel { Normal, Box, Triangle, Epanechnikov };
KsKernel parse_ks_kernel(const std::string &raw) {
    std::string s; s.reserve(raw.size());
    for (char c : raw) s.push_back((char)std::tolower((unsigned char)c));
    if (s == "normal" || s == "gauss" || s == "gaussian") return KsKernel::Normal;
    if (s == "box" || s == "rectangular" || s == "rect")   return KsKernel::Box;
    if (s == "triangle" || s == "triangular")              return KsKernel::Triangle;
    if (s == "epanechnikov" || s == "epan")                return KsKernel::Epanechnikov;
    throw Error("ksdensity: unknown Kernel '" + raw + "'",
                0, 0, "ksdensity", "", "numkit:ksdensity:kernel");
}
inline double ks_pdf(double u, KsKernel k) {
    switch (k) {
        case KsKernel::Normal:
            return 0.3989422804014327 * std::exp(-0.5 * u * u);
        case KsKernel::Box:
            return (std::fabs(u) <= 1.0) ? 0.5 : 0.0;
        case KsKernel::Triangle:
            return (std::fabs(u) <= 1.0) ? (1.0 - std::fabs(u)) : 0.0;
        case KsKernel::Epanechnikov:
            return (std::fabs(u) <= 1.0) ? (0.75 * (1.0 - u * u)) : 0.0;
    }
    return 0.0;
}
// MATLAB-compat scaling: each kernel's "unit" form has variance σ² which
// differs across kernel types. MATLAB normalizes the EFFECTIVE bandwidth
// so that h has the same standard-deviation interpretation as the
// normal kernel. Result: multiply h by 1/σ_unit for finite-support
// kernels.
//   Normal: σ²=1     → factor 1.0000
//   Box:    σ²=1/3   → factor sqrt(3) ≈ 1.7321
//   Tri:    σ²=1/6   → factor sqrt(6) ≈ 2.4495
//   Epan:   σ²=1/5   → factor sqrt(5) ≈ 2.2361
inline double ks_h_factor(KsKernel k) {
    switch (k) {
        case KsKernel::Normal:       return 1.0;
        case KsKernel::Box:          return std::sqrt(3.0);
        case KsKernel::Triangle:     return std::sqrt(6.0);
        case KsKernel::Epanechnikov: return std::sqrt(5.0);
    }
    return 1.0;
}
inline double ks_cdf(double u, KsKernel k) {
    switch (k) {
        case KsKernel::Normal:
            return 0.5 * (1.0 + std::erf(u / std::sqrt(2.0)));
        case KsKernel::Box:
            if (u <= -1.0) return 0.0;
            if (u >=  1.0) return 1.0;
            return 0.5 * (u + 1.0);
        case KsKernel::Triangle:
            if (u <= -1.0) return 0.0;
            if (u >=  1.0) return 1.0;
            if (u <= 0.0) return 0.5 * (u + 1.0) * (u + 1.0);
            return 1.0 - 0.5 * (1.0 - u) * (1.0 - u);
        case KsKernel::Epanechnikov:
            if (u <= -1.0) return 0.0;
            if (u >=  1.0) return 1.0;
            return 0.5 + 0.75 * u - 0.25 * u * u * u;
    }
    return 0.0;
}
} // anonymous
namespace {

inline bool finite_double(double v) {
    return !std::isnan(v) && !std::isinf(v);
}

// Flatten + filter helper. `srcs` give pointer/numel pairs (already
// linearised), `keep_mask[i]` indicates whether row i survives.
Value pack_filtered(const std::vector<double> &src, const std::vector<uint8_t> &keep, std::pmr::memory_resource *mr)
{
    size_t kept = 0;
    for (uint8_t k : keep) if (k) ++kept;
    Value out = Value::matrix(kept, 1, ValueType::DOUBLE, mr);
    if (kept == 0) return out;
    double *od = out.doubleDataMut();
    size_t j = 0;
    for (size_t i = 0; i < src.size(); ++i)
        if (keep[i]) od[j++] = src[i];
    return out;
}

} // anonymous

} // namespace numkit::stats
