// toolboxes/stats/src/descriptive/descriptive_extras.cpp
//
// Descriptive stats extras (B2): bounds, iqr, maxk, mink, rmse.

#include <numkit/stats/descriptive/descriptive.hpp>
#include <numkit/stats/distributions/students_t.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>
#include <numkit/ops/reductions.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <vector>

#include "descriptive_extras_detail.hpp"

namespace numkit::stats {


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


// Result struct for the extended ksdensity API. Forward-declared also
// above the engine-adapter `ksdensity_reg` (in the outer namespace).

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

// Forward declaration for ksdensity_full. KsdensityFull is defined above.

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
