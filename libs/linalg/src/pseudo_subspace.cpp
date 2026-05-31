// libs/linalg/src/pseudo_subspace.cpp
//
// pinv / orth / null / subspace — implementations and engine adapters.
// All four are SVD-based; migrated alongside the SVD kernel.

#include <numkit/linalg/pseudo_subspace.hpp>

#include <numkit/linalg/decompositions.hpp>   // svd_decompose / svd_values
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace numkit::linalg {

namespace {

// Default tolerance for rank-cutoff: max(m,n) * eps(sigma_max).
double defaultRankTol(std::size_t m, std::size_t n, double sigma_max)
{
    return static_cast<double>(std::max(m, n))
         * sigma_max
         * std::numeric_limits<double>::epsilon();
}

} // anonymous namespace

Value pinv(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    auto [U, S, V] = svd_decompose(A, mr);
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t k = std::min(m, n);

    const double *S_data = S.doubleData();
    const std::size_t Srows = static_cast<std::size_t>(S.dims().dim(0));

    double sigma_max = 0.0;
    for (std::size_t i = 0; i < k; ++i)
        sigma_max = std::max(sigma_max, S_data[i + i * Srows]);
    const double cutoff = (tol < 0.0) ? defaultRankTol(m, n, sigma_max) : tol;

    ScratchArena scratch(mr);
    ScratchVec<double> Splus(n * m, 0.0, &scratch);
    for (std::size_t i = 0; i < k; ++i) {
        const double sig = S_data[i + i * Srows];
        if (sig > cutoff)
            Splus[i + i * n] = 1.0 / sig;
    }

    // pinv(A) = V * S^+ * U' -- output is n × m.
    auto out = Value::matrix(n, m, ValueType::DOUBLE, mr);
    double *P = out.doubleDataMut();
    std::fill(P, P + n * m, 0.0);

    const double *Vdata = V.doubleData();
    const double *Udata = U.doubleData();
    const std::size_t Vrows = static_cast<std::size_t>(V.dims().dim(0));
    const std::size_t Urows = static_cast<std::size_t>(U.dims().dim(0));

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < m; ++j) {
            double s = 0.0;
            for (std::size_t a = 0; a < k; ++a) {
                const double sp = Splus[a + a * n];
                if (sp == 0.0) continue;
                s += Vdata[i + a * Vrows] * sp * Udata[j + a * Urows];
            }
            P[i + j * n] = s;
        }
    }
    return out;
}

Value orth(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    auto [U, S, V] = svd_decompose(A, mr);
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t k = std::min(m, n);

    const double *S_data = S.doubleData();
    const std::size_t Srows = static_cast<std::size_t>(S.dims().dim(0));
    const double sigma_max = (k > 0) ? S_data[0] : 0.0;
    const double cutoff = (tol < 0.0) ? defaultRankTol(m, n, sigma_max) : tol;

    int r = 0;
    for (std::size_t i = 0; i < k; ++i)
        if (S_data[i + i * Srows] > cutoff) ++r;

    auto out = Value::matrix(m, static_cast<std::size_t>(r), ValueType::DOUBLE, mr);
    if (r == 0) return out;
    double *Q = out.doubleDataMut();
    const double *Udata = U.doubleData();
    const std::size_t Urows = static_cast<std::size_t>(U.dims().dim(0));
    for (std::size_t j = 0; j < static_cast<std::size_t>(r); ++j)
        for (std::size_t i = 0; i < m; ++i)
            Q[i + j * m] = Udata[i + j * Urows];
    return out;
}

Value null_basis(const Value &A, double tol, std::pmr::memory_resource *mr)
{
    auto [U, S, V] = svd_decompose(A, mr);
    const std::size_t m = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(A.dims().dim(1));
    const std::size_t k = std::min(m, n);

    const double *S_data = S.doubleData();
    const std::size_t Srows = static_cast<std::size_t>(S.dims().dim(0));
    const double sigma_max = (k > 0) ? S_data[0] : 0.0;
    const double cutoff = (tol < 0.0) ? defaultRankTol(m, n, sigma_max) : tol;

    int r = 0;
    for (std::size_t i = 0; i < k; ++i)
        if (S_data[i + i * Srows] > cutoff) ++r;

    const std::size_t null_dim = n - static_cast<std::size_t>(r);
    auto out = Value::matrix(n, null_dim, ValueType::DOUBLE, mr);
    if (null_dim == 0) return out;
    double *N = out.doubleDataMut();
    const double *Vdata = V.doubleData();
    const std::size_t Vrows = static_cast<std::size_t>(V.dims().dim(0));
    for (std::size_t j = 0; j < null_dim; ++j) {
        const std::size_t src = static_cast<std::size_t>(r) + j;
        for (std::size_t i = 0; i < n; ++i)
            N[i + j * n] = Vdata[i + src * Vrows];
    }
    return out;
}

Value subspace(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() != 2 || B.dims().ndim() != 2)
        throw Error("subspace: inputs must be 2D matrices",
                    0, 0, "subspace", "", "numkit:subspace:notMatrix");
    const std::size_t mA = static_cast<std::size_t>(A.dims().dim(0));
    const std::size_t mB = static_cast<std::size_t>(B.dims().dim(0));
    if (mA != mB)
        throw Error("subspace: inputs must have the same number of rows",
                    0, 0, "subspace", "", "numkit:subspace:dimMismatch");

    auto Qa = orth(A, -1.0, mr);
    auto Qb = orth(B, -1.0, mr);
    const std::size_t na = static_cast<std::size_t>(Qa.dims().dim(1));
    const std::size_t nb = static_cast<std::size_t>(Qb.dims().dim(1));
    if (na == 0 || nb == 0) return Value::scalar(0.0, mr);

    // M = Qa' * Qb (na × nb).
    auto Mout = Value::matrix(na, nb, ValueType::DOUBLE, mr);
    double *M = Mout.doubleDataMut();
    const double *Qad = Qa.doubleData();
    const double *Qbd = Qb.doubleData();
    for (std::size_t i = 0; i < na; ++i)
        for (std::size_t j = 0; j < nb; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < mA; ++k)
                s += Qad[k + i * mA] * Qbd[k + j * mB];
            M[i + j * na] = s;
        }

    // SVD of M -- singular values are cosines of principal angles.
    auto s = svd_values(Mout, mr);
    const std::size_t k = s.numel();
    if (k == 0) return Value::scalar(0.0, mr);
    const double *sd = s.doubleData();
    double smin = sd[0];
    for (std::size_t i = 1; i < k; ++i)
        if (sd[i] < smin) smin = sd[i];
    if (smin > 1.0) smin = 1.0;
    if (smin < 0.0) smin = 0.0;
    return Value::scalar(std::acos(smin), mr);
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters — registered in LinalgLibrary::install
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void pinv_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("pinv: requires (A) or (A, tol)",
                    0, 0, "pinv", "", "numkit:pinv:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = pinv(args[0], tol, ctx.engine->resource());
}

void orth_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("orth: requires (A) or (A, tol)",
                    0, 0, "orth", "", "numkit:orth:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = orth(args[0], tol, ctx.engine->resource());
}

void null_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 1 || args.size() > 2)
        throw Error("null: requires (A) or (A, tol)",
                    0, 0, "null", "", "numkit:null:nargin");
    const double tol = (args.size() >= 2) ? args[1].toScalar() : -1.0;
    outs[0] = null_basis(args[0], tol, ctx.engine->resource());
}

void subspace_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("subspace: requires (A, B)",
                    0, 0, "subspace", "", "numkit:subspace:nargin");
    outs[0] = subspace(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::linalg
