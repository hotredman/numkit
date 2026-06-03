// libs/stats/src/descriptive/descriptive_extras.cpp
//
// Descriptive stats extras (B2): bounds, iqr, maxk, mink, rmse.

#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/stats/distributions/students_t.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"
#include "reduction_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

using ::numkit::builtin::detail::applyAlongDim;
using ::numkit::builtin::detail::firstNonSingletonDim;
using ::numkit::builtin::detail::validateDim;
using ::numkit::builtin::detail::applyAlongDimWithIndex;

int resolveDim(const Value &x, int dim, const char *fn)
{
    if (dim == 0) return firstNonSingletonDim(x);
    return validateDim(x, dim, fn);
}

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

// ── bounds ────────────────────────────────────────────────────────────
std::tuple<Value, Value>
bounds(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "bounds");
    auto lo = applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) { return sliceMin(s, n); }, mr);
    auto hi = applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) { return sliceMax(s, n); }, mr);
    return std::make_tuple(std::move(lo), std::move(hi));
}

// ── iqr ───────────────────────────────────────────────────────────────
Value iqr(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "iqr");
    return applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) -> double {
            // MATLAB iqr ignores NaN values.
            std::vector<double> buf;
            buf.reserve(n);
            for (size_t i = 0; i < n; ++i)
                if (!std::isnan(s[i])) buf.push_back(s[i]);
            const size_t k = buf.size();
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            std::vector<double> buf2 = buf;
            const double q3 = sliceQuantile(buf.data(), k, 0.75);
            const double q1 = sliceQuantile(buf2.data(), k, 0.25);
            return q3 - q1;
        }, mr);
}

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

// ── maxk / mink ───────────────────────────────────────────────────────
Value maxk(const Value &x, int k, int dim, std::pmr::memory_resource *mr)
{
    return topKAlongDim(x, dim, k, /*ascending=*/false, "maxk", mr);
}

Value mink(const Value &x, int k, int dim, std::pmr::memory_resource *mr)
{
    return topKAlongDim(x, dim, k, /*ascending=*/true, "mink", mr);
}

// ── rmse ──────────────────────────────────────────────────────────────
Value rmse(const Value &f, const Value &a, int dim, std::pmr::memory_resource *mr)
{
    if (f.dims() != a.dims() && !(f.isScalar() || a.isScalar()))
        throw Error("rmse: F and A must have compatible sizes",
                     0, 0, "rmse", "", "numkit:rmse:sizeMismatch");
    // Build the squared-difference array, then reduce.
    const size_t n = std::max(f.numel(), a.numel());
    auto diff = (f.numel() >= a.numel())
                  ? createLike(f, ValueType::DOUBLE, mr)
                  : createLike(a, ValueType::DOUBLE, mr);
    double *dst = diff.doubleDataMut();
    if (f.isScalar()) {
        const double fs = f.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double d = a.elemAsDouble(i) - fs;
            dst[i] = d * d;
        }
    } else if (a.isScalar()) {
        const double as = a.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double d = f.elemAsDouble(i) - as;
            dst[i] = d * d;
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            const double d = f.elemAsDouble(i) - a.elemAsDouble(i);
            dst[i] = d * d;
        }
    }
    const int dResolved = resolveDim(diff, dim, "rmse");
    auto v = applyAlongDim(diff, dResolved,
        [](size_t, const double *s, size_t k) -> double {
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            double sum = 0.0;
            for (size_t i = 0; i < k; ++i) sum += s[i];
            return std::sqrt(sum / static_cast<double>(k));
        }, mr);
    return v;
}

// ── mape ──────────────────────────────────────────────────────────────
Value mape(const Value &f, const Value &a, int dim, std::pmr::memory_resource *mr)
{
    if (f.dims() != a.dims() && !(f.isScalar() || a.isScalar()))
        throw Error("mape: F and A must have compatible sizes",
                     0, 0, "mape", "", "numkit:mape:sizeMismatch");
    const size_t n = std::max(f.numel(), a.numel());
    auto pct = (f.numel() >= a.numel())
                  ? createLike(f, ValueType::DOUBLE, mr)
                  : createLike(a, ValueType::DOUBLE, mr);
    double *dst = pct.doubleDataMut();

    if (f.isScalar()) {
        const double fs = f.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double ai = a.elemAsDouble(i);
            dst[i] = std::abs((ai - fs) / ai) * 100.0;
        }
    } else if (a.isScalar()) {
        const double as = a.toScalar();
        for (size_t i = 0; i < n; ++i) {
            const double fi = f.elemAsDouble(i);
            dst[i] = std::abs((as - fi) / as) * 100.0;
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            const double fi = f.elemAsDouble(i);
            const double ai = a.elemAsDouble(i);
            dst[i] = std::abs((ai - fi) / ai) * 100.0;
        }
    }

    const int dResolved = resolveDim(pct, dim, "mape");
    return applyAlongDim(pct, dResolved,
        [](size_t, const double *s, size_t k) -> double {
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            double sum = 0.0;
            for (size_t i = 0; i < k; ++i) sum += s[i];
            return sum / static_cast<double>(k);
        }, mr);
}

// ── partialcorr (regress-out + correlate) ───────────────────────────

Value partialcorr_of(const Value &X, const Value &Y, const Value &Z, std::pmr::memory_resource *mr)
{
    if (X.dims().ndim() != 2 || Y.dims().ndim() != 2 || Z.dims().ndim() != 2)
        throw Error("partialcorr: X, Y, Z must be 2D matrices",
                    0, 0, "partialcorr", "", "numkit:partialcorr:notMatrix");
    const std::size_t m = static_cast<std::size_t>(X.dims().dim(0));
    const std::size_t mY = static_cast<std::size_t>(Y.dims().dim(0));
    const std::size_t mZ = static_cast<std::size_t>(Z.dims().dim(0));
    if (mY != m || mZ != m)
        throw Error("partialcorr: X, Y, Z must have the same number of rows",
                    0, 0, "partialcorr", "", "numkit:partialcorr:dimMismatch");
    const std::size_t pX = static_cast<std::size_t>(X.dims().dim(1));
    const std::size_t pY = static_cast<std::size_t>(Y.dims().dim(1));
    const std::size_t pZ = static_cast<std::size_t>(Z.dims().dim(1));

    // Augment Z with intercept column (column of ones) for proper regression.
    // Z_full is m × (pZ + 1).
    ScratchArena scratch(mr);
    const std::size_t pZ1 = pZ + 1;
    ScratchVec<double> Zf(m * pZ1, &scratch);
    for (std::size_t i = 0; i < m; ++i) Zf[i + 0 * m] = 1.0;  // intercept
    const double *Zd = Z.doubleData();
    for (std::size_t j = 0; j < pZ; ++j)
        for (std::size_t i = 0; i < m; ++i)
            Zf[i + (j + 1) * m] = Zd[i + j * m];

    // Compute Zf' * Zf and its LU once.
    // M = Zf' * Zf  (pZ1 × pZ1).
    ScratchVec<double> M(pZ1 * pZ1, 0.0, &scratch);
    for (std::size_t i = 0; i < pZ1; ++i)
        for (std::size_t j = 0; j < pZ1; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < m; ++k)
                s += Zf[k + i * m] * Zf[k + j * m];
            M[i + j * pZ1] = s;
        }

    auto regressOut = [&](const Value &W) -> ScratchVec<double> {
        const std::size_t p = static_cast<std::size_t>(W.dims().dim(1));
        const double *Wd = W.doubleData();
        ScratchVec<double> Res(m * p, 0.0, &scratch);
        // For each column of W:
        for (std::size_t col = 0; col < p; ++col) {
            // b = Zf' * W(:, col)   (pZ1)
            ScratchVec<double> b(pZ1, 0.0, &scratch);
            for (std::size_t i = 0; i < pZ1; ++i)
                for (std::size_t k = 0; k < m; ++k)
                    b[i] += Zf[k + i * m] * Wd[k + col * m];
            // Solve M * coef = b via inline Gaussian elimination.
            ScratchVec<double> Mcopy(M.begin(), M.end(), &scratch);
            ScratchVec<double> coef(b.begin(), b.end(), &scratch);
            bool singular = false;
            for (std::size_t kc = 0; kc < pZ1 && !singular; ++kc) {
                // Pivot.
                std::size_t piv = kc;
                double pmax = std::fabs(Mcopy[kc + kc * pZ1]);
                for (std::size_t r = kc + 1; r < pZ1; ++r) {
                    const double v = std::fabs(Mcopy[r + kc * pZ1]);
                    if (v > pmax) { pmax = v; piv = r; }
                }
                if (pmax == 0.0) { singular = true; break; }
                if (piv != kc) {
                    for (std::size_t j = 0; j < pZ1; ++j)
                        std::swap(Mcopy[kc + j * pZ1], Mcopy[piv + j * pZ1]);
                    std::swap(coef[kc], coef[piv]);
                }
                const double pivVal = Mcopy[kc + kc * pZ1];
                for (std::size_t r = kc + 1; r < pZ1; ++r) {
                    const double f = Mcopy[r + kc * pZ1] / pivVal;
                    for (std::size_t j = kc; j < pZ1; ++j)
                        Mcopy[r + j * pZ1] -= f * Mcopy[kc + j * pZ1];
                    coef[r] -= f * coef[kc];
                }
            }
            if (singular) {
                std::fill(coef.begin(), coef.end(), 0.0);
            } else {
                // Back-substitute.
                for (std::size_t kk = pZ1; kk-- > 0;) {
                    double s = coef[kk];
                    for (std::size_t j = kk + 1; j < pZ1; ++j)
                        s -= Mcopy[kk + j * pZ1] * coef[j];
                    coef[kk] = s / Mcopy[kk + kk * pZ1];
                }
            }
            // Residual: W(:, col) - Zf * coef
            for (std::size_t i = 0; i < m; ++i) {
                double pred = 0.0;
                for (std::size_t j = 0; j < pZ1; ++j)
                    pred += Zf[i + j * m] * coef[j];
                Res[i + col * m] = Wd[i + col * m] - pred;
            }
        }
        return Res;
    };

    // Build residual matrices.
    auto Xres_data = regressOut(X);
    auto Yres_data = regressOut(Y);

    // Compute correlation matrix between Xres (m×pX) and Yres (m×pY).
    // R[i, j] = corr(Xres(:,i), Yres(:,j)) = cov / (std_x * std_y).
    auto Rout = Value::matrix(pX, pY, ValueType::DOUBLE, mr);
    double *R = Rout.doubleDataMut();

    auto colMean = [&](const ScratchVec<double> &v, std::size_t col) {
        double s = 0.0;
        for (std::size_t i = 0; i < m; ++i) s += v[i + col * m];
        return s / static_cast<double>(m);
    };
    auto colStd = [&](const ScratchVec<double> &v, std::size_t col, double mean) {
        double s = 0.0;
        for (std::size_t i = 0; i < m; ++i) {
            const double d = v[i + col * m] - mean;
            s += d * d;
        }
        return std::sqrt(s / static_cast<double>(m - 1));
    };

    for (std::size_t i = 0; i < pX; ++i) {
        const double mx = colMean(Xres_data, i);
        const double sx = colStd(Xres_data, i, mx);
        for (std::size_t j = 0; j < pY; ++j) {
            const double my = colMean(Yres_data, j);
            const double sy = colStd(Yres_data, j, my);
            double cov = 0.0;
            for (std::size_t k = 0; k < m; ++k)
                cov += (Xres_data[k + i * m] - mx) * (Yres_data[k + j * m] - my);
            cov /= static_cast<double>(m - 1);
            R[i + j * pX] = (sx > 0.0 && sy > 0.0) ? cov / (sx * sy) : std::nan("");
        }
    }
    return Rout;
}

// ── partialcorr 1-arg / 2-arg forms ─────────────────────────────────
//
// `partialcorr(X, Z)` is `partialcorr(X, X, Z)` with the diagonal forced
// to exactly 1 (FP cancellation drops self-correlation to 1 - O(ε), but
// MATLAB returns exactly 1).
//
// `partialcorr(X)` residualises on a per-pair-distinct control set:
// for pair (i, j), control = all X columns except i and j (plus
// intercept). Implementation matches the residualise-then-Pearson
// approach used by `partialcorr_of`, but rebuilds the design matrix
// per pair. For small p (typical use ≤ 20) this is cheap; cost is
// O(p² · (p³ + m·p²)).

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

Value partialcorr_xx(const Value &X, std::pmr::memory_resource *mr)
{
    if (X.dims().ndim() != 2)
        throw Error("partialcorr: X must be a 2D matrix",
                    0, 0, "partialcorr", "", "numkit:partialcorr:notMatrix");
    const std::size_t m = static_cast<std::size_t>(X.dims().dim(0));
    const std::size_t p = static_cast<std::size_t>(X.dims().dim(1));

    auto Rout = Value::matrix(p, p, ValueType::DOUBLE, mr);
    double *R = Rout.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i) R[i + i * p] = 1.0;
    if (p < 2 || m < 2) return Rout;

    const double *Xd = X.doubleData();
    ScratchArena scratch(mr);
    // Control matrix: intercept + (p - 2) "other" cols = (p - 1) cols.
    const std::size_t pC = (p >= 2) ? (p - 1) : 1;
    ScratchVec<double> C(m * pC, 0.0, &scratch);
    ScratchVec<double> MM(pC * pC, 0.0, &scratch);
    ScratchVec<double> ri(m, 0.0, &scratch);
    ScratchVec<double> rj(m, 0.0, &scratch);

    for (std::size_t i = 0; i < p; ++i) {
        for (std::size_t j = i + 1; j < p; ++j) {
            // Build C column-major: col 0 = intercept, then X cols ≠ i, j.
            std::fill(C.begin(), C.end(), 0.0);
            for (std::size_t k = 0; k < m; ++k) C[k + 0 * m] = 1.0;
            std::size_t cidx = 1;
            for (std::size_t k = 0; k < p; ++k) {
                if (k == i || k == j) continue;
                for (std::size_t r = 0; r < m; ++r)
                    C[r + cidx * m] = Xd[r + k * m];
                ++cidx;
            }
            // MM = C' · C  (column-major pC × pC).
            std::fill(MM.begin(), MM.end(), 0.0);
            for (std::size_t a = 0; a < pC; ++a)
                for (std::size_t b = 0; b < pC; ++b) {
                    double s = 0.0;
                    for (std::size_t k = 0; k < m; ++k)
                        s += C[k + a * m] * C[k + b * m];
                    MM[a + b * pC] = s;
                }
            residualiseColumn_local(C.data(), m, pC, MM.data(),
                                     Xd + i * m, ri.data(), mr);
            residualiseColumn_local(C.data(), m, pC, MM.data(),
                                     Xd + j * m, rj.data(), mr);
            const double rij = pearsonOnResiduals(ri.data(), rj.data(), m);
            R[i + j * p] = rij;
            R[j + i * p] = rij;
        }
    }
    return Rout;
}

Value partialcorr_xz(const Value &X, const Value &Z, std::pmr::memory_resource *mr)
{
    // Equivalent to partialcorr_of(X, X, Z) with diagonal forced to 1
    // (FP cancellation in self-correlation may drift to 1 - O(ε)).
    auto R = partialcorr_of(X, X, Z, mr);
    const std::size_t p = static_cast<std::size_t>(R.dims().dim(0));
    double *Rd = R.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i) Rd[i + i * p] = 1.0;
    // Symmetrise: off-diagonal pairs are mathematically equal but may
    // differ by O(ε) due to non-associative summation in cov.
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = i + 1; j < p; ++j) {
            const double m = 0.5 * (Rd[i + j * p] + Rd[j + i * p]);
            Rd[i + j * p] = m;
            Rd[j + i * p] = m;
        }
    return R;
}

// ── corr (Pearson alias) ─────────────────────────────────────────────

Value corr_xx(const Value &X, std::pmr::memory_resource *mr)
{
    return corrcoef(X, mr);
}

Value corr_xy(const Value &X, const Value &Y, std::pmr::memory_resource *mr)
{
    // Pairwise Pearson correlation: X (n×p), Y (n×q) → p×q matrix where
    // out(i,j) = corr(X(:,i), Y(:,j)). Matches MATLAB corr(X, Y).
    // Vector × vector returns a 1×1 matrix (scalar-convertible) —
    // differs from corrcoef(x,y) which returns the 2×2 [x;y] matrix.
    const std::size_t n = static_cast<std::size_t>(X.dims().dim(0));
    const std::size_t p = (X.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(X.dims().dim(1)) : 1;
    if (static_cast<std::size_t>(Y.dims().dim(0)) != n)
        throw Error("corr: X and Y must have the same number of rows",
                    0, 0, "corr", "", "numkit:corr:rows");
    const std::size_t q = (Y.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(Y.dims().dim(1)) : 1;

    auto out = Value::matrix(p, q, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    // Pre-compute means + centered-norms per column.
    std::vector<double> mx(p), my(q);
    std::vector<double> nrmX(p), nrmY(q);
    for (std::size_t i = 0; i < p; ++i) {
        double s = 0.0;
        for (std::size_t r = 0; r < n; ++r)
            s += X.elemAsDouble(r + i * n);
        mx[i] = s / double(n);
    }
    for (std::size_t j = 0; j < q; ++j) {
        double s = 0.0;
        for (std::size_t r = 0; r < n; ++r)
            s += Y.elemAsDouble(r + j * n);
        my[j] = s / double(n);
    }
    for (std::size_t i = 0; i < p; ++i) {
        double ss = 0.0;
        for (std::size_t r = 0; r < n; ++r) {
            const double d = X.elemAsDouble(r + i * n) - mx[i];
            ss += d * d;
        }
        nrmX[i] = std::sqrt(ss);
    }
    for (std::size_t j = 0; j < q; ++j) {
        double ss = 0.0;
        for (std::size_t r = 0; r < n; ++r) {
            const double d = Y.elemAsDouble(r + j * n) - my[j];
            ss += d * d;
        }
        nrmY[j] = std::sqrt(ss);
    }
    for (std::size_t j = 0; j < q; ++j) {
        for (std::size_t i = 0; i < p; ++i) {
            double s = 0.0;
            for (std::size_t r = 0; r < n; ++r) {
                const double dx = X.elemAsDouble(r + i * n) - mx[i];
                const double dy = Y.elemAsDouble(r + j * n) - my[j];
                s += dx * dy;
            }
            const double denom = nrmX[i] * nrmY[j];
            od[i + j * p] = (denom > 0.0) ? (s / denom)
                                          : std::numeric_limits<double>::quiet_NaN();
        }
    }
    return out;
}

// ── detrend (polynomial trend removal) ──────────────────────────────

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

Value detrend_of(const Value &x, int order, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    ScratchArena scratch(mr);
    auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
    const double *xd = x.doubleData();
    double *od = out.doubleDataMut();

    if (r == 1 || c == 1) {
        detrendColumn(xd, od, r * c, order, &scratch);
        return out;
    }
    for (std::size_t j = 0; j < c; ++j)
        detrendColumn(xd + j * r, od + j * r, r, order, &scratch);
    return out;
}

// Continuous piecewise-linear detrend with breakpoints — MATLAB
// detrend(x, 1, bp). Replicates the classic detrend.m design matrix:
// breakpoints become bp = unique([0; bp; N-1]) (0-based sample offsets);
// the design has one ramp column per segment (rows off..N-1 hold
// (1:M)/M, M = N-off) plus a constant column, and each data column is
// detrended by subtracting its least-squares fit a·(a\col). Operates
// along dim 1 (per column) like the ordinary detrend.
Value detrendBP_of(const Value &x, const std::vector<double> &bpUser,
                   std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    const std::size_t N = (r == 1 || c == 1) ? r * c : r;
    if (N < 2) {                       // nothing to fit
        auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
        const double *src0 = x.doubleData();
        double *dst0 = out.doubleDataMut();
        for (std::size_t i = 0; i < N; ++i) dst0[i] = src0[i];
        return out;
    }

    // Breakpoint offsets: unique([0; round(bp); N-1]) within (0, N-1).
    std::vector<double> bps;
    bps.push_back(0.0);
    for (double v : bpUser) {
        const double off = std::floor(v + 0.5);
        if (off > 0.0 && off < static_cast<double>(N - 1)) bps.push_back(off);
    }
    bps.push_back(static_cast<double>(N - 1));
    std::sort(bps.begin(), bps.end());
    bps.erase(std::unique(bps.begin(), bps.end()), bps.end());
    const std::size_t lb = bps.size() - 1;
    const std::size_t k = lb + 1;

    ScratchArena scratch(mr);
    ScratchVec<double> a(N * k, 0.0, &scratch);     // column-major N×k
    for (std::size_t row = 0; row < N; ++row) a[(k - 1) * N + row] = 1.0;
    for (std::size_t kb = 0; kb < lb; ++kb) {
        const std::size_t off = static_cast<std::size_t>(bps[kb]);
        const std::size_t M = N - off;
        for (std::size_t i = 0; i < M; ++i)
            a[kb * N + (off + i)] = static_cast<double>(i + 1) / static_cast<double>(M);
    }

    auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
    const double *xd = x.doubleData();
    double *od = out.doubleDataMut();
    if (r == 1 || c == 1) {
        detrendColumnBP(a.data(), N, k, xd, od, &scratch);
    } else {
        for (std::size_t j = 0; j < c; ++j)
            detrendColumnBP(a.data(), N, k, xd + j * r, od + j * r, &scratch);
    }
    return out;
}

// ── isoutlier / rmoutliers / fillmissing / rmmissing / standardizeMissing ──

// isoutlier(x) — boolean array marking outliers via median + MAD
// (default MATLAB method: more than 3 scaled MADs from median).
Value isoutlier_of(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.numel() == 0) return Value::matrix(0, 0, ValueType::LOGICAL, mr);
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    auto out = Value::matrix(r, c, ValueType::LOGICAL, mr);
    uint8_t *od = out.logicalDataMut();
    const double *xd = x.doubleData();
    const std::size_t n = x.numel();

    ScratchArena scratch(mr);
    ScratchVec<double> buf(xd, xd + n, &scratch);
    std::sort(buf.begin(), buf.end());
    const double med = (n % 2 == 1) ? buf[n / 2]
                                     : 0.5 * (buf[n / 2 - 1] + buf[n / 2]);
    ScratchVec<double> dev(n, &scratch);
    for (std::size_t i = 0; i < n; ++i) dev[i] = std::fabs(xd[i] - med);
    std::sort(dev.begin(), dev.end());
    const double mad = (n % 2 == 1) ? dev[n / 2]
                                     : 0.5 * (dev[n / 2 - 1] + dev[n / 2]);
    // MATLAB scales MAD by 1.4826 for normal-consistency.
    const double scaled_mad = mad * 1.4826;
    const double thresh = 3.0 * scaled_mad;

    for (std::size_t i = 0; i < n; ++i)
        od[i] = (std::fabs(xd[i] - med) > thresh) ? 1 : 0;
    return out;
}

// ── filloutliers — detect outliers then replace with fillmethod ─────
//
// Detection methods covered:
//   * "median" (default) — outliers > 3 * 1.4826 * MAD from median.
//   * "mean"             — outliers > 3 * std    from mean.
//   * "quartiles"        — outside [Q1 - 1.5·IQR, Q3 + 1.5·IQR].
//
// Fill methods covered (vector-wise, single-column path):
//   * numeric scalar     — constant fill.
//   * "center"           — center value used by detection (median /
//                          mean / 0.5*(Q1+Q3)).
//   * "clip"             — clamp to [L, U] threshold.
//   * "previous"         — last non-outlier value (NaN if leading).
//   * "next"             — first non-outlier value (NaN if trailing).
//   * "nearest"          — closer of prev/next (ties → NEXT, MATLAB).
//   * "linear"           — linear interpolation between flanking
//                          non-outliers; extrapolates ends from the
//                          slope of the nearest interior pair.
//
// Deferred (require extra infrastructure or rare): "spline", "pchip",
// "makima", "movmedian"/"movmean" detection, "grubbs", "gesd",
// SamplePoints / OutlierLocations / MaxNumOutliers / ReplaceValues.
// ThresholdFactor is honoured for "median" and "mean" methods.
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

Value filloutliers_of(const Value &x,
                      const Value &fillArg,       // string OR numeric scalar
                      const std::string &detect,
                      double thresholdFactor,
                      double loP, double hiP,     // used iff detect=="percentiles"
                      std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const double *xd = x.doubleData();
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    std::copy(xd, xd + n, od);

    // Parse fill arg: scalar numeric → constant fill; string → method.
    bool fill_is_constant = false;
    double constVal = 0.0;
    std::string fillMethod;
    if (fillArg.isChar() || fillArg.isString()) {
        fillMethod = fillArg.toString();
        if (fillMethod != "center" && fillMethod != "clip" &&
            fillMethod != "previous" && fillMethod != "next" &&
            fillMethod != "nearest" && fillMethod != "linear")
            throw Error("filloutliers: fillmethod must be a scalar, "
                        "'center', 'clip', 'previous', 'next', "
                        "'nearest', or 'linear'",
                        0, 0, "filloutliers", "",
                        "numkit:filloutliers:fillmethod");
    } else {
        fill_is_constant = true;
        constVal = fillArg.toScalar();
    }

    auto run_col = [&](double *p, std::size_t len) {
        FoDetect d = (detect == "percentiles")
                       ? detect_percentile_column(p, len, loP, hiP)
                       : detect_one_column(p, len, detect, thresholdFactor);
        apply_fill(p, len, d.mask, d.center, d.lo, d.hi,
                   fillMethod, constVal, fill_is_constant);
    };
    if (r == 1) {
        run_col(od, c);
    } else {
        for (std::size_t col = 0; col < c; ++col)
            run_col(od + col * r, r);
    }
    return out;
}

// rmoutliers(x) — drop elements flagged by isoutlier; vector form.
Value rmoutliers_of(const Value &x, std::pmr::memory_resource *mr)
{
    auto mask = isoutlier_of(x, mr);
    const std::size_t n = x.numel();
    const double *xd = x.doubleData();
    const uint8_t *m = mask.logicalData();
    ScratchArena scratch(mr);
    ScratchVec<double> kept(&scratch);
    kept.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (!m[i]) kept.push_back(xd[i]);
    const bool col = x.dims().ndim() >= 2 && x.dims().dim(1) == 1;
    auto out = col
        ? Value::matrix(kept.size(), 1, ValueType::DOUBLE, mr)
        : Value::matrix(1, kept.size(), ValueType::DOUBLE, mr);
    if (!kept.empty())
        std::copy(kept.begin(), kept.end(), out.doubleDataMut());
    return out;
}

// fillmissing(x, method[, constant_value]) — replace NaN with method.
// MATLAB-canonical methods: 'constant' (needs value), 'previous',
// 'next', 'nearest', 'linear'. Internal 'mean'/'median' kept as a
// numkit convenience (undocumented; use mean(x,'omitnan') +
// 'constant' for portability). Per-column processing matches
// MATLAB's default (each column filled independently). For vectors
// (rows or cols) the per-column scan reduces to a flat scan.
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

Value fillmissing_of(const Value &x, const std::string &method, double constVal, std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (n == 0) return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const double *xd = x.doubleData();
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    std::copy(xd, xd + n, od);

    // mean / median use the whole column, not the column slice — kept
    // as the (undocumented) numkit convenience.
    if (method == "mean" || method == "median") {
        ScratchArena scratch(mr);
        ScratchVec<double> good(&scratch);
        good.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            if (!std::isnan(xd[i])) good.push_back(xd[i]);
        if (good.empty()) return out;
        double fill;
        if (method == "mean") {
            double s = 0.0;
            for (double v : good) s += v;
            fill = s / static_cast<double>(good.size());
        } else {
            std::sort(good.begin(), good.end());
            const std::size_t gn = good.size();
            fill = (gn % 2 == 1) ? good[gn / 2]
                                  : 0.5 * (good[gn / 2 - 1] + good[gn / 2]);
        }
        for (std::size_t i = 0; i < n; ++i)
            if (std::isnan(od[i])) od[i] = fill;
        return out;
    }

    // constant / previous / next / nearest / linear → per-column.
    if (method == "constant" || method == "previous" || method == "next" ||
        method == "nearest"  || method == "linear")
    {
        // For a row vector treat as one column of length n.
        if (r == 1) {
            fill_one_column(od, c, method, constVal);
        } else {
            for (std::size_t col = 0; col < c; ++col)
                fill_one_column(od + col * r, r, method, constVal);
        }
        return out;
    }

    throw Error("fillmissing: method must be 'constant', 'previous', "
                "'next', 'nearest', 'linear', 'mean', or 'median' in "
                "this revision (MATLAB also supports 'spline', "
                "'pchip', 'makima', 'movmean', 'movmedian', 'knn' -- "
                "those are deferred)",
                0, 0, "fillmissing", "", "numkit:fillmissing:method");
}

// rmmissing(x) — drop NaN entries.
Value rmmissing_of(const Value &x, std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    const double *xd = x.doubleData();
    ScratchArena scratch(mr);
    ScratchVec<double> kept(&scratch);
    kept.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isnan(xd[i])) kept.push_back(xd[i]);
    const bool col = x.dims().ndim() >= 2 && x.dims().dim(1) == 1;
    auto out = col
        ? Value::matrix(kept.size(), 1, ValueType::DOUBLE, mr)
        : Value::matrix(1, kept.size(), ValueType::DOUBLE, mr);
    if (!kept.empty())
        std::copy(kept.begin(), kept.end(), out.doubleDataMut());
    return out;
}

// standardizeMissing(x, sentinel) — replace sentinel with NaN.
Value standardizeMissing_of(const Value &x, double sentinel, std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
    if (n == 0) return out;
    const double *xd = x.doubleData();
    double *od = out.doubleDataMut();
    const double nanv = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t i = 0; i < n; ++i)
        od[i] = (xd[i] == sentinel) ? nanv : xd[i];
    return out;
}

// ── range / mad / geomean / harmmean / moment / trimmean ─────────────

// range(x) = max(x) - min(x) along dim.
Value range_of(const Value &x, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "range");
    return applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            double lo = s[0], hi = s[0];
            for (size_t i = 1; i < n; ++i) {
                if (s[i] < lo) lo = s[i];
                if (s[i] > hi) hi = s[i];
            }
            return hi - lo;
        }, mr);
}

// mad(x) -- mean absolute deviation: mean(abs(x - mean(x))).
// mad(x, 1) -- median absolute deviation: median(abs(x - median(x))).
Value mad_of(const Value &x, int flag, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "mad");
    if (flag == 0) {
        // Mean form. MATLAB mad ignores NaN values.
        return applyAlongDim(x, d,
            [](size_t, const double *s, size_t n) -> double {
                std::vector<double> v;
                v.reserve(n);
                for (size_t i = 0; i < n; ++i)
                    if (!std::isnan(s[i])) v.push_back(s[i]);
                const size_t k = v.size();
                if (k == 0) return std::numeric_limits<double>::quiet_NaN();
                double mean = 0.0;
                for (size_t i = 0; i < k; ++i) mean += v[i];
                mean /= static_cast<double>(k);
                double sum = 0.0;
                for (size_t i = 0; i < k; ++i) sum += std::fabs(v[i] - mean);
                return sum / static_cast<double>(k);
            }, mr);
    }
    // Median form. MATLAB mad ignores NaN values.
    return applyAlongDim(x, d,
        [](size_t, const double *s, size_t n) -> double {
            std::vector<double> buf;
            buf.reserve(n);
            for (size_t i = 0; i < n; ++i)
                if (!std::isnan(s[i])) buf.push_back(s[i]);
            const size_t k = buf.size();
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            const double med = sliceQuantile(buf.data(), k, 0.5);
            std::vector<double> dev(k);
            for (size_t i = 0; i < k; ++i) dev[i] = std::fabs(buf[i] - med);
            return sliceQuantile(dev.data(), k, 0.5);
        }, mr);
}

// geomean(x) = (prod(x))^(1/n) = exp(mean(log(x))). x must be >= 0.
Value geomean_of(const Value &x, int dim, bool omitnan, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "geomean");
    return applyAlongDim(x, d,
        [omitnan](size_t, const double *s, size_t n) -> double {
            double sum = 0.0;
            size_t k = 0;
            for (size_t i = 0; i < n; ++i) {
                if (omitnan && std::isnan(s[i])) continue;  // 'omitnan'
                if (s[i] < 0.0)
                    return std::numeric_limits<double>::quiet_NaN();
                if (s[i] == 0.0) return 0.0;
                sum += std::log(s[i]);
                ++k;
            }
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            return std::exp(sum / static_cast<double>(k));
        }, mr);
}

// harmmean(x) = n / sum(1./x). x must be > 0.
Value harmmean_of(const Value &x, int dim, bool omitnan, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "harmmean");
    return applyAlongDim(x, d,
        [omitnan](size_t, const double *s, size_t n) -> double {
            double sum = 0.0;
            size_t k = 0;
            for (size_t i = 0; i < n; ++i) {
                if (omitnan && std::isnan(s[i])) continue;  // 'omitnan'
                if (s[i] <= 0.0)
                    return std::numeric_limits<double>::quiet_NaN();
                sum += 1.0 / s[i];
                ++k;
            }
            if (k == 0) return std::numeric_limits<double>::quiet_NaN();
            return static_cast<double>(k) / sum;
        }, mr);
}

// moment(x, k) = mean((x - mean(x))^k). k >= 2.
Value moment_of(const Value &x, int order, int dim, std::pmr::memory_resource *mr)
{
    const int d = resolveDim(x, dim, "moment");
    const int k = order;
    return applyAlongDim(x, d,
        [k](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            if (k < 0) return std::numeric_limits<double>::quiet_NaN();
            if (k == 0) return 1.0;          // m_0 = 1
            if (k == 1) return 0.0;          // central first moment
            double mean = 0.0;
            for (size_t i = 0; i < n; ++i) mean += s[i];
            mean /= static_cast<double>(n);
            double sum = 0.0;
            for (size_t i = 0; i < n; ++i) {
                const double d2 = s[i] - mean;
                sum += std::pow(d2, k);
            }
            return sum / static_cast<double>(n);
        }, mr);
}

// trimmean(x, p) = mean of x after trimming p/2% from each end (p in [0, 100]).
// `useFloor` selects MATLAB's flag: false ('round', the default) rounds the
// per-end count n*p/200 to the nearest integer with ties going DOWN (so
// k = ceil(n*p/200 - 0.5)); true ('floor') takes the plain floor.
Value trimmean_of(const Value &x, double pct, int dim, bool useFloor,
                  std::pmr::memory_resource *mr)
{
    if (pct < 0.0 || pct >= 100.0)
        throw Error("trimmean: percent must be in [0, 100)",
                    0, 0, "trimmean", "", "numkit:trimmean:badPct");
    const int d = resolveDim(x, dim, "trimmean");
    const double p = pct;
    return applyAlongDim(x, d,
        [p, useFloor](size_t, const double *s, size_t n) -> double {
            if (n == 0) return std::numeric_limits<double>::quiet_NaN();
            // Number of values to trim from EACH end.
            const double kf = static_cast<double>(n) * p / 200.0;
            const size_t k = useFloor
                ? static_cast<size_t>(std::floor(kf))
                : static_cast<size_t>(std::max(0.0, std::ceil(kf - 0.5)));
            if (2 * k >= n) return std::numeric_limits<double>::quiet_NaN();
            std::vector<double> buf(s, s + n);
            std::sort(buf.begin(), buf.end());
            double sum = 0.0;
            for (size_t i = k; i < n - k; ++i) sum += buf[i];
            return sum / static_cast<double>(n - 2 * k);
        }, mr);
}

// ── ksdensity ─────────────────────────────────────────────────────────

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

// Result struct for the extended ksdensity API. Forward-declared also
// above the engine-adapter `ksdensity_reg` (in the outer namespace).
struct KsdensityFull { Value f, xi, bw; };

KsdensityFull
ksdensity_full(const Value &x, const Value &pts, double bw_user, const std::string &kernel_name, const std::string &function_mode, size_t numpoints, const Value *weights, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0)
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::scalar(nan, mr)};

    const KsKernel kernel = parse_ks_kernel(kernel_name);

    std::vector<double> xv(N);
    std::vector<double> wv(N, 1.0);
    if (weights && weights->numel() == N)
        for (size_t i = 0; i < N; ++i) wv[i] = weights->elemAsDouble(i);
    for (size_t i = 0; i < N; ++i) xv[i] = x.elemAsDouble(i);
    // Normalize weights so Σw = 1 (matches MATLAB semantics).
    double Wsum = 0.0;
    for (double w : wv) Wsum += w;
    if (!(Wsum > 0.0)) Wsum = double(N);
    const double Winv = 1.0 / Wsum;

    // Sort xv and wv jointly.
    std::vector<size_t> idx(N);
    for (size_t i = 0; i < N; ++i) idx[i] = i;
    std::sort(idx.begin(), idx.end(),
              [&](size_t a, size_t b) { return xv[a] < xv[b]; });
    std::vector<double> xs(N), ws(N);
    for (size_t i = 0; i < N; ++i) { xs[i] = xv[idx[i]]; ws[i] = wv[idx[i]]; }

    // MATLAB's default bandwidth for the normal kernel:
    //   sigma = mad(x, 1) / 0.6745 if positive, else iqr(x) / 1.349
    //   bw    = sigma · (4 / (3·n))^(1/5)
    double bw = bw_user;
    if (!(bw > 0.0)) {
        // median absolute deviation
        std::vector<double> tmp = xs;          // already sorted
        const double med = (N % 2 == 1) ? tmp[N / 2]
                                        : 0.5 * (tmp[N / 2 - 1] + tmp[N / 2]);
        std::vector<double> dev(N);
        for (size_t i = 0; i < N; ++i) dev[i] = std::fabs(xs[i] - med);
        std::sort(dev.begin(), dev.end());
        const double mad = (N % 2 == 1) ? dev[N / 2]
                                        : 0.5 * (dev[N / 2 - 1] + dev[N / 2]);
        double sigma = (mad > 0.0) ? mad / 0.6745 : 0.0;
        if (!(sigma > 0.0)) {
            // IQR fallback.
            auto pct = [&](double p) {
                const double pos = p * (double(N) - 1.0);
                const size_t lo = (size_t)std::floor(pos);
                const size_t hi = (size_t)std::ceil(pos);
                const double t = pos - double(lo);
                return xs[lo] * (1.0 - t) + xs[hi] * t;
            };
            const double iqr = pct(0.75) - pct(0.25);
            sigma = (iqr > 0.0) ? iqr / 1.349 : 1.0;
        }
        bw = sigma * std::pow(4.0 / (3.0 * double(N)), 0.2);
        if (!(bw > 0.0)) bw = 1.0;
    }

    // Apply kernel-specific bandwidth scaling so h has consistent
    // standard-deviation semantics across kernels (MATLAB convention).
    const double h_eff = bw * ks_h_factor(kernel);

    // Build evaluation grid.
    std::vector<double> grid;
    if (pts.isEmpty()) {
        const size_t M = (numpoints > 0) ? numpoints : 100;
        const double xmin = xs.front() - 3.0 * h_eff;
        const double xmax = xs.back()  + 3.0 * h_eff;
        grid.resize(M);
        if (M == 1) grid[0] = xmin;
        else {
            const double step = (xmax - xmin) / double(M - 1);
            for (size_t i = 0; i < M; ++i) grid[i] = xmin + step * double(i);
            grid[M - 1] = xmax;
        }
    } else {
        const size_t M = pts.numel();
        grid.resize(M);
        for (size_t i = 0; i < M; ++i) grid[i] = pts.elemAsDouble(i);
    }

    const size_t M = grid.size();
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    const std::string mode = function_mode.empty() ? "pdf"
                                                    : lower(function_mode);
    Value fv = Value::matrix(1, M, ValueType::DOUBLE, mr);
    double *fd = fv.doubleDataMut();
    const double inv_h = 1.0 / h_eff;

    if (mode == "pdf") {
        for (size_t j = 0; j < M; ++j) {
            double sum = 0.0;
            for (size_t i = 0; i < N; ++i) {
                const double u = (grid[j] - xs[i]) * inv_h;
                sum += ws[i] * ks_pdf(u, kernel);
            }
            fd[j] = sum * inv_h * Winv;
        }
    } else if (mode == "cdf" || mode == "survivor" || mode == "cumhazard"
               || mode == "cumulative hazard") {
        for (size_t j = 0; j < M; ++j) {
            double sum = 0.0;
            for (size_t i = 0; i < N; ++i) {
                const double u = (grid[j] - xs[i]) * inv_h;
                sum += ws[i] * ks_cdf(u, kernel);
            }
            const double F = sum * Winv;
            if      (mode == "cdf")      fd[j] = F;
            else if (mode == "survivor") fd[j] = 1.0 - F;
            else                         fd[j] = -std::log(std::max(1.0 - F, 1e-300));
        }
    } else if (mode == "icdf") {
        // Inverse CDF: the `grid` values are PROBABILITIES p in [0, 1].
        // Solve F(x) = p via Newton's method on the smoothed cdf/pdf, seeded
        // by linear inverse-interpolation of a 100-point grid cdf over the
        // data range — matching MATLAB R2025b's ksdensity icdf algorithm
        // (compute_initial_icdf + Newton in statkscompute.m).
        const double infv = std::numeric_limits<double>::infinity();
        auto cdf_at = [&](double xq) {
            double s = 0.0;
            for (size_t i = 0; i < N; ++i)
                s += ws[i] * ks_cdf((xq - xs[i]) * inv_h, kernel);
            return s * Winv;
        };
        auto pdf_at = [&](double xq) {
            double s = 0.0;
            for (size_t i = 0; i < N; ++i)
                s += ws[i] * ks_pdf((xq - xs[i]) * inv_h, kernel);
            return s * inv_h * Winv;
        };
        // 100-point grid cdf over [min(x), max(x)] for the initial guess.
        const size_t G = 100;
        std::vector<double> gx(G), gF(G);
        {
            const double gmin = xs.front();
            const double gmax = xs.back();
            const double step = (G > 1) ? (gmax - gmin) / double(G - 1) : 0.0;
            for (size_t g = 0; g < G; ++g) {
                gx[g] = (g + 1 == G) ? gmax : gmin + step * double(g);
                gF[g] = cdf_at(gx[g]);
            }
        }
        const double min_dF0 = std::sqrt(std::numeric_limits<double>::epsilon());
        for (size_t j = 0; j < M; ++j) {
            const double p = grid[j];
            if (p < 0.0 || p > 1.0) { fd[j] = nan;   continue; }
            if (p == 0.0)           { fd[j] = -infv; continue; }
            if (p == 1.0)           { fd[j] =  infv; continue; }
            // Initial guess via linear inverse interpolation on (gF, gx).
            double x0;
            if      (p <= gF.front()) x0 = gx.front();
            else if (p >= gF.back())  x0 = gx.back();
            else {
                size_t lo = 0;
                while (lo + 1 < G && gF[lo + 1] < p) ++lo;
                const double denom = gF[lo + 1] - gF[lo];
                const double t = (denom != 0.0) ? (p - gF[lo]) / denom : 0.0;
                x0 = gx[lo] + t * (gx[lo + 1] - gx[lo]);
            }
            // Newton refinement on the smoothed CDF.
            for (int iter = 0; iter < 100; ++iter) {
                const double F0 = cdf_at(x0);
                double dF0 = pdf_at(x0);
                if (dF0 < min_dF0) dF0 = min_dF0;
                const double dp = p - F0;
                const double dx = dp / dF0;
                x0 += dx;
                if (std::fabs(dx) <= 1e-6 * std::fabs(x0) || std::fabs(dp) <= 1e-8)
                    break;
            }
            fd[j] = x0;
        }
    } else {
        throw Error("ksdensity: unknown Function '" + mode + "'",
                    0, 0, "ksdensity", "", "numkit:ksdensity:badfn");
    }

    Value xiV = Value::matrix(1, M, ValueType::DOUBLE, mr);
    double *xd = xiV.doubleDataMut();
    for (size_t i = 0; i < M; ++i) xd[i] = grid[i];
    return {std::move(fv), std::move(xiV), Value::scalar(bw, mr)};
}

// Backward-compat 4-arg form: pdf with normal kernel, default
// numpoints=100, no weights.
std::tuple<Value, Value, Value>
ksdensity(const Value &x, const Value &pts, double bw_user, std::pmr::memory_resource *mr)
{
    auto R = ksdensity_full(x, pts, bw_user, "normal", "pdf", 100, nullptr, mr);
    return std::make_tuple(std::move(R.f), std::move(R.xi), std::move(R.bw));
}

// ── prepareCurveData / prepareSurfaceData ─────────────────────────────

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

std::tuple<Value, Value, Value>
prepareCurveData(const Value &x, const Value &y, const Value &w, std::pmr::memory_resource *mr)
{
    const size_t Nx = x.numel();
    const size_t Ny = y.numel();
    const bool   hasW = !w.isEmpty();
    const size_t Nw = hasW ? w.numel() : Nx;
    if (Nx != Ny || (hasW && Nw != Nx))
        throw Error("prepareCurveData: x, y" +
                    std::string(hasW ? ", w" : "") + " must be same length",
                    0, 0, "prepareCurveData", "", "numkit:prepCD:size");

    std::vector<double> xv(Nx), yv(Nx), wv(hasW ? Nx : 0);
    std::vector<uint8_t> keep(Nx, 1);
    for (size_t i = 0; i < Nx; ++i) {
        xv[i] = x.elemAsDouble(i);
        yv[i] = y.elemAsDouble(i);
        if (hasW) wv[i] = w.elemAsDouble(i);
        if (!finite_double(xv[i]) || !finite_double(yv[i]) ||
            (hasW && !finite_double(wv[i])))
            keep[i] = 0;
    }

    Value xo = pack_filtered(xv, keep, mr);
    Value yo = pack_filtered(yv, keep, mr);
    Value wo = hasW ? pack_filtered(wv, keep, mr)
                    : Value::matrix(0, 1, ValueType::DOUBLE, mr);
    return {std::move(xo), std::move(yo), std::move(wo)};
}

std::tuple<Value, Value, Value>
prepareSurfaceData(const Value &x, const Value &y, const Value &z, std::pmr::memory_resource *mr)
{
    // For surface fits MATLAB lets x and y be either vectors of length
    // numel(z), or matrices the same shape as z (meshgrid). Normalise
    // by linearising in column-major order; numel must match.
    const size_t Nz = z.numel();
    const size_t Nx = x.numel();
    const size_t Ny = y.numel();

    auto broadcastTo = [&](const Value &v, size_t target) -> std::vector<double> {
        std::vector<double> out(target);
        if (v.numel() == target) {
            for (size_t i = 0; i < target; ++i) out[i] = v.elemAsDouble(i);
        } else {
            // Allow x = row of length cols, y = col of length rows for
            // implicit meshgrid (matches MATLAB behaviour).
            const auto &dz = z.dims();
            const size_t rows = dz.rows();
            const size_t cols = dz.cols();
            if (v.numel() == cols) {
                // treat as x-coords per column
                for (size_t c = 0; c < cols; ++c)
                    for (size_t r = 0; r < rows; ++r)
                        out[r + c * rows] = v.elemAsDouble(c);
            } else if (v.numel() == rows) {
                for (size_t c = 0; c < cols; ++c)
                    for (size_t r = 0; r < rows; ++r)
                        out[r + c * rows] = v.elemAsDouble(r);
            } else {
                throw Error("prepareSurfaceData: x, y, z size mismatch",
                            0, 0, "prepareSurfaceData", "", "numkit:prepSD:size");
            }
        }
        return out;
    };

    std::vector<double> xv = broadcastTo(x, Nz);
    std::vector<double> yv = broadcastTo(y, Nz);
    std::vector<double> zv(Nz);
    for (size_t i = 0; i < Nz; ++i) zv[i] = z.elemAsDouble(i);

    std::vector<uint8_t> keep(Nz, 1);
    for (size_t i = 0; i < Nz; ++i) {
        if (!finite_double(xv[i]) || !finite_double(yv[i]) ||
            !finite_double(zv[i]))
            keep[i] = 0;
    }

    Value xo = pack_filtered(xv, keep, mr);
    Value yo = pack_filtered(yv, keep, mr);
    Value zo = pack_filtered(zv, keep, mr);
    (void)Nx; (void)Ny;
    return {std::move(xo), std::move(yo), std::move(zo)};
}

// ── datastats ─────────────────────────────────────────────────────────

std::tuple<Value, Value, Value, Value, Value, Value, Value>
datastats(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    std::vector<double> v;
    v.reserve(N);
    for (size_t i = 0; i < N; ++i) v.push_back(x.elemAsDouble(i));

    const auto nan = std::numeric_limits<double>::quiet_NaN();
    if (N == 0) {
        return std::make_tuple(Value::scalar(0.0, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(nan, mr));
    }

    double mn = v[0], mx = v[0], sum = 0.0;
    for (double vi : v) {
        if (vi < mn) mn = vi;
        if (vi > mx) mx = vi;
        sum += vi;
    }
    const double mean = sum / static_cast<double>(N);

    std::vector<double> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    double median;
    if (N % 2 == 1) median = sorted[N / 2];
    else            median = 0.5 * (sorted[N / 2 - 1] + sorted[N / 2]);

    double sd = 0.0;
    if (N > 1) {
        double sq = 0.0;
        for (double vi : v) { const double d = vi - mean; sq += d * d; }
        sd = std::sqrt(sq / static_cast<double>(N - 1));
    }
    const double range = mx - mn;

    return std::make_tuple(Value::scalar(static_cast<double>(N), mr),
                           Value::scalar(mx,     mr),
                           Value::scalar(mn,     mr),
                           Value::scalar(mean,   mr),
                           Value::scalar(median, mr),
                           Value::scalar(range,  mr),
                           Value::scalar(sd,     mr));
}

// Forward declarations for ecdf (defined at the end of this TU).
struct EcdfFull { Value f, x, flo, fup; };
EcdfFull ecdf_full(const Value &y, const Value *freq, const std::string &function_mode, double alpha, bool want_bounds, std::pmr::memory_resource *mr);

// Forward declaration for ksdensity_full. KsdensityFull is defined above.
struct KsdensityFull;
KsdensityFull ksdensity_full(const Value &x, const Value &pts, double bw_user, const std::string &kernel_name, const std::string &function_mode, size_t numpoints, const Value *weights, std::pmr::memory_resource *mr);

// ── filloutliers (public typed entry; see descriptive.hpp) ────────────
// Typed front over filloutliers_of covering the median / mean / quartiles
// find-methods. 'percentiles' (needs a [lo hi] pair) and the deferred
// grubbs/gesd/movmedian/movmean methods stay script-only via the adapter.
Value filloutliers(const Value &A, const Value &fillMethod,
                   const std::string &findMethod, double thresholdFactor,
                   std::pmr::memory_resource *mr)
{
    std::string detect = findMethod;
    for (char &ch : detect)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (detect != "median" && detect != "mean" && detect != "quartiles")
        throw Error("filloutliers: findMethod must be 'median', 'mean', or "
                    "'quartiles' for the C++ API ('percentiles' and "
                    "grubbs/gesd/movmedian/movmean are script-only / deferred)",
                    0, 0, "filloutliers", "", "numkit:filloutliers:findmethod");
    // Per-method default threshold when thresholdFactor is NaN (matches the
    // adapter): median/mean default 3; quartiles internal 3.0 (= MATLAB k=1.5).
    // A user-supplied factor for quartiles is the IQR multiplier k; the
    // internal formula uses 0.5·tf·IQR, so scale by 2.
    double tf;
    if (std::isnan(thresholdFactor))
        tf = 3.0;
    else
        tf = (detect == "quartiles") ? 2.0 * thresholdFactor : thresholdFactor;
    return filloutliers_of(A, fillMethod, detect, tf, 0.0, 0.0, mr);
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void prepareCurveData_reg(Span<const Value> args, size_t nargout,
                          Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("prepareCurveData: requires (X, Y[, W])",
                    0, 0, "prepareCurveData", "", "numkit:prepCD:nargin");
    auto *mr = ctx.engine->resource();
    Value w_empty = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &w = (args.size() >= 3) ? args[2] : w_empty;
    auto [xo, yo, wo] = prepareCurveData(args[0], args[1], w, mr);
    outs[0] = std::move(xo);
    if (nargout > 1) outs[1] = std::move(yo);
    if (nargout > 2) outs[2] = std::move(wo);
}

void prepareSurfaceData_reg(Span<const Value> args, size_t nargout,
                            Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("prepareSurfaceData: requires (X, Y, Z)",
                    0, 0, "prepareSurfaceData", "", "numkit:prepSD:nargin");
    auto [xo, yo, zo] = prepareSurfaceData(args[0], args[1], args[2], ctx.engine->resource());
    outs[0] = std::move(xo);
    if (nargout > 1) outs[1] = std::move(yo);
    if (nargout > 2) outs[2] = std::move(zo);
}

void ksdensity_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ksdensity: requires (x[, pts][, N-V pairs])",
                    0, 0, "ksdensity", "", "numkit:ksdensity:nargin");
    auto *mr = ctx.engine->resource();
    Value pts = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    double bw_user = 0.0;
    std::string kernel = "normal";
    std::string function_mode = "pdf";
    size_t numpoints = 100;
    const Value *weights = nullptr;
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    size_t i = 1;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        pts = args[i];
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        const std::string name = lower(args[i].toString());
        const Value &v = args[i + 1];
        if      (name == "bandwidth" || name == "width") {
            if (v.isChar() || v.isString()) {
                // 'normal-approx' / 'plug-in' string forms — only
                // 'normal-approx' (default behavior) is supported.
                const std::string s = lower(v.toString());
                if (s != "normal-approx" && s != "plug-in")
                    throw Error("ksdensity: unknown Bandwidth string '" + s + "'",
                                0, 0, "ksdensity", "", "numkit:ksdensity:bw");
                bw_user = 0.0;
            } else {
                bw_user = v.toScalar();
            }
        }
        else if (name == "kernel")    kernel = v.toString();
        else if (name == "function")  function_mode = v.toString();
        else if (name == "numpoints") numpoints = (size_t)v.toScalar();
        else if (name == "weights")   { if (!v.isEmpty()) weights = &v; }
        else if (name == "censoring" || name == "support"
                 || name == "boundarycorrection") {
            if (!v.isEmpty())
                throw Error("ksdensity: '" + name + "' is not yet supported",
                            0, 0, "ksdensity", "", "numkit:ksdensity:nyi");
        }
        // 'PlotFcn' silently ignored (no-op headless).
        i += 2;
    }
    auto R = ksdensity_full(args[0], pts, bw_user, kernel, function_mode, numpoints, weights, mr);
    outs[0] = std::move(R.f);
    if (nargout > 1) outs[1] = std::move(R.xi);
    if (nargout > 2) outs[2] = std::move(R.bw);
}

void datastats_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("datastats: requires X[, Y]",
                    0, 0, "datastats", "", "numkit:datastats:nargin");
    auto build = [&](const Value &v) {
        auto [num, mx, mn, me, md, rg, sd] =
            datastats(v, ctx.engine->resource());
        Value s = Value::structure(ctx.engine->resource());
        s.field("num")    = num;
        s.field("max")    = mx;
        s.field("min")    = mn;
        s.field("mean")   = me;
        s.field("median") = md;
        s.field("range")  = rg;
        s.field("std")    = sd;
        return s;
    };
    outs[0] = build(args[0]);
    // Two-arg form returns separate stats structs for x and y.
    // We only fill outs[1] if a second argument was supplied AND the
    // caller actually requested two outputs (otherwise it's a no-op).
    if (args.size() >= 2 && outs.size() > 1)
        outs[1] = build(args[1]);
}

void bounds_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bounds: requires at least 1 argument",
                     0, 0, "bounds", "", "numkit:bounds:nargin");
    int dim = 0;
    bool flatten = false;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &a = args[1];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (s == "all") flatten = true;
            else throw Error("bounds: unknown flag '" + s + "'",
                             0, 0, "bounds", "", "numkit:bounds:badFlag");
        } else if (a.numel() == 1) {
            dim = static_cast<int>(a.toScalar());
        } else {
            // vecdim: full-flatten only
            std::vector<int> dims;
            for (size_t i = 0; i < a.numel(); ++i)
                dims.push_back(static_cast<int>(a.elemAsDouble(i)));
            const int rank = args[0].dims().is3D() ? 3
                              : (args[0].dims().isVector() || args[0].isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (int d : dims) {
                if (d < 1 || d > rank)
                    throw Error("bounds: vecdim entries out of range",
                                0, 0, "bounds", "", "numkit:bounds:vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error("bounds: partial vecdim reduction is not yet "
                            "supported (only full-flatten vecdim)",
                            0, 0, "bounds", "", "numkit:bounds:vecdim");
            flatten = true;
        }
    }
    auto *mr = ctx.engine->resource();
    if (flatten) {
        Value flat = Value::matrix(1, args[0].numel(), ValueType::DOUBLE, mr);
        if (args[0].numel() > 0) {
            const double *src = args[0].doubleData();
            std::copy(src, src + args[0].numel(), flat.doubleDataMut());
        }
        auto [lo, hi] = bounds(flat, 2, mr);
        outs[0] = std::move(lo);
        if (nargout > 1) outs[1] = std::move(hi);
    } else {
        auto [lo, hi] = bounds(args[0], dim, mr);
        outs[0] = std::move(lo);
        if (nargout > 1) outs[1] = std::move(hi);
    }
}

void iqr_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("iqr: requires at least 1 argument",
                     0, 0, "iqr", "", "numkit:iqr:nargin");
    int dim = 0;
    bool flatten = false;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &a = args[1];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (s == "all") flatten = true;
            else throw Error("iqr: unknown flag '" + s + "'",
                              0, 0, "iqr", "", "numkit:iqr:badFlag");
        } else if (a.numel() == 1) {
            dim = static_cast<int>(a.toScalar());
        } else {
            // vecdim: only full-flatten coverage supported
            std::vector<int> dims;
            for (size_t i = 0; i < a.numel(); ++i)
                dims.push_back(static_cast<int>(a.elemAsDouble(i)));
            const int rank = args[0].dims().is3D() ? 3
                              : (args[0].dims().isVector() || args[0].isScalar() ? 1 : 2);
            std::vector<bool> seen(rank + 1, false);
            for (int d : dims) {
                if (d < 1 || d > rank)
                    throw Error("iqr: vecdim entries out of range",
                                0, 0, "iqr", "", "numkit:iqr:vecdim");
                seen[d] = true;
            }
            bool allCovered = true;
            for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
            if (!allCovered)
                throw Error("iqr: partial vecdim reduction is not yet "
                            "supported (only full-flatten vecdim like [1 2])",
                            0, 0, "iqr", "", "numkit:iqr:vecdim");
            flatten = true;
        }
    }
    auto *mr = ctx.engine->resource();
    if (flatten) {
        // Flatten and compute on the 1×N row.
        Value flat = Value::matrix(1, args[0].numel(), ValueType::DOUBLE, mr);
        if (args[0].numel() > 0) {
            const double *src = args[0].doubleData();
            std::copy(src, src + args[0].numel(), flat.doubleDataMut());
        }
        outs[0] = iqr(flat, 2, mr);
    } else {
        outs[0] = iqr(args[0], dim, mr);
    }
}

void maxk_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("maxk: requires at least 2 arguments (x, k)",
                     0, 0, "maxk", "", "numkit:maxk:nargin");
    const int k = static_cast<int>(args[1].toScalar());
    int dim = 0;
    // Optional positional dim (numeric scalar that's not a string).
    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    // Remaining args may be Name-Value pairs; only 'ComparisonMethod'
    // is documented (real|abs|auto). For real input 'auto' = 'real'; 'abs'
    // ranks by magnitude |x| (returning the original signed values).
    bool byAbs = false;
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("maxk: expected Name-Value pair",
                        0, 0, "maxk", "", "numkit:maxk:nv");
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (name == "comparisonmethod") {
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (m != "real" && m != "abs" && m != "auto")
                throw Error("maxk: ComparisonMethod must be 'real', 'abs' or 'auto'",
                            0, 0, "maxk", "", "numkit:maxk:cm");
            byAbs = (m == "abs");
        } else {
            throw Error("maxk: unknown Name-Value '" + name + "'",
                        0, 0, "maxk", "", "numkit:maxk:nv");
        }
        i += 2;
    }
    auto *mr = ctx.engine->resource();
    Value idx;
    Value *idxPtr = (nargout >= 2) ? &idx : nullptr;
    outs[0] = topKAlongDim(args[0], dim, k, /*ascending=*/false, "maxk", mr, idxPtr, byAbs);
    if (nargout >= 2) outs[1] = std::move(idx);
}

void mink_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mink: requires at least 2 arguments (x, k)",
                     0, 0, "mink", "", "numkit:mink:nargin");
    const int k = static_cast<int>(args[1].toScalar());
    int dim = 0;
    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()
        && !args[i].isEmpty()) {
        dim = static_cast<int>(args[i].toScalar()); ++i;
    }
    bool byAbs = false;
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("mink: expected Name-Value pair",
                        0, 0, "mink", "", "numkit:mink:nv");
        std::string name = args[i].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (name == "comparisonmethod") {
            std::string m = args[i + 1].toString();
            std::transform(m.begin(), m.end(), m.begin(),
                           [](unsigned char c){ return std::tolower(c); });
            if (m != "real" && m != "abs" && m != "auto")
                throw Error("mink: ComparisonMethod must be 'real', 'abs' or 'auto'",
                            0, 0, "mink", "", "numkit:mink:cm");
            byAbs = (m == "abs");
        } else {
            throw Error("mink: unknown Name-Value '" + name + "'",
                        0, 0, "mink", "", "numkit:mink:nv");
        }
        i += 2;
    }
    auto *mr = ctx.engine->resource();
    Value idx;
    Value *idxPtr = (nargout >= 2) ? &idx : nullptr;
    outs[0] = topKAlongDim(args[0], dim, k, /*ascending=*/true, "mink", mr, idxPtr, byAbs);
    if (nargout >= 2) outs[1] = std::move(idx);
}

// Common parser for mape/rmse trailing args: optional dim ('all', vecdim,
// integer scalar). Returns (dim, flatten). Vector inputs to mape/rmse
// are inherently 1-D so flatten and dim=0 produce the same result.
namespace {
void parseDimOrAll(const Value &x, Span<const Value> args, size_t pos,
                   int &dim, bool &flatten, const char *fn)
{
    dim = 0; flatten = false;
    if (pos >= args.size() || args[pos].isEmpty()) return;
    const Value &a = args[pos];
    if (a.isChar() || a.isString()) {
        std::string s = a.toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (s == "all") { flatten = true; return; }
        throw Error(std::string(fn) + ": unknown flag '" + s + "'",
                    0, 0, fn, "", std::string("numkit:") + fn + ":badFlag");
    }
    if (a.numel() == 1) { dim = static_cast<int>(a.toScalar()); return; }
    // vecdim — full-flatten only
    const int rank = x.dims().is3D() ? 3
                      : (x.dims().isVector() || x.isScalar() ? 1 : 2);
    std::vector<bool> seen(rank + 1, false);
    for (size_t i = 0; i < a.numel(); ++i) {
        int d = static_cast<int>(a.elemAsDouble(i));
        if (d < 1 || d > rank)
            throw Error(std::string(fn) + ": vecdim entries out of range",
                        0, 0, fn, "", std::string("numkit:") + fn + ":vecdim");
        seen[d] = true;
    }
    bool allCovered = true;
    for (int d = 1; d <= rank; ++d) if (!seen[d]) allCovered = false;
    if (!allCovered)
        throw Error(std::string(fn) + ": partial vecdim reduction not supported",
                    0, 0, fn, "", std::string("numkit:") + fn + ":vecdim");
    flatten = true;
}

Value flattenToRow(const Value &x, std::pmr::memory_resource *mr)
{
    Value flat = Value::matrix(1, x.numel(), ValueType::DOUBLE, mr);
    if (x.numel() > 0) {
        const double *src = x.doubleData();
        std::copy(src, src + x.numel(), flat.doubleDataMut());
    }
    return flat;
}
} // anonymous

void mape_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mape: requires 2 arguments (F, A)",
                     0, 0, "mape", "", "numkit:mape:nargin");
    int dim = 0; bool flatten = false;
    parseDimOrAll(args[0], args, 2, dim, flatten, "mape");
    auto *mr = ctx.engine->resource();
    if (flatten) {
        outs[0] = mape(flattenToRow(args[0], mr), flattenToRow(args[1], mr), 2, mr);
    } else {
        outs[0] = mape(args[0], args[1], dim, mr);
    }
}

void rmse_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rmse: requires at least 2 arguments (F, A)",
                     0, 0, "rmse", "", "numkit:rmse:nargin");
    int dim = 0; bool flatten = false;
    parseDimOrAll(args[0], args, 2, dim, flatten, "rmse");
    auto *mr = ctx.engine->resource();
    if (flatten) {
        outs[0] = rmse(flattenToRow(args[0], mr), flattenToRow(args[1], mr), 2, mr);
    } else {
        outs[0] = rmse(args[0], args[1], dim, mr);
    }
}

void ecdf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ecdf: requires (y[, N-V pairs])",
                     0, 0, "ecdf", "", "numkit:ecdf:nargin");
    auto *mr = ctx.engine->resource();
    std::string function_mode = "cdf";
    double alpha = 0.05;
    const Value *freq = nullptr;
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) break;
        const std::string key = lower(args[i].toString());
        const Value &v = args[i + 1];
        if      (key == "function")  function_mode = v.toString();
        else if (key == "frequency") {
            if (!v.isEmpty()) freq = &v;
        }
        else if (key == "alpha")     alpha = v.toScalar();
        else if (key == "censoring") {
            if (!v.isEmpty())
                throw Error("ecdf: 'Censoring' is not yet supported "
                            "(Kaplan-Meier estimator). Skip the arg or "
                            "filter censored observations beforehand.",
                            0, 0, "ecdf", "", "numkit:ecdf:censoring_nyi");
        }
        else if (key == "iterationlimit" || key == "tolerance"
                 || key == "icmfrequency" || key == "bounds") {
            // Silently accepted (no-op for non-censored ecdf).
        }
    }
    const bool want_bounds = (nargout > 2);
    auto R = ecdf_full(args[0], freq, function_mode, alpha, want_bounds, mr);
    outs[0] = std::move(R.f);
    if (nargout > 1) outs[1] = std::move(R.x);
    if (nargout > 2) outs[2] = std::move(R.flo);
    if (nargout > 3) outs[3] = std::move(R.fup);
}

void ecdfhist_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ecdfhist: requires (f, x [, m])",
                     0, 0, "ecdfhist", "", "numkit:ecdfhist:nargin");
    int m = 10;
    if (args.size() >= 3 && !args[2].isEmpty())
        m = static_cast<int>(args[2].toScalar());
    auto [n, c] = ecdfhist(args[0], args[1], m, ctx.engine->resource());
    outs[0] = std::move(n);
    if (nargout > 1) outs[1] = std::move(c);
}

// ── partialcorr adapter ──────────────────────────────────────────────

void partialcorr_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    if (args.empty())
        throw Error("partialcorr: requires (X), (X, Z), or (X, Y, Z)",
                    0, 0, "partialcorr", "", "numkit:partialcorr:nargin");
    // Positional matrices precede the trailing Name-Value pairs (which are
    // all strings: 'Rows'/'Type' plus their values).
    std::size_t posN = args.size();
    while (posN > 0 && (args[posN - 1].isChar() || args[posN - 1].isString()))
        --posN;
    if (posN < 1 || posN > 3)
        throw Error("partialcorr: requires (X), (X, Z), or (X, Y, Z)",
                    0, 0, "partialcorr", "", "numkit:partialcorr:nargin");

    // Parse the 'Rows' NaN policy from the NV region (args[posN..]):
    //   'all' (default) NaN-poison, 'complete' listwise deletion.
    int rowsMode = 0;  // 0=all, 1=complete, 2=pairwise
    for (std::size_t i = posN; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string name = args[i].toString();
        for (char &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "rows") {
            std::string v = args[i + 1].toString();
            for (char &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (v == "all") rowsMode = 0;
            else if (v == "complete") rowsMode = 1;
            else if (v == "pairwise") rowsMode = 2;
            else throw Error("partialcorr: Rows must be 'all', 'complete', or "
                             "'pairwise'", 0, 0, "partialcorr", "",
                             "numkit:partialcorr:BadRows");
        }
    }
    if (rowsMode == 2)
        throw Error("partialcorr: 'pairwise' rows option is not yet supported "
                    "(use 'complete')",
                    0, 0, "partialcorr", "", "numkit:partialcorr:Pairwise");

    auto nrows = [](const Value &M) {
        return (M.dims().isVector() || M.isScalar())
                   ? M.numel() : static_cast<std::size_t>(M.dims().rows());
    };
    auto ncols = [](const Value &M) {
        return (M.dims().isVector() || M.isScalar())
                   ? static_cast<std::size_t>(1)
                   : static_cast<std::size_t>(M.dims().cols());
    };

    Value c0, c1, c2;
    const Value *p0 = &args[0], *p1 = (posN >= 2 ? &args[1] : nullptr),
                *p2 = (posN >= 3 ? &args[2] : nullptr);
    if (rowsMode == 1) {
        // Listwise deletion: drop every row with a NaN in ANY of the
        // positional matrices (they all share the same row index).
        const std::size_t n = nrows(args[0]);
        ScratchArena scratch(mr);
        ScratchVec<std::size_t> keep(&scratch);
        const Value *mats[3] = {p0, p1, p2};
        for (std::size_t r = 0; r < n; ++r) {
            bool ok = true;
            for (std::size_t t = 0; t < posN && ok; ++t) {
                const Value &M = *mats[t];
                const std::size_t p = ncols(M);
                for (std::size_t c = 0; c < p && ok; ++c)
                    if (std::isnan(M.elemAsDouble(r + c * n))) ok = false;
            }
            if (ok) keep.push_back(r);
        }
        const std::size_t m = keep.size();
        auto cleanOne = [&](const Value &M) {
            const std::size_t p = ncols(M);
            Value out = Value::matrix(m, p, ValueType::DOUBLE, mr);
            double *o = out.doubleDataMut();
            for (std::size_t c = 0; c < p; ++c)
                for (std::size_t k = 0; k < m; ++k)
                    o[k + c * m] = M.elemAsDouble(keep[k] + c * n);
            return out;
        };
        c0 = cleanOne(args[0]);
        p0 = &c0;
        if (p1) { c1 = cleanOne(args[1]); p1 = &c1; }
        if (p2) { c2 = cleanOne(args[2]); p2 = &c2; }
    }

    if (posN == 1)
        outs[0] = partialcorr_xx(*p0, mr);
    else if (posN == 2)
        outs[0] = partialcorr_xz(*p0, *p1, mr);
    else
        outs[0] = partialcorr_of(*p0, *p1, *p2, mr);
}

// ── corr / detrend adapters ──────────────────────────────────────────

namespace {

enum class CorrType { Pearson, Spearman, Kendall };

// Parse a 'Type' Name-Value option (case-insensitive) from args[start..].
// Other NV names (Rows/Tail/Weights) are skipped. Default Pearson.
CorrType parseCorrType(Span<const Value> args, std::size_t start)
{
    for (std::size_t i = start; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string name = args[i].toString();
        for (char &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "type") {
            std::string v = args[i + 1].toString();
            for (char &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (v == "pearson")  return CorrType::Pearson;
            if (v == "spearman") return CorrType::Spearman;
            if (v == "kendall")  return CorrType::Kendall;
            throw Error("corr: Type must be 'Pearson', 'Spearman', or 'Kendall'",
                        0, 0, "corr", "", "numkit:corr:BadType");
        }
    }
    return CorrType::Pearson;
}

// Replace each column of X (n×p) with its tied (average) ranks. Pearson of
// the ranks is exactly the Spearman correlation.
Value rankColumns(const Value &X, std::pmr::memory_resource *mr)
{
    const std::size_t n = static_cast<std::size_t>(X.dims().dim(0));
    const std::size_t p = (X.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(X.dims().dim(1)) : 1;
    Value R = Value::matrix(n, p, ValueType::DOUBLE, mr);
    double *rd = R.doubleDataMut();
    ScratchArena scratch(mr);
    for (std::size_t c = 0; c < p; ++c) {
        ScratchVec<std::size_t> idx(n, static_cast<std::size_t>(0), &scratch);
        for (std::size_t i = 0; i < n; ++i) idx[i] = i;
        std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
            return X.elemAsDouble(a + c * n) < X.elemAsDouble(b + c * n);
        });
        std::size_t i = 0;
        while (i < n) {
            std::size_t j = i;
            const double v = X.elemAsDouble(idx[i] + c * n);
            while (j + 1 < n && X.elemAsDouble(idx[j + 1] + c * n) == v) ++j;
            const double avg =
                (static_cast<double>(i + 1) + static_cast<double>(j + 1)) / 2.0;
            for (std::size_t k = i; k <= j; ++k) rd[idx[k] + c * n] = avg;
            i = j + 1;
        }
    }
    return R;
}

// Kendall tau-b between column ci of X and column cj of Y (length n).
double kendallTauB(const Value &X, std::size_t ci,
                   const Value &Y, std::size_t cj, std::size_t n)
{
    long nc = 0, nd = 0, n1 = 0, n2 = 0; // concordant, discordant, ties in x, ties in y
    for (std::size_t i = 0; i < n; ++i) {
        const double ai = X.elemAsDouble(i + ci * n);
        const double bi = Y.elemAsDouble(i + cj * n);
        for (std::size_t j = i + 1; j < n; ++j) {
            const double da = X.elemAsDouble(j + ci * n) - ai;
            const double db = Y.elemAsDouble(j + cj * n) - bi;
            const bool tiea = (da == 0.0);
            const bool tieb = (db == 0.0);
            if (tiea) ++n1;
            if (tieb) ++n2;
            if (!tiea && !tieb) {
                if ((da > 0.0) == (db > 0.0)) ++nc; else ++nd;
            }
        }
    }
    const double n0 = static_cast<double>(n) * (static_cast<double>(n) - 1.0) / 2.0;
    const double denom = std::sqrt((n0 - static_cast<double>(n1)) *
                                   (n0 - static_cast<double>(n2)));
    if (denom <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    return (static_cast<double>(nc) - static_cast<double>(nd)) / denom;
}

// p×q matrix of Kendall tau-b for every column pair of X (n×p), Y (n×q).
Value kendallMatrix(const Value &X, const Value &Y, std::pmr::memory_resource *mr)
{
    const std::size_t n = static_cast<std::size_t>(X.dims().dim(0));
    const std::size_t p = (X.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(X.dims().dim(1)) : 1;
    if (static_cast<std::size_t>(Y.dims().dim(0)) != n)
        throw Error("corr: X and Y must have the same number of rows",
                    0, 0, "corr", "", "numkit:corr:rows");
    const std::size_t q = (Y.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(Y.dims().dim(1)) : 1;
    Value out = Value::matrix(p, q, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t j = 0; j < q; ++j)
        for (std::size_t i = 0; i < p; ++i)
            od[i + j * p] = kendallTauB(X, i, Y, j, n);
    return out;
}

// ── corr 'Rows' NaN policy ────────────────────────────────────────────
enum class CorrRows { All, Complete, Pairwise };

// Parse a 'Rows' Name-Value option (case-insensitive): 'all' (default,
// NaN-poison), 'complete' (listwise deletion), 'pairwise'.
CorrRows parseCorrRows(Span<const Value> args, std::size_t start)
{
    for (std::size_t i = start; i + 1 < args.size(); i += 2) {
        if (!(args[i].isChar() || args[i].isString())) continue;
        std::string name = args[i].toString();
        for (char &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name == "rows") {
            std::string v = args[i + 1].toString();
            for (char &c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (v == "all")      return CorrRows::All;
            if (v == "complete") return CorrRows::Complete;
            if (v == "pairwise") return CorrRows::Pairwise;
            throw Error("corr: Rows must be 'all', 'complete', or 'pairwise'",
                        0, 0, "corr", "", "numkit:corr:BadRows");
        }
    }
    return CorrRows::All;
}

std::size_t corrRows(const Value &X) { return static_cast<std::size_t>(X.dims().dim(0)); }
std::size_t corrCols(const Value &X)
{
    return (X.dims().ndim() >= 2) ? static_cast<std::size_t>(X.dims().dim(1)) : 1;
}

// Listwise deletion: keep only the rows that contain no NaN across every
// column of X (and Y, when two matrices share a row index). The same kept
// rows are applied to both so the columns stay aligned.
void dropNaNRows(const Value &X, const Value *Y, std::pmr::memory_resource *mr,
                 Value &Xo, Value *Yo)
{
    const std::size_t n = corrRows(X);
    const std::size_t pX = corrCols(X);
    const std::size_t pY = Y ? corrCols(*Y) : 0;
    ScratchArena scratch(mr);
    ScratchVec<std::size_t> keep(&scratch);
    for (std::size_t r = 0; r < n; ++r) {
        bool ok = true;
        for (std::size_t c = 0; c < pX && ok; ++c)
            if (std::isnan(X.elemAsDouble(r + c * n))) ok = false;
        if (ok && Y)
            for (std::size_t c = 0; c < pY && ok; ++c)
                if (std::isnan(Y->elemAsDouble(r + c * n))) ok = false;
        if (ok) keep.push_back(r);
    }
    const std::size_t m = keep.size();
    Xo = Value::matrix(m, pX, ValueType::DOUBLE, mr);
    double *xo = Xo.doubleDataMut();
    for (std::size_t c = 0; c < pX; ++c)
        for (std::size_t k = 0; k < m; ++k)
            xo[k + c * m] = X.elemAsDouble(keep[k] + c * n);
    if (Y && Yo) {
        *Yo = Value::matrix(m, pY, ValueType::DOUBLE, mr);
        double *yo = Yo->doubleDataMut();
        for (std::size_t c = 0; c < pY; ++c)
            for (std::size_t k = 0; k < m; ++k)
                yo[k + c * m] = Y->elemAsDouble(keep[k] + c * n);
    }
}

// Pairwise Pearson correlation: each entry (i,j) uses the rows where both
// column i of X and column j of Y are non-NaN, with the means taken over
// exactly those rows.
Value corrPairwisePearson(const Value &X, const Value &Y, std::pmr::memory_resource *mr)
{
    const std::size_t n = corrRows(X);
    if (corrRows(Y) != n)
        throw Error("corr: X and Y must have the same number of rows",
                    0, 0, "corr", "", "numkit:corr:rows");
    const std::size_t p = corrCols(X), q = corrCols(Y);
    Value out = Value::matrix(p, q, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < p; ++i)
        for (std::size_t j = 0; j < q; ++j) {
            double si = 0.0, sj = 0.0;
            std::size_t m = 0;
            for (std::size_t r = 0; r < n; ++r) {
                const double a = X.elemAsDouble(r + i * n);
                const double b = Y.elemAsDouble(r + j * n);
                if (!std::isnan(a) && !std::isnan(b)) { si += a; sj += b; ++m; }
            }
            double rij;
            if (m < 2) {
                rij = std::numeric_limits<double>::quiet_NaN();
            } else {
                const double mi = si / static_cast<double>(m);
                const double mj = sj / static_cast<double>(m);
                double sxy = 0.0, sxx = 0.0, syy = 0.0;
                for (std::size_t r = 0; r < n; ++r) {
                    const double a = X.elemAsDouble(r + i * n);
                    const double b = Y.elemAsDouble(r + j * n);
                    if (std::isnan(a) || std::isnan(b)) continue;
                    const double da = a - mi, db = b - mj;
                    sxy += da * db; sxx += da * da; syy += db * db;
                }
                const double den = std::sqrt(sxx * syy);
                rij = (den > 0.0) ? sxy / den
                                  : std::numeric_limits<double>::quiet_NaN();
            }
            od[i + j * p] = rij;
        }
    return out;
}

Value corrDispatch(bool twoArg, const Value &X, const Value &Y,
                   CorrType ct, std::pmr::memory_resource *mr)
{
    if (twoArg) {
        if (ct == CorrType::Pearson)  return corr_xy(X, Y, mr);
        if (ct == CorrType::Spearman) return corr_xy(rankColumns(X, mr),
                                                     rankColumns(Y, mr), mr);
        return kendallMatrix(X, Y, mr);
    }
    if (ct == CorrType::Pearson)  return corr_xx(X, mr);
    if (ct == CorrType::Spearman) return corr_xx(rankColumns(X, mr), mr);
    return kendallMatrix(X, X, mr);
}

} // namespace

void corr_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("corr: requires at least 1 argument",
                    0, 0, "corr", "", "numkit:corr:nargin");
    auto *mr = ctx.engine->resource();
    // Distinguish corr(X[, NV…]) from corr(X, Y[, NV…]). Y is the 2nd
    // positional non-string argument.
    const bool twoArg =
        (args.size() >= 2 && !args[1].isChar() && !args[1].isString());
    const std::size_t nvStart = twoArg ? 2 : 1;
    const CorrType ct = parseCorrType(args, nvStart);
    const CorrRows rows = parseCorrRows(args, nvStart);

    if (rows == CorrRows::Pairwise) {
        // Pairwise deletion is currently supported for Pearson only.
        if (ct != CorrType::Pearson)
            throw Error("corr: 'pairwise' rows option is supported only for "
                        "the 'Pearson' type",
                        0, 0, "corr", "", "numkit:corr:PairwiseType");
        if (twoArg)
            outs[0] = corrPairwisePearson(args[0], args[1], mr);
        else
            outs[0] = corrPairwisePearson(args[0], args[0], mr);
        return;
    }

    if (rows == CorrRows::Complete) {
        // Listwise deletion: drop every row containing a NaN, then compute.
        Value Xc, Yc;
        if (twoArg) {
            dropNaNRows(args[0], &args[1], mr, Xc, &Yc);
            outs[0] = corrDispatch(true, Xc, Yc, ct, mr);
        } else {
            dropNaNRows(args[0], nullptr, mr, Xc, nullptr);
            outs[0] = corrDispatch(false, Xc, Xc, ct, mr);
        }
        return;
    }

    // 'all' (default): NaN-poisoning behaviour, unchanged.
    if (twoArg)
        outs[0] = corrDispatch(true, args[0], args[1], ct, mr);
    else
        outs[0] = corrDispatch(false, args[0], args[0], ct, mr);
}

void detrend_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("detrend: requires at least 1 argument",
                    0, 0, "detrend", "", "numkit:detrend:nargin");
    int order = 1;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        if (args[1].isChar() || args[1].isString()) {
            const auto s = args[1].toString();
            if (s == "constant") order = 0;
            else if (s == "linear") order = 1;
            else throw Error("detrend: string mode must be 'constant' or 'linear'",
                             0, 0, "detrend", "", "numkit:detrend:mode");
        } else {
            order = static_cast<int>(args[1].toScalar());
        }
    }
    // detrend(x, 1, bp): continuous piecewise-linear detrend with
    // breakpoints. Supported for linear (order 1) only — order-0 +
    // breakpoints is a rare, ill-defined MATLAB edge and is deferred
    // (the bp argument is then ignored, matching the prior behaviour).
    if (args.size() >= 3 && order == 1 && !args[2].isEmpty()
        && !args[2].isChar() && !args[2].isString()) {
        const Value &bpv = args[2];
        std::vector<double> bp;
        bp.reserve(bpv.numel());
        for (std::size_t i = 0; i < bpv.numel(); ++i)
            bp.push_back(bpv.elemAsDouble(i));
        outs[0] = detrendBP_of(args[0], bp, ctx.engine->resource());
        return;
    }
    outs[0] = detrend_of(args[0], order, ctx.engine->resource());
}

// ── missing-data adapters ────────────────────────────────────────────

// Method-aware isoutlier: per-column detection (MATLAB operates per column
// for matrices) via detect_one_column. detectTf is the value detect_one_column
// expects (median/mean: 3 == MATLAB ThresholdFactor; quartiles: 2*MATLAB-tf
// because detect_one_column scales by tf*0.5).
// Iterative Grubbs's-test outlier mask for one column. `alpha` is the
// significance level (MATLAB isoutlier 'grubbs' ThresholdFactor, default
// 0.05). At each step the point with the largest studentized deviation
// G = max|x-mean|/std (std with N-1) is compared to the Grubbs critical
// value G_crit = ((N-1)/sqrt(N))·sqrt(t²/(N-2+t²)) with t = tinv(alpha/(2N),
// N-2); if G > G_crit that point is flagged and removed, then repeat (down
// to N>=3). NaNs are ignored. Matches MATLAB R2025b.
static void grubbsColumnMask(const double *x, std::size_t n, double alpha,
                             uint8_t *maskOut, std::pmr::memory_resource *mr)
{
    for (std::size_t i = 0; i < n; ++i) maskOut[i] = 0;
    std::vector<double> v;
    std::vector<std::size_t> pos;
    v.reserve(n); pos.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isnan(x[i])) { v.push_back(x[i]); pos.push_back(i); }
    while (v.size() >= 3) {
        const std::size_t m = v.size();
        double s = 0.0;
        for (double e : v) s += e;
        const double mean = s / double(m);
        double ss = 0.0;
        for (double e : v) ss += (e - mean) * (e - mean);
        const double sd = std::sqrt(ss / double(m - 1));
        if (!(sd > 0.0)) break;
        std::size_t jmax = 0;
        double gmax = -1.0;
        for (std::size_t j = 0; j < m; ++j) {
            const double g = std::fabs(v[j] - mean) / sd;
            if (g > gmax) { gmax = g; jmax = j; }
        }
        const double dof   = double(m) - 2.0;
        const double pp    = alpha / (2.0 * double(m));
        const double tcrit = tinv(Value::scalar(pp, mr), dof, mr).toScalar();
        const double Gcrit = ((double(m) - 1.0) / std::sqrt(double(m)))
                           * std::sqrt(tcrit * tcrit / (dof + tcrit * tcrit));
        if (gmax > Gcrit) {
            maskOut[pos[jmax]] = 1;
            v.erase(v.begin() + static_cast<std::ptrdiff_t>(jmax));
            pos.erase(pos.begin() + static_cast<std::ptrdiff_t>(jmax));
        } else {
            break;
        }
    }
}

static Value isoutlierMethod(const Value &x, const std::string &method,
                             double detectTf, std::pmr::memory_resource *mr,
                             long hb = 0, long hf = 0)
{
    if (x.numel() == 0) return Value::matrix(0, 0, ValueType::LOGICAL, mr);
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    auto out = Value::matrix(r, c, ValueType::LOGICAL, mr);
    uint8_t *od = out.logicalDataMut();
    const double *xd = x.doubleData();
    if (method == "movmedian" || method == "movmean") {
        const bool isMed = (method == "movmedian");
        if (r == 1 || c == 1) {
            auto m = detect_moving_column(xd, x.numel(), isMed, hb, hf, detectTf);
            for (std::size_t i = 0; i < x.numel(); ++i) od[i] = m[i];
        } else {
            for (std::size_t col = 0; col < c; ++col) {
                auto m = detect_moving_column(xd + col * r, r, isMed, hb, hf, detectTf);
                for (std::size_t i = 0; i < r; ++i) od[col * r + i] = m[i];
            }
        }
        return out;
    }
    if (method == "grubbs") {
        // Iterative test (not a static lo/hi rule); per-column. detectTf is
        // the significance level alpha.
        if (r == 1 || c == 1) {
            grubbsColumnMask(xd, x.numel(), detectTf, od, mr);
        } else {
            for (std::size_t col = 0; col < c; ++col)
                grubbsColumnMask(xd + col * r, r, detectTf, od + col * r, mr);
        }
        return out;
    }
    if (r == 1 || c == 1) {
        // Vector: the whole run is one column.
        FoDetect d = detect_one_column(xd, x.numel(), method, detectTf);
        for (std::size_t i = 0; i < x.numel(); ++i) od[i] = d.mask[i];
    } else {
        for (std::size_t col = 0; col < c; ++col) {
            FoDetect d = detect_one_column(xd + col * r, r, method, detectTf);
            for (std::size_t i = 0; i < r; ++i) od[col * r + i] = d.mask[i];
        }
    }
    return out;
}

void isoutlier_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isoutlier: requires at least 1 argument",
                    0, 0, "isoutlier", "", "numkit:isoutlier:nargin");
    auto *mr = ctx.engine->resource();

    // isoutlier(A[, method][, 'ThresholdFactor', tf]). The method arg was
    // parsed-and-ignored (always median/MAD); now honoured.
    std::string method = "median";
    std::size_t ai = 1;
    long hb = 0, hf = 0;   // moving-window half-spans (movmedian / movmean)
    if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
        std::string m = args[1].toString();
        for (char &ch : m) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (m == "median" || m == "mean" || m == "quartiles" || m == "grubbs") {
            method = m;
            ai = 2;
        } else if (m == "movmedian" || m == "movmean") {
            method = m;
            // Moving methods require a window as the next positional arg.
            if (args.size() < 3 || args[2].isChar() || args[2].isString())
                throw Error("isoutlier: the '" + m + "' method requires a "
                            "window length (scalar k or [back forward])",
                            0, 0, "isoutlier", "", "numkit:isoutlier:window");
            const Value &win = args[2];
            if (win.numel() == 1) {
                const long k = static_cast<long>(win.toScalar());
                if (k < 1)
                    throw Error("isoutlier: window length must be a positive integer",
                                0, 0, "isoutlier", "", "numkit:isoutlier:window");
                if (k % 2 == 1) { hb = hf = (k - 1) / 2; }
                else { hb = k / 2; hf = k / 2 - 1; }
            } else if (win.numel() == 2) {
                hb = static_cast<long>(win.elemAsDouble(0));
                hf = static_cast<long>(win.elemAsDouble(1));
                if (hb < 0 || hf < 0)
                    throw Error("isoutlier: window [back forward] must be nonnegative",
                                0, 0, "isoutlier", "", "numkit:isoutlier:window");
            } else {
                throw Error("isoutlier: window must be a scalar or a 2-element "
                            "[back forward] vector",
                            0, 0, "isoutlier", "", "numkit:isoutlier:window");
            }
            ai = 3;
        } else if (m == "gesd") {
            throw Error("isoutlier: method 'gesd' is not supported in this "
                        "revision (median, mean, quartiles, grubbs, movmedian, "
                        "movmean only)",
                         0, 0, "isoutlier", "", "numkit:isoutlier:method");
        }
        // else: not a method token — leave as a Name-Value name parsed below.
    }

    // Default ThresholdFactor per method: quartiles 1.5, grubbs 0.05
    // (significance level), median/mean/movmedian/movmean 3.
    double userTf = (method == "quartiles") ? 1.5
                  : (method == "grubbs")    ? 0.05
                                            : 3.0;
    for (std::size_t i = ai; i + 1 < args.size(); i += 2) {
        if (args[i].isChar() || args[i].isString()) {
            std::string nm = args[i].toString();
            for (char &ch : nm) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (nm == "thresholdfactor")
                userTf = args[i + 1].toScalar();
            else
                throw Error("isoutlier: unknown option '" + args[i].toString() + "'",
                             0, 0, "isoutlier", "", "numkit:isoutlier:option");
        }
    }
    if (userTf < 0.0)
        throw Error("isoutlier: ThresholdFactor must be nonnegative",
                     0, 0, "isoutlier", "", "numkit:isoutlier:tf");

    const double detectTf = (method == "quartiles") ? 2.0 * userTf : userTf;
    outs[0] = isoutlierMethod(args[0], method, detectTf, mr, hb, hf);
}

// Per-column outlier mask for rmoutliers. Adds the rmoutliers-specific
// 'percentiles' method (NOT in detect_one_column): elements below the
// loP-th percentile or above the hiP-th percentile are outliers, using
// MATLAB's prctile convention (sorted positions at 100*(k-0.5)/n, clamped
// at the ends). median/mean/quartiles delegate to detect_one_column.
// Returns a column-major mask of x.numel() bytes (1 == outlier).
static std::vector<uint8_t> rmoutlierMask(const Value &x, const std::string &method,
                                          double loP, double hiP, double detectTf)
{
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    const std::size_t n = x.numel();
    std::vector<uint8_t> mask(n, 0);
    if (n == 0) return mask;
    const double *xd = x.doubleData();

    auto fill_col = [&](const double *col, std::size_t len, uint8_t *m) {
        if (method == "percentiles") {
            std::vector<double> buf;
            buf.reserve(len);
            for (std::size_t i = 0; i < len; ++i)
                if (!std::isnan(col[i])) buf.push_back(col[i]);
            if (buf.empty()) return;
            std::sort(buf.begin(), buf.end());
            auto prc = [&](double p) {
                const double q = p / 100.0 * double(buf.size()) - 0.5;
                if (q <= 0.0) return buf.front();
                if (q >= double(buf.size() - 1)) return buf.back();
                const std::size_t f = static_cast<std::size_t>(std::floor(q));
                const double fr = q - double(f);
                return buf[f] + fr * (buf[f + 1] - buf[f]);
            };
            const double lo = prc(loP);
            const double hi = prc(hiP);
            for (std::size_t i = 0; i < len; ++i)
                m[i] = (std::isnan(col[i]) ? 0
                      : ((col[i] < lo || col[i] > hi) ? 1 : 0));
        } else {
            FoDetect d = detect_one_column(col, len, method, detectTf);
            for (std::size_t i = 0; i < len; ++i) m[i] = d.mask[i];
        }
    };

    if (r == 1 || c == 1) {
        fill_col(xd, n, mask.data());
    } else {
        for (std::size_t col = 0; col < c; ++col)
            fill_col(xd + col * r, r, mask.data() + col * r);
    }
    return mask;
}

// rmoutliers(A[, method][, percentiles-vec][, 'ThresholdFactor', tf]).
// Vectors: drop flagged ENTRIES (orientation preserved). Matrices:
// detect per column, remove any ROW containing an outlier. Optional
// 2nd output is the logical mask of removed entries (vector) / rows
// (matrix). DEEP-PROBE 2026-05-31: previously delegated to the default
// median detector and IGNORED method/percentiles/ThresholdFactor, and
// flattened matrices instead of removing rows.
void rmoutliers_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rmoutliers: requires at least 1 argument",
                    0, 0, "rmoutliers", "", "numkit:rmoutliers:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];

    auto lower = [](std::string s) {
        for (char &ch : s)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return s;
    };

    std::string method = "median";
    double loP = 0.0, hiP = 0.0;
    std::size_t ai = 1;
    if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
        const std::string m = lower(args[1].toString());
        if (m == "median" || m == "mean" || m == "quartiles") {
            method = m;
            ai = 2;
        } else if (m == "percentiles") {
            method = "percentiles";
            if (args.size() < 3 || args[2].numel() != 2)
                throw Error("rmoutliers: 'percentiles' requires a 2-element "
                            "[lower upper] vector",
                            0, 0, "rmoutliers", "", "numkit:rmoutliers:percentiles");
            loP = args[2].elemAsDouble(0);
            hiP = args[2].elemAsDouble(1);
            ai = 3;
        } else if (m == "grubbs" || m == "gesd" || m == "movmedian" || m == "movmean") {
            throw Error("rmoutliers: method '" + args[1].toString() +
                            "' is not supported in this revision "
                            "(median, mean, quartiles, percentiles only)",
                         0, 0, "rmoutliers", "", "numkit:rmoutliers:method");
        }
        // else: leave as a Name-Value name parsed below.
    }

    double userTf = (method == "quartiles") ? 1.5 : 3.0;
    for (std::size_t i = ai; i + 1 < args.size(); i += 2) {
        if (args[i].isChar() || args[i].isString()) {
            const std::string nm = lower(args[i].toString());
            if (nm == "thresholdfactor")
                userTf = args[i + 1].toScalar();
            else
                throw Error("rmoutliers: unknown option '" + args[i].toString() + "'",
                             0, 0, "rmoutliers", "", "numkit:rmoutliers:option");
        }
    }
    if (userTf < 0.0)
        throw Error("rmoutliers: ThresholdFactor must be nonnegative",
                     0, 0, "rmoutliers", "", "numkit:rmoutliers:tf");
    const double detectTf = (method == "quartiles") ? 2.0 * userTf : userTf;

    const std::size_t n = x.numel();
    if (n == 0) {
        outs[0] = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        if (nargout >= 2) outs[1] = Value::matrix(0, 0, ValueType::LOGICAL, mr);
        return;
    }
    const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
    const std::size_t c = (x.dims().ndim() >= 2)
                            ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
    const double *xd = x.doubleData();
    const std::vector<uint8_t> mask = rmoutlierMask(x, method, loP, hiP, detectTf);

    if (r == 1 || c == 1) {
        // Vector: drop flagged entries, preserve orientation.
        ScratchArena scratch(mr);
        ScratchVec<double> kept(&scratch);
        kept.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            if (!mask[i]) kept.push_back(xd[i]);
        const bool colOrient = (r != 1);  // column vector → column output
        auto out = colOrient
            ? Value::matrix(kept.size(), 1, ValueType::DOUBLE, mr)
            : Value::matrix(1, kept.size(), ValueType::DOUBLE, mr);
        if (!kept.empty())
            std::copy(kept.begin(), kept.end(), out.doubleDataMut());
        outs[0] = out;
        if (nargout >= 2) {
            auto rm = Value::matrix(r, c, ValueType::LOGICAL, mr);
            uint8_t *rd = rm.logicalDataMut();
            for (std::size_t i = 0; i < n; ++i) rd[i] = mask[i];
            outs[1] = rm;
        }
    } else {
        // Matrix: remove any ROW with an outlier in any column.
        std::vector<uint8_t> rowRemove(r, 0);
        for (std::size_t col = 0; col < c; ++col)
            for (std::size_t i = 0; i < r; ++i)
                if (mask[col * r + i]) rowRemove[i] = 1;
        std::size_t keptRows = 0;
        for (std::size_t i = 0; i < r; ++i) if (!rowRemove[i]) ++keptRows;
        auto out = Value::matrix(keptRows, c, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        std::size_t orow = 0;
        for (std::size_t i = 0; i < r; ++i) {
            if (rowRemove[i]) continue;
            for (std::size_t col = 0; col < c; ++col)
                od[col * keptRows + orow] = xd[col * r + i];
            ++orow;
        }
        outs[0] = out;
        if (nargout >= 2) {
            auto rm = Value::matrix(r, 1, ValueType::LOGICAL, mr);
            uint8_t *rd = rm.logicalDataMut();
            for (std::size_t i = 0; i < r; ++i) rd[i] = rowRemove[i];
            outs[1] = rm;
        }
    }
}

void fillmissing_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fillmissing: requires (x, method[, constant_value][,'EndValues',ev])",
                    0, 0, "fillmissing", "", "numkit:fillmissing:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("fillmissing: method must be a string",
                    0, 0, "fillmissing", "", "numkit:fillmissing:method");

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return s;
    };
    const std::string m = lower(args[1].toString());
    auto *mr = ctx.engine->resource();

    // 'constant' takes a positional fill value (first non-string arg).
    double cv = 0.0;
    std::size_t ai = 2;
    if (m == "constant" && args.size() >= 3 &&
        !args[2].isChar() && !args[2].isString()) {
        cv = args[2].toScalar();
        ai = 3;
    }

    // Optional 'EndValues', ev name-value pair (extrap | none | nearest |
    // numeric constant). 'previous'/'next' EndValues deferred.
    FmEndMode endMode = FmEndMode::Extrap;
    double endVal = 0.0;
    bool haveEnd = false;
    for (std::size_t i = ai; i < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("fillmissing: expected an option name string",
                        0, 0, "fillmissing", "", "numkit:fillmissing:option");
        const std::string nm = lower(args[i].toString());
        if (i + 1 >= args.size())
            throw Error("fillmissing: option '" + nm + "' requires a value",
                        0, 0, "fillmissing", "", "numkit:fillmissing:option");
        if (nm == "endvalues") {
            haveEnd = true;
            const Value &ev = args[i + 1];
            if (ev.isChar() || ev.isString()) {
                const std::string evs = lower(ev.toString());
                if (evs == "extrap")       endMode = FmEndMode::Extrap;
                else if (evs == "none")    endMode = FmEndMode::None;
                else if (evs == "nearest") endMode = FmEndMode::Nearest;
                else if (evs == "previous" || evs == "next")
                    throw Error("fillmissing: EndValues '" + evs +
                                "' not supported in this revision ('extrap', "
                                "'none', 'nearest', or a numeric constant only)",
                                0, 0, "fillmissing", "", "numkit:fillmissing:endvalues");
                else
                    throw Error("fillmissing: unknown EndValues '" + evs + "'",
                                0, 0, "fillmissing", "", "numkit:fillmissing:endvalues");
            } else {
                endMode = FmEndMode::Const;
                endVal = ev.toScalar();
            }
        } else {
            throw Error("fillmissing: unknown option '" + nm + "'",
                        0, 0, "fillmissing", "", "numkit:fillmissing:option");
        }
    }

    if (haveEnd && endMode != FmEndMode::Extrap &&
        (m == "constant" || m == "mean" || m == "median"))
        throw Error("fillmissing: 'EndValues' is not supported with fill "
                    "method '" + m + "'",
                    0, 0, "fillmissing", "", "numkit:fillmissing:endvalues");

    outs[0] = fillmissing_of(args[0], m, cv, mr);

    if (haveEnd && endMode != FmEndMode::Extrap && args[0].numel() > 0) {
        const Value &x = args[0];
        const double *xd = x.doubleData();
        double *od = outs[0].doubleDataMut();
        const std::size_t r = static_cast<std::size_t>(x.dims().dim(0));
        const std::size_t c = (x.dims().ndim() >= 2)
                                ? static_cast<std::size_t>(x.dims().dim(1)) : 1;
        if (r == 1) {
            apply_end_values_column(xd, od, x.numel(), endMode, endVal);
        } else {
            for (std::size_t col = 0; col < c; ++col)
                apply_end_values_column(xd + col * r, od + col * r, r, endMode, endVal);
        }
    }
}

void rmmissing_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rmmissing: requires at least 1 argument",
                    0, 0, "rmmissing", "", "numkit:rmmissing:nargin");
    outs[0] = rmmissing_of(args[0], ctx.engine->resource());
}

void standardizeMissing_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("standardizeMissing: requires (x, sentinel)",
                    0, 0, "standardizeMissing", "", "numkit:standardizeMissing:nargin");
    outs[0] = standardizeMissing_of(args[0], args[1].toScalar(), ctx.engine->resource());
}

// filloutliers(A, fillmethod[, findmethod][, NV])
//   fillmethod : numeric scalar | "center" | "clip" | "previous" |
//                "next" | "nearest" | "linear"
//   findmethod : "median" (default) | "mean" | "quartiles"
//   NV         : ThresholdFactor (default per-method)
void filloutliers_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("filloutliers: requires (A, fillmethod[, findmethod][, NV])",
                    0, 0, "filloutliers", "", "numkit:filloutliers:nargin");
    auto lower = [](std::string v) {
        for (char &ch : v)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return v;
    };

    std::string detect = "median";
    double tf = 3.0;
    bool tf_set = false;
    double loP = 0.0, hiP = 0.0;
    std::size_t i = 2;
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        const std::string s = lower(args[i].toString());
        // Distinguish findmethod string vs NV name. NV names are known.
        if (s == "thresholdfactor" || s == "maxnumoutliers" ||
            s == "samplepoints"    || s == "outlierlocations") {
            // fall through — handled by NV loop below.
        } else {
            detect = s;
            if (detect == "median" || detect == "mean" || detect == "quartiles") {
                ++i;
            } else if (detect == "percentiles") {
                ++i;
                if (i >= args.size() || args[i].numel() != 2)
                    throw Error("filloutliers: 'percentiles' requires a "
                                "2-element [lower upper] vector",
                                0, 0, "filloutliers", "",
                                "numkit:filloutliers:percentiles");
                loP = args[i].elemAsDouble(0);
                hiP = args[i].elemAsDouble(1);
                ++i;
            } else {
                throw Error("filloutliers: findmethod must be 'median', "
                            "'mean', 'quartiles', or 'percentiles' in this "
                            "revision (MATLAB also supports 'grubbs', 'gesd', "
                            "'movmedian', 'movmean' — deferred)",
                            0, 0, "filloutliers", "",
                            "numkit:filloutliers:findmethod");
            }
        }
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("filloutliers: expected name-value pair",
                        0, 0, "filloutliers", "", "numkit:filloutliers:nv");
        const std::string nm = lower(args[i].toString());
        if (nm == "thresholdfactor") {
            tf = args[i + 1].toScalar(); tf_set = true;
        } else {
            throw Error("filloutliers: unsupported name-value parameter '"
                        + args[i].toString() + "'",
                        0, 0, "filloutliers", "", "numkit:filloutliers:nv");
        }
        i += 2;
    }
    // MATLAB's per-method default ThresholdFactor.
    if (!tf_set) {
        if (detect == "quartiles") tf = 3.0;     // 1.5·IQR → scaled by 0.5 internally so 3.0 here
        else                       tf = 3.0;
    } else if (detect == "quartiles") {
        // User-set tf for quartiles means "k" in [Q1 - k·IQR, Q3 + k·IQR].
        // Our internal formula uses 0.5·tf·IQR, so multiply by 2.
        tf = 2.0 * tf;
    }
    outs[0] = filloutliers_of(args[0], args[1], detect, tf, loP, hiP,
                              ctx.engine->resource());
}

// ── range / mad / geomean / harmmean / moment / trimmean adapters ────

void range_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("range: requires at least 1 argument",
                    0, 0, "range", "", "numkit:range:nargin");
    const int dim = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = range_of(args[0], dim, ctx.engine->resource());
}

void mad_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mad: requires at least 1 argument",
                    0, 0, "mad", "", "numkit:mad:nargin");
    const int flag = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    const int dim  = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = mad_of(args[0], flag, dim, ctx.engine->resource());
}

// Parse a trailing 'omitnan'/'includenan' nanflag from a geomean/harmmean
// arg list. Returns the omit flag and the count of remaining numeric args.
bool parseMeanNanFlag(Span<const Value> args, const char *fn, std::size_t &nargs)
{
    nargs = args.size();
    if (nargs >= 2 && (args[nargs - 1].isChar() || args[nargs - 1].isString())) {
        std::string f = args[nargs - 1].toString();
        for (char &c : f) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (f == "omitnan")     { --nargs; return true; }
        if (f == "includenan")  { --nargs; return false; }
        throw Error(std::string(fn) + ": unknown option '" + f + "'",
                    0, 0, fn, "", std::string("numkit:") + fn + ":badopt");
    }
    return false;
}

void geomean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("geomean: requires at least 1 argument",
                    0, 0, "geomean", "", "numkit:geomean:nargin");
    std::size_t nargs;
    const bool omitnan = parseMeanNanFlag(args, "geomean", nargs);
    const int dim = (nargs >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = geomean_of(args[0], dim, omitnan, ctx.engine->resource());
}

void harmmean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("harmmean: requires at least 1 argument",
                    0, 0, "harmmean", "", "numkit:harmmean:nargin");
    std::size_t nargs;
    const bool omitnan = parseMeanNanFlag(args, "harmmean", nargs);
    const int dim = (nargs >= 2) ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = harmmean_of(args[0], dim, omitnan, ctx.engine->resource());
}

void moment_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("moment: requires (x, order)",
                    0, 0, "moment", "", "numkit:moment:nargin");
    const int order = static_cast<int>(args[1].toScalar());
    const int dim   = (args.size() >= 3) ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = moment_of(args[0], order, dim, ctx.engine->resource());
}

void trimmean_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("trimmean: requires (x, percent)",
                    0, 0, "trimmean", "", "numkit:trimmean:nargin");
    const double pct = args[1].toScalar();

    // trimmean(x, percent [, flag] [, dim]). The 3rd arg is EITHER a string
    // flag ('round' default, or 'floor') OR a numeric dim; if a flag is
    // present the dim may follow it. Distinguish by type before toScalar.
    bool useFloor = false;
    int dim = 0;
    std::size_t i = 2;
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        std::string f = args[i].toString();
        for (char &c : f) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (f == "floor")      useFloor = true;
        else if (f == "round") useFloor = false;
        else
            throw Error("trimmean: flag must be 'round' or 'floor'",
                        0, 0, "trimmean", "", "numkit:trimmean:flag");
        ++i;
    }
    if (i < args.size())
        dim = static_cast<int>(args[i].toScalar());

    outs[0] = trimmean_of(args[0], pct, dim, useFloor, ctx.engine->resource());
}

} // namespace detail

// ── ecdfhist ─────────────────────────────────────────────────────────
// Convert (f, x) from ecdf into a probability-density histogram.
// Algorithm: probs[k] = f[k+1] - f[k]; vals[k] = x[k+1] (the unique
// data values). Bin into m equal-width bins over [min(x), max(x)],
// each height = sum(probs in bin) / bin_width.
std::tuple<Value, Value>
ecdfhist(const Value &f, const Value &x, int m, std::pmr::memory_resource *mr)
{
    if (m < 1)
        throw Error("ecdfhist: number of bins must be >= 1",
                    0, 0, "ecdfhist", "", "numkit:ecdfhist:nbins");
    const size_t Lf = f.numel();
    const size_t Lx = x.numel();
    if (Lf != Lx)
        throw Error("ecdfhist: f and x must have the same length",
                    0, 0, "ecdfhist", "", "numkit:ecdfhist:size");
    if (Lf < 2) {
        Value n_empty = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
        Value c_empty = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
        return {std::move(n_empty), std::move(c_empty)};
    }

    // Build (vals, probs) from the ecdf step structure.
    const size_t K = Lf - 1;
    std::vector<double> vals(K), probs(K);
    for (size_t k = 0; k < K; ++k) {
        vals[k]  = x.elemAsDouble(k + 1);
        probs[k] = f.elemAsDouble(k + 1) - f.elemAsDouble(k);
    }

    const double xmin = vals.front();
    const double xmax = vals.back();
    const double width = (xmax > xmin)
        ? (xmax - xmin) / static_cast<double>(m)
        : 1.0;  // degenerate single-value case: arbitrary width=1

    Value n_out = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
    Value c_out = Value::matrix(1, static_cast<size_t>(m), ValueType::DOUBLE, mr);
    double *nd = n_out.doubleDataMut();
    double *cd = c_out.doubleDataMut();

    for (int k = 0; k < m; ++k) {
        cd[k] = xmin + (k + 0.5) * width;
        nd[k] = 0.0;
    }
    if (xmax <= xmin) {
        // All-equal data: drop full mass into the centre bin.
        nd[m / 2] = 1.0 / width;
        return {std::move(n_out), std::move(c_out)};
    }

    // MATLAB convention: a value v at an edge belongs to the LOWER bin,
    // i.e. bin k contains (edge[k-1], edge[k]] in 1-based indexing.
    // Equivalent: `bin = ceil((v - xmin) / width) - 1` in 0-based.
    // Bug fix 2026-05-08: previous impl used `floor`, which sent
    // boundary values to the upper bin (off-by-one shift in counts).
    for (size_t k = 0; k < K; ++k) {
        const double v = vals[k];
        int bin = static_cast<int>(std::ceil((v - xmin) / width)) - 1;
        if (bin < 0)  bin = 0;
        if (bin >= m) bin = m - 1;
        nd[bin] += probs[k];
    }
    for (int k = 0; k < m; ++k) nd[k] /= width;
    return {std::move(n_out), std::move(c_out)};
}

// ── ecdf ─────────────────────────────────────────────────────────────
// Empirical CDF / survivor / cumulative-hazard. Optional Frequency
// weights; optional 95% confidence bounds.
//
// Output shapes match MATLAB R2025b: f and x are column vectors of
// length K+1 (K = number of distinct sample values). For cdf/survivor:
//   f[0] = 0 (cdf) or 1 (survivor); x[0] = min sample value.
// For cumulative hazard: same shape but f is the Nelson-Aalen
// estimator, NOT -log(1-cdf).

// `EcdfFull` forward-declared above. f/x/flo/fup are K+1 column vectors.
EcdfFull ecdf_full(const Value &y, const Value *freq, const std::string &function_mode, double alpha, bool want_bounds, std::pmr::memory_resource *mr)
{
    const size_t n = y.numel();
    const bool has_freq = (freq && freq->numel() == n);
    if (freq && freq->numel() != 0 && freq->numel() != n)
        throw Error("ecdf: Frequency length must match data length",
                    0, 0, "ecdf", "", "numkit:ecdf:freqsize");

    // Collect (value, weight) pairs, dropping NaNs. Sort by value.
    std::vector<std::pair<double, double>> vw;
    vw.reserve(n);
    double Wtotal = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double s = y.elemAsDouble(i);
        if (std::isnan(s)) continue;
        const double w = has_freq ? freq->elemAsDouble(i) : 1.0;
        if (w == 0.0) continue;
        vw.push_back({s, w});
        Wtotal += w;
    }
    EcdfFull R{};
    if (vw.empty() || Wtotal <= 0.0) {
        R.f   = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.x   = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.flo = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.fup = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return R;
    }
    std::sort(vw.begin(), vw.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    const double N = Wtotal;

    // Walk through sorted (v, w) and emit one row per distinct value.
    std::vector<double> fs, xs;
    std::vector<double> at_risk;   // n_i (for Nelson-Aalen and Greenwood)
    std::vector<double> events;    // d_i (events at this distinct value)
    fs.push_back(0.0);
    xs.push_back(vw.front().first);  // F = 0 at x = min(y)
    at_risk.push_back(N);
    events.push_back(0.0);

    double cum_w = 0.0;
    size_t i = 0;
    while (i < vw.size()) {
        size_t j = i + 1;
        double w_block = vw[i].second;
        while (j < vw.size() && vw[j].first == vw[i].first) {
            w_block += vw[j].second;
            ++j;
        }
        const double n_i = N - cum_w;       // at-risk just before this event
        cum_w += w_block;
        fs.push_back(cum_w / N);
        xs.push_back(vw[i].first);
        at_risk.push_back(n_i);
        events.push_back(w_block);
        i = j;
    }

    const size_t L = fs.size();

    // Apply Function mode.
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    const std::string mode = lower(function_mode);
    std::vector<double> ff(L);
    if (mode == "cdf" || mode.empty()) {
        for (size_t k = 0; k < L; ++k) ff[k] = fs[k];
    } else if (mode == "survivor") {
        for (size_t k = 0; k < L; ++k) ff[k] = 1.0 - fs[k];
    } else if (mode == "cumulative hazard" || mode == "cumhazard") {
        // Nelson-Aalen estimator: H(x) = sum over t_i ≤ x of d_i / n_i.
        ff[0] = 0.0;
        double H = 0.0;
        for (size_t k = 1; k < L; ++k) {
            if (at_risk[k] > 0.0) H += events[k] / at_risk[k];
            ff[k] = H;
        }
    } else {
        throw Error("ecdf: unknown Function mode '" + mode + "'",
                    0, 0, "ecdf", "", "numkit:ecdf:badmode");
    }

    R.f = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    R.x = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    {
        double *fd = R.f.doubleDataMut();
        double *xd = R.x.doubleDataMut();
        for (size_t k = 0; k < L; ++k) { fd[k] = ff[k]; xd[k] = xs[k]; }
    }

    if (!want_bounds) {
        R.flo = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.fup = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return R;
    }

    // Greenwood-style binomial Wald CI for cdf / survivor; analogous
    // log-transform for cumulative hazard. Match MATLAB R2025b: first
    // and last rows return NaN bounds.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    R.flo = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    R.fup = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    double *lo = R.flo.doubleDataMut();
    double *hi = R.fup.doubleDataMut();

    // z = -norminv(α/2). For α=0.05 → ~1.959964.
    const double z = std::sqrt(2.0) * [&]{
        double e = 1.0 - alpha;
        for (int it = 0; it < 50; ++it) {
            const double f = std::erf(e) - (1.0 - alpha);
            const double fp = (2.0 / std::sqrt(3.14159265358979323846))
                              * std::exp(-e * e);
            e -= f / fp;
        }
        return e;
    }();
    for (size_t k = 0; k < L; ++k) {
        if (k == 0 || k == L - 1) { lo[k] = nan; hi[k] = nan; continue; }
        const double F = ff[k];
        const double se = std::sqrt(F * (1.0 - F) / N);
        double l = F - z * se;
        double h = F + z * se;
        if (l < 0.0) l = 0.0;
        if (h > 1.0) h = 1.0;
        lo[k] = l;
        hi[k] = h;
    }
    return R;
}

// Backward-compat 1-arg form.
std::tuple<Value, Value>
ecdf(const Value &y, std::pmr::memory_resource *mr)
{
    auto R = ecdf_full(y, nullptr, "cdf", 0.05, false, mr);
    return {std::move(R.f), std::move(R.x)};
}

} // namespace numkit::stats
