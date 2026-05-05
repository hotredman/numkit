// libs/stats/src/mvdist/mvdist.cpp
//
// mvnpdf / mnpdf — closed-form multivariate PDFs/PMFs.

#include <numkit/stats/mvdist/mvdist.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

constexpr double kLog2Pi = 1.8378770664093454835606594728112352;

// Cholesky factor of d×d positive-definite matrix in-place into L
// (lower-triangular, column-major). Returns true on success.
// `M` is the column-major d² array (read-only); writes lower triangle to L.
bool cholesky(const double *M, double *L, size_t d)
{
    for (size_t i = 0; i < d * d; ++i) L[i] = 0.0;
    for (size_t j = 0; j < d; ++j) {
        // diagonal
        double s = M[j + j * d];
        for (size_t k = 0; k < j; ++k) s -= L[j + k * d] * L[j + k * d];
        if (s <= 0.0) return false;  // not PSD
        const double Ljj = std::sqrt(s);
        L[j + j * d] = Ljj;
        // below-diagonal entries
        for (size_t i = j + 1; i < d; ++i) {
            double t = M[i + j * d];
            for (size_t k = 0; k < j; ++k) t -= L[i + k * d] * L[j + k * d];
            L[i + j * d] = t / Ljj;
        }
    }
    return true;
}

// Forward substitution: L · z = b → z, where L is lower-triangular d×d
// (column-major). z and b can overlap.
void forward_solve(const double *L, double *z, const double *b, size_t d)
{
    for (size_t i = 0; i < d; ++i) {
        double s = b[i];
        for (size_t k = 0; k < i; ++k) s -= L[i + k * d] * z[k];
        z[i] = s / L[i + i * d];
    }
}

} // anonymous

Value mvnpdf(std::pmr::memory_resource *mr, const Value &X,
             const Value &mu, const Value &Sigma)
{
    // Determine dimensionality d. X is N×d (or 1×d for a single point).
    if (X.numel() == 0)
        return Value::matrix(0, 1, ValueType::DOUBLE, mr);
    const size_t Nrows = X.dims().rows();
    const size_t d     = X.dims().cols();
    if (d == 0)
        return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    // Build mu vector (length d). Empty → zeros.
    std::vector<double> muv(d, 0.0);
    if (!mu.isEmpty()) {
        if (mu.numel() != d)
            throw Error("mvnpdf: mu must have d entries",
                        0, 0, "mvnpdf", "", "m:mvnpdf:mu");
        for (size_t i = 0; i < d; ++i) muv[i] = mu.elemAsDouble(i);
    }

    // Build Sigma matrix (d×d). Empty → identity.
    std::vector<double> Sig(d * d, 0.0);
    if (Sigma.isEmpty()) {
        for (size_t i = 0; i < d; ++i) Sig[i + i * d] = 1.0;
    } else if (Sigma.numel() == d) {
        // 1×d row treated as diagonal of Sigma (MATLAB convention).
        for (size_t i = 0; i < d; ++i) Sig[i + i * d] = Sigma.elemAsDouble(i);
    } else {
        if (Sigma.dims().rows() != d || Sigma.dims().cols() != d)
            throw Error("mvnpdf: Sigma must be d×d (or 1×d diag)",
                        0, 0, "mvnpdf", "", "m:mvnpdf:Sigma");
        for (size_t j = 0; j < d; ++j)
            for (size_t i = 0; i < d; ++i)
                Sig[i + j * d] = Sigma.elemAsDouble(i + j * d);
    }

    std::vector<double> L(d * d, 0.0);
    if (!cholesky(Sig.data(), L.data(), d))
        throw Error("mvnpdf: Sigma must be positive definite",
                    0, 0, "mvnpdf", "", "m:mvnpdf:psd");

    double sumLogDiag = 0.0;
    for (size_t i = 0; i < d; ++i) sumLogDiag += std::log(L[i + i * d]);

    // logpdf = -0.5·d·log(2π) - sum(log(diag(L))) - 0.5·||L⁻¹·(x-μ)||²
    Value out = Value::matrix(Nrows, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    std::vector<double> dx(d), z(d);
    for (size_t row = 0; row < Nrows; ++row) {
        for (size_t i = 0; i < d; ++i)
            dx[i] = X.elemAsDouble(row + i * Nrows) - muv[i];
        forward_solve(L.data(), z.data(), dx.data(), d);
        double q = 0.0;
        for (size_t i = 0; i < d; ++i) q += z[i] * z[i];
        const double logpdf = -0.5 * double(d) * kLog2Pi - sumLogDiag - 0.5 * q;
        od[row] = std::exp(logpdf);
    }
    return out;
}

Value mnpdf(std::pmr::memory_resource *mr, const Value &X, const Value &P)
{
    // X is 1×k or N×k of integer counts. P is 1×k probability vector.
    const size_t k = P.numel();
    if (k == 0) return Value::matrix(0, 1, ValueType::DOUBLE, mr);
    const size_t Nrows = X.dims().rows();
    const size_t kx    = X.dims().cols();
    if (kx != k && X.numel() != k)
        throw Error("mnpdf: X must have the same number of columns as P",
                    0, 0, "mnpdf", "", "m:mnpdf:size");

    std::vector<double> p(k);
    for (size_t j = 0; j < k; ++j) p[j] = P.elemAsDouble(j);

    const size_t Nout = (X.numel() == k) ? 1 : Nrows;
    Value out = Value::matrix(Nout, 1, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (size_t row = 0; row < Nout; ++row) {
        double n = 0.0;
        std::vector<double> x(k);
        for (size_t j = 0; j < k; ++j) {
            x[j] = (X.numel() == k) ? X.elemAsDouble(j)
                                    : X.elemAsDouble(row + j * Nrows);
            n += x[j];
        }
        // log P = lgamma(n+1) − Σ lgamma(x_j+1) + Σ x_j·log(p_j)
        double lp = std::lgamma(n + 1.0);
        for (size_t j = 0; j < k; ++j) {
            lp -= std::lgamma(x[j] + 1.0);
            if (p[j] > 0.0)      lp += x[j] * std::log(p[j]);
            else if (x[j] != 0.0) { lp = -std::numeric_limits<double>::infinity(); break; }
        }
        od[row] = std::exp(lp);
    }
    return out;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void mvnpdf_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mvnpdf: requires X[, mu, Sigma]",
                    0, 0, "mvnpdf", "", "m:mvnpdf:nargin");
    auto *mr = ctx.engine->resource();
    Value empty = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &mu  = (args.size() >= 2) ? args[1] : empty;
    const Value &sig = (args.size() >= 3) ? args[2] : empty;
    outs[0] = mvnpdf(mr, args[0], mu, sig);
}

void mnpdf_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mnpdf: requires (X, P)",
                    0, 0, "mnpdf", "", "m:mnpdf:nargin");
    outs[0] = mnpdf(ctx.engine->resource(), args[0], args[1]);
}

} // namespace detail
} // namespace numkit::stats
