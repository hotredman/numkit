// libs/stats/src/dim/pca.cpp
//
// PCA via classical Jacobi eigenvalue iteration on the sample
// covariance matrix. Self-contained — no dependence on a linalg
// SVD/eig infrastructure (which is still deferred at the runtime level).

#include <numkit/stats/dim/pca.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace numkit::stats {

namespace {

// Read X (N×D, column-major) into a flat row-major buffer.
std::vector<double> read_rows(const Value &X) {
    const size_t N = X.dims().rows();
    const size_t D = X.dims().cols();
    std::vector<double> out(N * D);
    for (size_t r = 0; r < N; ++r)
        for (size_t c = 0; c < D; ++c)
            out[r * D + c] = X.elemAsDouble(c * N + r);
    return out;
}

// Jacobi eigendecomposition of D×D symmetric matrix A. On exit:
//   A holds the eigenvalues on its diagonal (off-diagonals near zero)
//   V is D×D orthogonal whose columns are the eigenvectors.
// Both stored row-major.
void jacobi(std::vector<double> &A, std::vector<double> &V, size_t D) {
    V.assign(D * D, 0.0);
    for (size_t i = 0; i < D; ++i) V[i * D + i] = 1.0;

    const int max_sweeps = 50;
    const double tol = 1e-12;

    for (int sweep = 0; sweep < max_sweeps; ++sweep) {
        // Sum of squared off-diagonals.
        double off = 0.0;
        for (size_t p = 0; p < D; ++p)
            for (size_t q = p + 1; q < D; ++q)
                off += A[p * D + q] * A[p * D + q];
        if (off < tol) break;

        for (size_t p = 0; p < D - 1; ++p) {
            for (size_t q = p + 1; q < D; ++q) {
                const double app = A[p * D + p];
                const double aqq = A[q * D + q];
                const double apq = A[p * D + q];
                if (std::fabs(apq) < 1e-15) continue;

                // Compute rotation angle.
                const double theta = (aqq - app) / (2.0 * apq);
                double t;
                if (theta == 0.0) t = 1.0;
                else {
                    const double sgn = (theta > 0) ? 1.0 : -1.0;
                    t = sgn / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
                }
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;

                // Apply rotation to A.
                A[p * D + p] = app - t * apq;
                A[q * D + q] = aqq + t * apq;
                A[p * D + q] = 0.0;
                A[q * D + p] = 0.0;
                for (size_t r = 0; r < D; ++r) {
                    if (r == p || r == q) continue;
                    const double arp = A[r * D + p], arq = A[r * D + q];
                    A[r * D + p] = c * arp - s * arq;
                    A[p * D + r] = A[r * D + p];
                    A[r * D + q] = c * arq + s * arp;
                    A[q * D + r] = A[r * D + q];
                }
                // Accumulate rotation into V.
                for (size_t r = 0; r < D; ++r) {
                    const double vrp = V[r * D + p], vrq = V[r * D + q];
                    V[r * D + p] = c * vrp - s * vrq;
                    V[r * D + q] = c * vrq + s * vrp;
                }
            }
        }
    }
}

} // anonymous

std::tuple<Value, Value, Value, Value, Value, Value>
pca(std::pmr::memory_resource *mr, const Value &X)
{
    const size_t N = X.dims().rows();
    const size_t D = X.dims().cols();
    if (N < 2 || D < 1)
        throw Error("pca: need at least 2 rows and 1 column",
                    0, 0, "pca", "", "m:pca:size");

    std::vector<double> Xv = read_rows(X);

    // Mean of each column.
    std::vector<double> mu(D, 0.0);
    for (size_t r = 0; r < N; ++r)
        for (size_t c = 0; c < D; ++c) mu[c] += Xv[r * D + c];
    for (auto &m : mu) m /= double(N);

    // Centred X.
    std::vector<double> Xc(N * D);
    for (size_t r = 0; r < N; ++r)
        for (size_t c = 0; c < D; ++c) Xc[r * D + c] = Xv[r * D + c] - mu[c];

    // Sample covariance (D×D, divisor n-1).
    std::vector<double> C(D * D, 0.0);
    for (size_t r = 0; r < N; ++r) {
        for (size_t a = 0; a < D; ++a)
            for (size_t b = 0; b < D; ++b)
                C[a * D + b] += Xc[r * D + a] * Xc[r * D + b];
    }
    const double inv_n = 1.0 / double(N - 1);
    for (auto &c : C) c *= inv_n;

    // Jacobi eigendecomposition: A·V = V·Λ.
    std::vector<double> Vmat;
    jacobi(C, Vmat, D);

    // Extract eigenvalues from C diagonal; sort descending.
    std::vector<double> latent(D);
    std::vector<int> order(D);
    for (size_t i = 0; i < D; ++i) { latent[i] = C[i * D + i]; order[i] = (int)i; }
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return latent[a] > latent[b]; });

    // Reorder eigenvalues + eigenvectors.
    std::vector<double> latent_s(D);
    std::vector<double> coeff(D * D, 0.0);  // D×D row-major; cols = eigenvectors
    for (size_t i = 0; i < D; ++i) {
        const int src = order[i];
        latent_s[i] = latent[src];
        for (size_t r = 0; r < D; ++r) coeff[r * D + i] = Vmat[r * D + src];
    }

    // score = Xc · coeff (N×D × D×D = N×D)
    std::vector<double> score(N * D, 0.0);
    for (size_t r = 0; r < N; ++r)
        for (size_t k = 0; k < D; ++k) {
            double s = 0.0;
            for (size_t j = 0; j < D; ++j) s += Xc[r * D + j] * coeff[j * D + k];
            score[r * D + k] = s;
        }

    // explained: percent of variance.
    double total = 0.0; for (double v : latent_s) total += v;
    std::vector<double> explained(D, 0.0);
    if (total > 0.0)
        for (size_t i = 0; i < D; ++i) explained[i] = 100.0 * latent_s[i] / total;

    // tsquared: Hotelling's T² = Σ_k (score_ik² / latent_k) per row.
    std::vector<double> tsq(N, 0.0);
    for (size_t r = 0; r < N; ++r) {
        double s = 0.0;
        for (size_t k = 0; k < D; ++k)
            if (latent_s[k] > 1e-15)
                s += score[r * D + k] * score[r * D + k] / latent_s[k];
        tsq[r] = s;
    }

    // Pack outputs (column-major).
    auto pack_NxM = [&](const std::vector<double> &src, size_t rows, size_t cols) {
        Value v = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
        double *p = v.doubleDataMut();
        for (size_t r = 0; r < rows; ++r)
            for (size_t c = 0; c < cols; ++c) p[c * rows + r] = src[r * cols + c];
        return v;
    };

    Value coeff_v = pack_NxM(coeff, D, D);
    Value score_v = pack_NxM(score, N, D);
    Value latent_v = Value::matrix(D, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < D; ++i) latent_v.doubleDataMut()[i] = latent_s[i];
    Value tsq_v = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < N; ++i) tsq_v.doubleDataMut()[i] = tsq[i];
    Value explained_v = Value::matrix(D, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < D; ++i) explained_v.doubleDataMut()[i] = explained[i];
    Value mu_v = Value::matrix(1, D, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < D; ++i) mu_v.doubleDataMut()[i] = mu[i];

    return std::make_tuple(std::move(coeff_v), std::move(score_v),
                           std::move(latent_v), std::move(tsq_v),
                           std::move(explained_v), std::move(mu_v));
}

std::tuple<Value, Value, Value>
pcacov(std::pmr::memory_resource *mr, const Value &C) {
    const size_t D = C.dims().rows();
    if (C.dims().cols() != D)
        throw Error("pcacov: C must be square", 0, 0, "pcacov", "",
                    "m:pcacov:size");

    std::vector<double> A(D * D);
    for (size_t r = 0; r < D; ++r)
        for (size_t c = 0; c < D; ++c) A[r * D + c] = C.elemAsDouble(c * D + r);

    std::vector<double> V;
    jacobi(A, V, D);

    std::vector<double> latent(D);
    std::vector<int> order(D);
    for (size_t i = 0; i < D; ++i) { latent[i] = A[i * D + i]; order[i] = (int)i; }
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return latent[a] > latent[b]; });

    std::vector<double> latent_s(D);
    std::vector<double> coeff(D * D, 0.0);
    for (size_t i = 0; i < D; ++i) {
        const int src = order[i];
        latent_s[i] = latent[src];
        for (size_t r = 0; r < D; ++r) coeff[r * D + i] = V[r * D + src];
    }

    double total = 0.0; for (double v : latent_s) total += v;
    std::vector<double> explained(D, 0.0);
    if (total > 0.0)
        for (size_t i = 0; i < D; ++i) explained[i] = 100.0 * latent_s[i] / total;

    Value coeff_v = Value::matrix(D, D, ValueType::DOUBLE, mr);
    for (size_t r = 0; r < D; ++r)
        for (size_t c = 0; c < D; ++c)
            coeff_v.doubleDataMut()[c * D + r] = coeff[r * D + c];

    Value latent_v = Value::matrix(D, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < D; ++i) latent_v.doubleDataMut()[i] = latent_s[i];

    Value explained_v = Value::matrix(D, 1, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < D; ++i) explained_v.doubleDataMut()[i] = explained[i];

    return std::make_tuple(std::move(coeff_v), std::move(latent_v),
                           std::move(explained_v));
}

// Internal: returns both residuals and reconstruction (MATLAB form).
std::tuple<Value, Value>
pcares_full(std::pmr::memory_resource *mr, const Value &X, int ndim) {
    auto [coeff, score, latent, tsq, explained, mu] = pca(mr, X);
    const size_t N = X.dims().rows();
    const size_t D = X.dims().cols();
    if (ndim < 0 || (size_t)ndim > D)
        throw Error("pcares: ndim must be in 0..D", 0, 0, "pcares", "",
                    "m:pcares:badndim");

    Value res   = Value::matrix(N, D, ValueType::DOUBLE, mr);
    Value recon = Value::matrix(N, D, ValueType::DOUBLE, mr);
    double *rd = res.doubleDataMut();
    double *cd = recon.doubleDataMut();

    for (size_t i = 0; i < N; ++i)
        for (size_t j = 0; j < D; ++j) {
            double r = mu.elemAsDouble(j);
            for (int k = 0; k < ndim; ++k) {
                r += score.elemAsDouble((size_t)k * N + i)
                   * coeff.elemAsDouble((size_t)k * D + j);
            }
            cd[j * N + i] = r;
            rd[j * N + i] = X.elemAsDouble(j * N + i) - r;
        }
    return {std::move(res), std::move(recon)};
}

Value pcares(std::pmr::memory_resource *mr, const Value &X, int ndim) {
    auto [res, recon] = pcares_full(mr, X, ndim);
    (void)recon;
    return res;
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void pca_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pca: requires X", 0, 0, "pca", "", "m:pca:nargin");
    auto [coeff, score, latent, tsq, explained, mu] =
        pca(ctx.engine->resource(), args[0]);
    outs[0] = std::move(coeff);
    if (nargout > 1) outs[1] = std::move(score);
    if (nargout > 2) outs[2] = std::move(latent);
    if (nargout > 3) outs[3] = std::move(tsq);
    if (nargout > 4) outs[4] = std::move(explained);
    if (nargout > 5) outs[5] = std::move(mu);
}

void pcacov_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pcacov: requires C", 0, 0, "pcacov", "",
                    "m:pcacov:nargin");
    auto [coeff, latent, explained] = pcacov(ctx.engine->resource(), args[0]);
    outs[0] = std::move(coeff);
    if (nargout > 1) outs[1] = std::move(latent);
    if (nargout > 2) outs[2] = std::move(explained);
}

void pcares_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pcares: requires (X, ndim)", 0, 0, "pcares", "",
                    "m:pcares:nargin");
    auto [res, recon] = pcares_full(ctx.engine->resource(),
                                    args[0], (int)args[1].toScalar());
    outs[0] = std::move(res);
    if (nargout > 1) outs[1] = std::move(recon);
}

} // namespace detail
} // namespace numkit::stats
