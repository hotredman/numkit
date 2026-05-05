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

std::tuple<Value, Value, Value, Value>
regress(std::pmr::memory_resource *mr, const Value &y, const Value &X,
        double alpha)
{
    const size_t N = y.numel();
    const size_t p = X.dims().cols();
    if (X.dims().rows() != N || N == 0 || p == 0)
        throw Error("regress: X must be N×p with same N as y",
                    0, 0, "regress", "", "m:regress:size");

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
                    0, 0, "regress", "", "m:regress:rank");

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

    // Compute (XtX)^{-1} via solving XtX·M = I, column-by-column.
    std::vector<double> invDiag(p, 0.0);
    {
        std::vector<double> ej(p), zj(p), mj(p);
        for (size_t j = 0; j < p; ++j) {
            std::fill(ej.begin(), ej.end(), 0.0);
            ej[j] = 1.0;
            fwd_solve(L.data(), zj.data(), ej.data(), p);
            back_solve(L.data(), mj.data(), zj.data(), p);
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
        tcrit = tinv(mr, pV, dfErr).toScalar();
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
        const double cdf = fcdf(mr, fv, dfModel, dfErr).toScalar();
        pF = std::max(0.0, 1.0 - cdf);
    }
    Value statsV = Value::matrix(1, 4, ValueType::DOUBLE, mr);
    double *sd = statsV.doubleDataMut();
    sd[0] = R2; sd[1] = F; sd[2] = pF; sd[3] = sigma2;

    return {std::move(bV), std::move(bintV), std::move(rV), std::move(statsV)};
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
                    0, 0, "regress", "", "m:regress:nargin");
    const double alpha = (args.size() >= 3 && !args[2].isEmpty())
                         ? args[2].toScalar() : 0.05;
    auto [b, bint, r, stats] = regress(ctx.engine->resource(),
                                        args[0], args[1], alpha);
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(bint);
    if (nargout > 2) outs[2] = std::move(r);
    if (nargout > 3) outs[3] = Value::matrix(0, 0, ValueType::DOUBLE,
                                             ctx.engine->resource());  // rint placeholder
    if (nargout > 4) outs[4] = std::move(stats);
}

} // namespace detail
} // namespace numkit::stats
