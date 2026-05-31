// libs/stats/src/regress/regress.cpp

#include <numkit/stats/regress/regress.hpp>

#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/distributions/fisher_f.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

// Cholesky factor of d×d symmetric PSD matrix M (column-major)
// into L (lower-triangular, column-major). Returns true on success.
bool cholesky(const double *M, double *L, size_t d)
{
    for (size_t i = 0; i < d * d; ++i) L[i] = 0.0;
    for (size_t j = 0; j < d; ++j) {
        double s = M[j + j * d];
        for (size_t k = 0; k < j; ++k) s -= L[j + k * d] * L[j + k * d];
        if (s <= 0.0) return false;
        const double Ljj = std::sqrt(s);
        L[j + j * d] = Ljj;
        for (size_t i = j + 1; i < d; ++i) {
            double t = M[i + j * d];
            for (size_t k = 0; k < j; ++k) t -= L[i + k * d] * L[j + k * d];
            L[i + j * d] = t / Ljj;
        }
    }
    return true;
}

// Solve L · z = b (forward substitution).
void fwd_solve(const double *L, double *z, const double *b, size_t d)
{
    for (size_t i = 0; i < d; ++i) {
        double s = b[i];
        for (size_t k = 0; k < i; ++k) s -= L[i + k * d] * z[k];
        z[i] = s / L[i + i * d];
    }
}

// Solve L^T · x = z (backward substitution).
void back_solve(const double *L, double *x, const double *z, size_t d)
{
    for (size_t i = d; i-- > 0;) {
        double s = z[i];
        for (size_t k = i + 1; k < d; ++k) s -= L[k + i * d] * x[k];
        x[i] = s / L[i + i * d];
    }
}

} // anonymous

// 5-output form: [b, bint, r, rint, stats]. rint = residual CI for
// outlier detection — added 2026-05-08.
std::tuple<Value, Value, Value, Value, Value>
regress_full(const Value &y, const Value &X, double alpha, std::pmr::memory_resource *mr)
{
    const size_t N = y.numel();
    const size_t p = X.dims().cols();
    if (X.dims().rows() != N || N == 0 || p == 0)
        throw Error("regress: X must be N×p with same N as y",
                    0, 0, "regress", "", "numkit:regress:size");

    // Build XtX (p×p) and Xty (p×1) using column-major X.
    std::vector<double> XtX(p * p, 0.0);
    std::vector<double> Xty(p, 0.0);
    for (size_t j = 0; j < p; ++j) {
        for (size_t i = 0; i < N; ++i) {
            const double xij = X.elemAsDouble(i + j * N);
            Xty[j] += xij * y.elemAsDouble(i);
            for (size_t k = j; k < p; ++k) {
                const double xik = X.elemAsDouble(i + k * N);
                XtX[k + j * p] += xij * xik;
            }
        }
    }
    // Mirror upper triangle.
    for (size_t j = 0; j < p; ++j)
        for (size_t k = 0; k < j; ++k)
            XtX[k + j * p] = XtX[j + k * p];

    // Cholesky factor.
    std::vector<double> L(p * p, 0.0);
    if (!cholesky(XtX.data(), L.data(), p))
        throw Error("regress: design matrix is rank-deficient",
                    0, 0, "regress", "", "numkit:regress:rank");

    // Solve XtX β = Xty:  L z = Xty,  L^T β = z.
    std::vector<double> beta(p), z(p);
    fwd_solve(L.data(), z.data(), Xty.data(), p);
    back_solve(L.data(), beta.data(), z.data(), p);

    // Residuals r = y - X β.
    std::vector<double> r(N);
    double SSR = 0.0;
    for (size_t i = 0; i < N; ++i) {
        double pred = 0.0;
        for (size_t j = 0; j < p; ++j) pred += X.elemAsDouble(i + j * N) * beta[j];
        const double ri = y.elemAsDouble(i) - pred;
        r[i] = ri;
        SSR += ri * ri;
    }
    const double dfErr = double(N) - double(p);
    const double sigma2 = (dfErr > 0) ? SSR / dfErr
                                      : std::numeric_limits<double>::quiet_NaN();

    // Compute the FULL (XtX)^{-1} (p×p, column-major) so we can compute
    // both the diagonal (for bint SE) and the leverage h_ii = X·M·X'
    // for rint.
    std::vector<double> invMat(p * p, 0.0);
    std::vector<double> invDiag(p, 0.0);
    {
        std::vector<double> ej(p), zj(p), mj(p);
        for (size_t j = 0; j < p; ++j) {
            std::fill(ej.begin(), ej.end(), 0.0);
            ej[j] = 1.0;
            fwd_solve(L.data(), zj.data(), ej.data(), p);
            back_solve(L.data(), mj.data(), zj.data(), p);
            for (size_t i = 0; i < p; ++i) invMat[i + j * p] = mj[i];
            invDiag[j] = mj[j];
        }
    }

    // Confidence intervals for β.
    Value bV    = Value::matrix(p, 1, ValueType::DOUBLE, mr);
    Value bintV = Value::matrix(p, 2, ValueType::DOUBLE, mr);
    Value rV    = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *bd = bV.doubleDataMut();
    double *bid = bintV.doubleDataMut();
    double *rd = rV.doubleDataMut();
    double tcrit = 0.0;
    if (dfErr > 0) {
        Value pV = Value::scalar(1.0 - alpha / 2.0, mr);
        tcrit = tinv(pV, dfErr, mr).toScalar();
    }
    for (size_t j = 0; j < p; ++j) {
        bd[j] = beta[j];
        const double se = (sigma2 == sigma2 && invDiag[j] > 0)
                          ? std::sqrt(sigma2 * invDiag[j])
                          : std::numeric_limits<double>::quiet_NaN();
        bid[j]     = beta[j] - tcrit * se;
        bid[j + p] = beta[j] + tcrit * se;
    }
    for (size_t i = 0; i < N; ++i) rd[i] = r[i];

    // stats = [R², F, p_F, sigma²]
    double yMean = 0.0;
    for (size_t i = 0; i < N; ++i) yMean += y.elemAsDouble(i);
    yMean /= double(N);
    double SStot = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = y.elemAsDouble(i) - yMean;
        SStot += d * d;
    }
    const double R2 = (SStot > 0.0) ? 1.0 - SSR / SStot
                                    : std::numeric_limits<double>::quiet_NaN();
    const double dfModel = double(p) - 1.0;
    const double F = (dfModel > 0.0 && dfErr > 0.0 && (1.0 - R2) > 0.0)
                     ? (R2 / dfModel) / ((1.0 - R2) / dfErr)
                     : std::numeric_limits<double>::quiet_NaN();
    double pF = std::numeric_limits<double>::quiet_NaN();
    if (!std::isnan(F)) {
        Value fv = Value::scalar(F, mr);
        const double cdf = fcdf(fv, dfModel, dfErr, mr).toScalar();
        pF = std::max(0.0, 1.0 - cdf);
    }
    Value statsV = Value::matrix(1, 4, ValueType::DOUBLE, mr);
    double *sd = statsV.doubleDataMut();
    sd[0] = R2; sd[1] = F; sd[2] = pF; sd[3] = sigma2;

    // rint: 100·(1-α)% CI for residuals, used to diagnose outliers
    // (Chatterjee & Hadi 1986 eq.14; Belsley, Kuh & Welsch 1980 eq.2.26).
    // MATLAB uses the LEAVE-ONE-OUT variance estimate σ_i and (nu-1) dof,
    // NOT a flat σ·sqrt(1-h) with nu dof:
    //   h_ii  = X[i,:]·(XtX)^{-1}·X[i,:]'                  (leverage)
    //   σ_i   = sqrt(max(0, nu·s²/(nu-1) - r_i²/((nu-1)(1-h_ii))))
    //   ser   = sqrt(1-h_ii)·σ_i,   t = tinv(1-α/2, nu-1)
    //   rint  = r_i ± t·ser
    // When h_ii ≥ 1 (perfect leverage) the CI degenerates to [r_i, r_i].
    Value rintV = Value::matrix(N, 2, ValueType::DOUBLE, mr);
    double *rid = rintV.doubleDataMut();
    const double nu   = dfErr;
    const double s2   = sigma2;
    const double rmse = (s2 == s2 && s2 > 0.0) ? std::sqrt(s2) : 0.0;
    double tcritR = tcrit;                       // nu<=1 fallback: nu dof
    if (nu > 1.0) {
        Value pV = Value::scalar(1.0 - alpha / 2.0, mr);
        tcritR = tinv(pV, nu - 1.0, mr).toScalar();
    }
    const double epsThresh = 1.4901161193847656e-08;   // sqrt(eps)
    for (size_t i = 0; i < N; ++i) {
        // h_ii = sum_{j,k} X[i,j] · invMat[j,k] · X[i,k].
        double hii = 0.0;
        for (size_t j = 0; j < p; ++j) {
            double row = 0.0;
            for (size_t k = 0; k < p; ++k)
                row += invMat[j + k * p] * X.elemAsDouble(i + k * N);
            hii += X.elemAsDouble(i + j * N) * row;
        }
        const double oneMinusH = 1.0 - hii;
        double ser = 0.0;
        if (oneMinusH > epsThresh && std::isfinite(oneMinusH)) {
            if (nu > 1.0) {
                const double denom  = (nu - 1.0) * oneMinusH;
                const double sigmai = std::sqrt(std::max(0.0,
                    (nu * s2 / (nu - 1.0)) - (r[i] * r[i]) / denom));
                ser = std::sqrt(oneMinusH) * sigmai;
            } else {
                ser = std::sqrt(oneMinusH) * rmse;
            }
        }
        rid[i]     = r[i] - tcritR * ser;
        rid[i + N] = r[i] + tcritR * ser;
    }

    return {std::move(bV), std::move(bintV), std::move(rV),
            std::move(rintV), std::move(statsV)};
}

// Backward-compat 4-tuple wrapper (for legacy callers; rint dropped).
std::tuple<Value, Value, Value, Value>
regress(const Value &y, const Value &X, double alpha, std::pmr::memory_resource *mr)
{
    auto [b, bint, r, rint, stats] = regress_full(y, X, alpha, mr);
    (void)rint;
    return {std::move(b), std::move(bint), std::move(r), std::move(stats)};
}

std::tuple<Value, Value, Value, Value>
lscov(const Value &A, const Value &b, const Value &w, std::pmr::memory_resource *mr)
{
    const size_t N = b.numel();
    const size_t p = A.dims().cols();
    if (A.dims().rows() != N || N == 0 || p == 0)
        throw Error("lscov: A must be N×p with same N as b",
                    0, 0, "lscov", "", "numkit:lscov:size");

    const bool weighted = !w.isEmpty();
    if (weighted) {
        if (w.numel() == N * N)
            throw Error("lscov: full covariance V not yet supported",
                        0, 0, "lscov", "", "numkit:lscov:fullV");
        if (w.numel() != N)
            throw Error("lscov: w must be a length-N vector",
                        0, 0, "lscov", "", "numkit:lscov:w");
    }

    auto wi = [&](size_t i) {
        return weighted ? w.elemAsDouble(i) : 1.0;
    };

    // Build XtWX and XtWy with weights baked in (W = diag(w)).
    std::vector<double> XtWX(p * p, 0.0);
    std::vector<double> XtWy(p, 0.0);
    for (size_t i = 0; i < N; ++i) {
        const double wii = wi(i);
        const double bi  = b.elemAsDouble(i);
        for (size_t j = 0; j < p; ++j) {
            const double xij = A.elemAsDouble(i + j * N);
            XtWy[j] += wii * xij * bi;
            for (size_t k = j; k < p; ++k) {
                const double xik = A.elemAsDouble(i + k * N);
                XtWX[k + j * p] += wii * xij * xik;
            }
        }
    }
    for (size_t j = 0; j < p; ++j)
        for (size_t k = 0; k < j; ++k)
            XtWX[k + j * p] = XtWX[j + k * p];

    std::vector<double> L(p * p, 0.0);
    if (!cholesky(XtWX.data(), L.data(), p))
        throw Error("lscov: design matrix is rank-deficient",
                    0, 0, "lscov", "", "numkit:lscov:rank");

    std::vector<double> beta(p), z(p);
    fwd_solve(L.data(), z.data(), XtWy.data(), p);
    back_solve(L.data(), beta.data(), z.data(), p);

    // weighted residuals + SSR
    double SSRw = 0.0;
    for (size_t i = 0; i < N; ++i) {
        double pred = 0.0;
        for (size_t j = 0; j < p; ++j) pred += A.elemAsDouble(i + j * N) * beta[j];
        const double ri = b.elemAsDouble(i) - pred;
        SSRw += wi(i) * ri * ri;
    }
    const double dfErr = double(N) - double(p);
    const double mse = (dfErr > 0.0) ? SSRw / dfErr
                                     : std::numeric_limits<double>::quiet_NaN();

    // (XtWX)⁻¹ then S = mse · (XtWX)⁻¹
    std::vector<double> Sm(p * p, 0.0);
    {
        std::vector<double> ej(p), zj(p), mj(p);
        for (size_t j = 0; j < p; ++j) {
            std::fill(ej.begin(), ej.end(), 0.0);
            ej[j] = 1.0;
            fwd_solve(L.data(), zj.data(), ej.data(), p);
            back_solve(L.data(), mj.data(), zj.data(), p);
            for (size_t i = 0; i < p; ++i)
                Sm[i + j * p] = mse * mj[i];
        }
    }

    Value xV    = Value::matrix(p, 1, ValueType::DOUBLE, mr);
    Value stdxV = Value::matrix(p, 1, ValueType::DOUBLE, mr);
    Value mseV  = Value::scalar(mse, mr);
    Value SV    = Value::matrix(p, p, ValueType::DOUBLE, mr);
    double *xd  = xV.doubleDataMut();
    double *sxd = stdxV.doubleDataMut();
    double *Sd  = SV.doubleDataMut();
    for (size_t j = 0; j < p; ++j) {
        xd[j]  = beta[j];
        sxd[j] = std::sqrt(std::max(Sm[j + j * p], 0.0));
    }
    for (size_t i = 0; i < p * p; ++i) Sd[i] = Sm[i];

    return {std::move(xV), std::move(stdxV), std::move(mseV), std::move(SV)};
}

Value ridge(const Value &y, const Value &X, const Value &kVec, bool scaled, std::pmr::memory_resource *mr)
{
    const size_t N = y.numel();
    const size_t p = X.dims().cols();
    if (X.dims().rows() != N || N == 0 || p == 0)
        throw Error("ridge: X must be N×p with same N as y",
                    0, 0, "ridge", "", "numkit:ridge:size");
    const size_t Nk = kVec.numel();
    if (Nk == 0)
        throw Error("ridge: k must be non-empty",
                    0, 0, "ridge", "", "numkit:ridge:k");

    // Center y, X by column mean.
    double yMean = 0.0;
    for (size_t i = 0; i < N; ++i) yMean += y.elemAsDouble(i);
    yMean /= double(N);
    std::vector<double> yc(N);
    for (size_t i = 0; i < N; ++i) yc[i] = y.elemAsDouble(i) - yMean;

    std::vector<double> xMean(p, 0.0), xStd(p, 0.0);
    for (size_t j = 0; j < p; ++j) {
        for (size_t i = 0; i < N; ++i) xMean[j] += X.elemAsDouble(i + j * N);
        xMean[j] /= double(N);
    }
    std::vector<double> Xs(N * p);
    for (size_t j = 0; j < p; ++j) {
        double sq = 0.0;
        for (size_t i = 0; i < N; ++i) {
            const double d = X.elemAsDouble(i + j * N) - xMean[j];
            Xs[i + j * N] = d;
            sq += d * d;
        }
        // Sample std with N-1 normalisation (matches MATLAB ridge).
        const double s = (N > 1) ? std::sqrt(sq / double(N - 1)) : 1.0;
        xStd[j] = s;
        if (s > 0.0)
            for (size_t i = 0; i < N; ++i) Xs[i + j * N] /= s;
    }

    // X_s' X_s and X_s' y_c.
    std::vector<double> XtX(p * p, 0.0);
    std::vector<double> Xty(p, 0.0);
    for (size_t i = 0; i < N; ++i) {
        const double yi = yc[i];
        for (size_t j = 0; j < p; ++j) {
            const double xij = Xs[i + j * N];
            Xty[j] += xij * yi;
            for (size_t kk = j; kk < p; ++kk) {
                const double xik = Xs[i + kk * N];
                XtX[kk + j * p] += xij * xik;
            }
        }
    }
    for (size_t j = 0; j < p; ++j)
        for (size_t kk = 0; kk < j; ++kk)
            XtX[kk + j * p] = XtX[j + kk * p];

    // Allocate output
    const size_t outRows = scaled ? p : (p + 1);
    Value B = Value::matrix(outRows, Nk, ValueType::DOUBLE, mr);
    double *Bd = B.doubleDataMut();

    std::vector<double> M(p * p), L(p * p), beta(p), z(p);
    for (size_t kIdx = 0; kIdx < Nk; ++kIdx) {
        const double k = kVec.elemAsDouble(kIdx);
        // M = XtX + k*I
        for (size_t j = 0; j < p * p; ++j) M[j] = XtX[j];
        for (size_t j = 0; j < p; ++j) M[j + j * p] += k;
        if (!cholesky(M.data(), L.data(), p))
            throw Error("ridge: regularised normal equations not PD",
                        0, 0, "ridge", "", "numkit:ridge:psd");
        fwd_solve(L.data(), z.data(), Xty.data(), p);
        back_solve(L.data(), beta.data(), z.data(), p);
        if (scaled) {
            for (size_t j = 0; j < p; ++j) Bd[j + kIdx * outRows] = beta[j];
        } else {
            // Rescale to original units; prepend intercept = mean(y) - Σ β_j·mean(X_j).
            double intercept = yMean;
            for (size_t j = 0; j < p; ++j) {
                const double bj = (xStd[j] > 0.0) ? beta[j] / xStd[j] : 0.0;
                Bd[(j + 1) + kIdx * outRows] = bj;
                intercept -= bj * xMean[j];
            }
            Bd[0 + kIdx * outRows] = intercept;
        }
    }
    return B;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void regress_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("regress: requires (y, X[, alpha])",
                    0, 0, "regress", "", "numkit:regress:nargin");
    const double alpha = (args.size() >= 3 && !args[2].isEmpty())
                         ? args[2].toScalar() : 0.05;
    auto [b, bint, r, rint, stats] = regress_full(args[0], args[1], alpha, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(bint);
    if (nargout > 2) outs[2] = std::move(r);
    if (nargout > 3) outs[3] = std::move(rint);
    if (nargout > 4) outs[4] = std::move(stats);
}

void ridge_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ridge: requires (y, X, k[, scaled])",
                    0, 0, "ridge", "", "numkit:ridge:nargin");
    bool scaled = true;
    if (args.size() >= 4 && !args[3].isEmpty())
        scaled = (args[3].toScalar() != 0.0);
    outs[0] = ridge(args[0], args[1], args[2], scaled, ctx.engine->resource());
}

void lscov_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lscov: requires (A, b[, w])",
                    0, 0, "lscov", "", "numkit:lscov:nargin");
    auto *mr = ctx.engine->resource();
    Value w_empty = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &w = (args.size() >= 3) ? args[2] : w_empty;
    auto [x, stdx, mse, S] = lscov(args[0], args[1], w, mr);
    outs[0] = std::move(x);
    if (nargout > 1) outs[1] = std::move(stdx);
    if (nargout > 2) outs[2] = std::move(mse);
    if (nargout > 3) outs[3] = std::move(S);
}

} // namespace detail
} // namespace numkit::stats
