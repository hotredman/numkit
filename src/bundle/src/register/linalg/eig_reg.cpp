// toolboxes/linalg/src/eig_reg.cpp
//
// Register half of the eigenvalue family: the CallContext wrappers
// eig / hess / schur / sylvester / polyeig / ordeig that delegate to the
// engine-free compute in eig.cpp. The auto-dispatch helpers
// (eigValuesAuto / eigVDAuto, choosing the symmetric Jacobi vs general
// char-poly path via the now-public isSymmetricApprox) and leftEigenvectors
// (the [V,D,W] form) live here too — they are register-only glue.
// library.cpp forward-declares + registers the *_reg fns by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/linalg/eig.hpp>

#include <numkit/linalg/properties.hpp>               // inv (generalized eig B\A)
#include <numkit/lang/operators/binary_ops.hpp>  // mtimes (eig(A,B))

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

namespace numkit::linalg {
namespace detail {

void ordeig_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("ordeig: requires (T)",
                    0, 0, "ordeig", "", "numkit:ordeig:nargin");
    outs[0] = ordeig(args[0], ctx.engine->resource());
}

void polyeig_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("polyeig: requires at least 2 coefficient matrices",
                    0, 0, "polyeig", "", "numkit:polyeig:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [V, e] = polyeig_VE(args, mr);
        outs[0] = std::move(V);
        outs[1] = std::move(e);
    } else {
        outs[0] = polyeig_values(args, mr);
    }
}

namespace {

// Eigenvalues of M as a column vector, choosing the symmetric (Jacobi) or
// general (char-poly + roots) path automatically.
Value eigValuesAuto(const Value &M, std::pmr::memory_resource *mr)
{
    return isSymmetricApprox(M, 1e-10) ? eig_values(M, mr)
                                       : eig_general_values(M, mr);
}

// [V, D] of M, choosing the symmetric or general path automatically.
std::tuple<Value, Value> eigVDAuto(const Value &M, std::pmr::memory_resource *mr)
{
    return isSymmetricApprox(M, 1e-10) ? eig_symmetric(M, mr)
                                       : eig_general_VD(M, mr);
}

} // namespace

// Left eigenvectors W (the [V,D,W]=eig form): W'·A = D·W', i.e. the columns of
// W are the right eigenvectors of Aᵀ. We eig(Mᵀ), reorder its columns to match
// the eigenvalue order of D, and normalize each to unit 2-norm (MATLAB). For
// symmetric M this reduces to W == V. Only real-eigenvalue M is supported (the
// general eig path itself throws on complex eigenvalues).
static Value leftEigenvectors(const Value &M, const Value &D,
                              std::pmr::memory_resource *mr)
{
    const std::size_t n = static_cast<std::size_t>(M.dims().dim(0));
    // Mᵀ.
    Value Mt = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *mt = Mt.doubleDataMut();
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < n; ++i)
            mt[i + j * n] = M.elemAsDouble(j + i * n);   // Mᵀ(i,j) = M(j,i)

    auto [VL, DL] = eigVDAuto(Mt, mr);

    std::vector<double> d(n), dl(n);
    for (std::size_t k = 0; k < n; ++k) d[k]  = D.elemAsDouble(k + k * n);
    for (std::size_t k = 0; k < n; ++k) dl[k] = DL.elemAsDouble(k + k * n);

    const double *vld = VL.doubleData();
    Value W = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *wd = W.doubleDataMut();
    std::vector<bool> used(n, false);
    for (std::size_t k = 0; k < n; ++k) {
        // Match D's k-th eigenvalue to the nearest unused eigenvalue of Mᵀ.
        std::size_t best = n;
        double bestErr = std::numeric_limits<double>::infinity();
        for (std::size_t j = 0; j < n; ++j) {
            if (used[j]) continue;
            const double e = std::fabs(dl[j] - d[k]);
            if (e < bestErr) { bestErr = e; best = j; }
        }
        used[best] = true;
        double nrm = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            nrm += vld[i + best * n] * vld[i + best * n];
        nrm = std::sqrt(nrm);
        for (std::size_t i = 0; i < n; ++i)
            wd[i + k * n] = (nrm > 0.0) ? vld[i + best * n] / nrm : vld[i + best * n];
    }
    return W;
}

void eig_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || args.size() > 3)
        throw Error("eig: requires 1 to 3 arguments",
                    0, 0, "eig", "", "numkit:eig:nargin");
    auto *mr = ctx.engine->resource();

    // Parse trailing args: an optional second matrix B (generalized
    // problem A·v = λ·B·v) and/or a string flag 'vector'/'matrix'
    // ('chol'/'qz'/'nobalance' accepted and ignored — we always reduce
    // the generalized problem to the standard one B\A).
    const Value *A = &args[0];
    const Value *B = nullptr;
    bool wantMatrix = false;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i].type() == ValueType::CHAR) {
            std::string s = args[i].toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (s == "matrix") wantMatrix = true;
            else if (s == "vector") { /* default for the 1-output form */ }
            else if (s == "chol" || s == "qz" || s == "nobalance") { /* accept */ }
            else
                throw Error("eig: unknown option '" + s + "'",
                            0, 0, "eig", "", "numkit:eig:badOption");
        } else {
            if (B)
                throw Error("eig: at most one matrix B is allowed",
                            0, 0, "eig", "", "numkit:eig:tooManyMatrices");
            B = &args[i];
        }
    }

    // Reduce a generalized problem (A, B) to the standard problem on
    // M = B\A = inv(B)·A. The eigenvalues of M equal the generalized
    // eigenvalues, and any eigenvector v of M satisfies A·v = B·v·λ.
    Value M = B ? numkit::lang::mtimes(inv(*B, mr), *A, mr) : *A;

    if (nargout >= 2) {
        auto [V, D] = eigVDAuto(M, mr);
        if (nargout >= 3)
            outs[2] = leftEigenvectors(M, D, mr);   // left eigenvectors
        outs[0] = std::move(V);
        outs[1] = std::move(D);
        return;
    }
    if (wantMatrix) {                       // 'matrix' → diagonal D even with 1 output
        auto [V, D] = eigVDAuto(M, mr);
        (void)V;
        outs[0] = std::move(D);
        return;
    }
    outs[0] = eigValuesAuto(M, mr);
}

void hess_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("hess: requires exactly 1 argument",
                    0, 0, "hess", "", "numkit:hess:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [P, H] = hess(args[0], mr);
        outs[0] = std::move(P);
        outs[1] = std::move(H);
    } else {
        outs[0] = hess_H_only(args[0], mr);
    }
}

void schur_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 1)
        throw Error("schur: requires exactly 1 argument",
                    0, 0, "schur", "", "numkit:schur:nargin");
    auto *mr = ctx.engine->resource();
    if (args[0].dims().ndim() != 2 || args[0].dims().dim(0) != args[0].dims().dim(1))
        throw Error("schur: matrix must be square",
                    0, 0, "schur", "", "numkit:schur:notSquare");
    // Symmetric A → diagonal Schur (eig); general A → real Schur (Francis QR).
    auto [U, T] = isSymmetricApprox(args[0], 1e-10)
                      ? schur_sym(args[0], mr)
                      : schur_general(args[0], mr);
    if (nargout >= 2) {
        outs[0] = std::move(U);
        outs[1] = std::move(T);
    } else {
        outs[0] = std::move(T);
    }
}

void sylvester_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 3)
        throw Error("sylvester: requires (A, B, C)",
                    0, 0, "sylvester", "", "numkit:sylvester:nargin");
    outs[0] = sylvester_sym(args[0], args[1], args[2], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::linalg
