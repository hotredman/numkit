// libs/stats/src/descriptive/nearcorr.cpp
//
// MATLAB nearcorr: nearest correlation matrix (Higham 2002 alternating
// projections with Dykstra's correction).
//
//   Y = nearcorr(A)
//
// Algorithm (Higham, "Computing the Nearest Correlation Matrix --
// A Problem from Finance", IMA J. Numer. Anal. 22 (3): 329-343, 2002):
//   Y = A; dS = 0
//   loop:
//     R = Y - dS                 -- subtract Dykstra correction
//     X = proj_PSD(R)            -- eig, clamp negative eigvals to 0
//     dS = X - R                 -- update Dykstra correction
//     Y = proj_unit_diag(X)      -- set diagonal entries to 1
//     until ||Y - Y_prev||_F / ||Y||_F < tol
//
// Convergence default tol = 1e-10, maxits = 100. Symmetry is enforced
// in proj_PSD via the symmetric eigendecomposition; the input is
// assumed symmetric (cholcov/grpstats workflow output is symmetric).
//
// KNOWN GAP: 'tolconv' / 'maxits' name-value parameters deferred.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/builtin/language/arrays/matrix.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::stats {

namespace {

// Project onto PSD cone via symmetric eigendecomposition.
// Symmetrise input first to guard against tiny asymmetry from the
// previous iteration.
std::vector<double>
projPSD(std::pmr::memory_resource *mr, const std::vector<double> &M, size_t n)
{
    // Symmetrise: M_sym = (M + M') / 2
    std::vector<double> Msym(n * n);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            Msym[i + j * n] = 0.5 * (M[i + j * n] + M[j + i * n]);
    Value M_v = Value::matrix(n, n, ValueType::DOUBLE, mr);
    std::copy(Msym.begin(), Msym.end(), M_v.doubleDataMut());

    auto [V, D] = ::numkit::builtin::eig_symmetric(mr, M_v);
    const double *vd = V.doubleData();
    const double *dd = D.doubleData();

    // Build sqrt(d_pos) and reconstruct V * diag(d_pos) * V'.
    std::vector<double> sqrt_d(n);
    for (size_t i = 0; i < n; ++i)
        sqrt_d[i] = std::sqrt(std::max(dd[i + i * n], 0.0));

    // X = V * diag(d_pos) * V' = (V * diag(sqrt_d)) * (V * diag(sqrt_d))'
    std::vector<double> VS(n * n);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            VS[i + j * n] = vd[i + j * n] * sqrt_d[j];

    std::vector<double> X(n * n, 0.0);
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (size_t k = 0; k < n; ++k)
                s += VS[i + k * n] * VS[j + k * n];   // VS * VS'
            X[i + j * n] = s;
        }
    return X;
}

double frobNorm(const std::vector<double> &M)
{
    double s = 0.0;
    for (double v : M) s += v * v;
    return std::sqrt(s);
}

} // namespace

Value nearcorr(std::pmr::memory_resource *mr, const Value &A)
{
    const size_t R = A.dims().rows();
    const size_t C = A.dims().cols();
    if (R != C)
        throw Error("nearcorr: input must be square",
                    0, 0, "nearcorr", "", "m:nearcorr:NotSquare");
    if (R == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Initialise Y = A, dS = 0.
    std::vector<double> Y(R * R), dS(R * R, 0.0);
    for (size_t j = 0; j < R; ++j)
        for (size_t i = 0; i < R; ++i)
            Y[i + j * R] = A.elemAsDouble(i + j * R);

    const double tol = 1e-10;
    const int maxits = 100;

    std::vector<double> Y_prev = Y;
    std::vector<double> Rmat(R * R), X(R * R);

    for (int it = 0; it < maxits; ++it) {
        // R = Y - dS
        for (size_t k = 0; k < R * R; ++k) Rmat[k] = Y[k] - dS[k];
        // X = proj_PSD(R)
        X = projPSD(mr, Rmat, R);
        // dS = X - R
        for (size_t k = 0; k < R * R; ++k) dS[k] = X[k] - Rmat[k];
        // Y = proj_unit_diag(X)  (set diagonal to 1)
        Y = X;
        for (size_t i = 0; i < R; ++i) Y[i + i * R] = 1.0;

        // Convergence: ||Y - Y_prev||_F / ||Y||_F
        std::vector<double> diff(R * R);
        for (size_t k = 0; k < R * R; ++k) diff[k] = Y[k] - Y_prev[k];
        const double dn = frobNorm(diff);
        const double yn = frobNorm(Y);
        if (yn > 0.0 && dn / yn < tol) break;
        Y_prev = Y;
    }

    // Symmetrise final result (small numerical drift is possible).
    Value out = Value::matrix(R, R, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    for (size_t j = 0; j < R; ++j)
        for (size_t i = 0; i < R; ++i)
            o[i + j * R] = 0.5 * (Y[i + j * R] + Y[j + i * R]);
    return out;
}

namespace detail {

void nearcorr_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("nearcorr: requires (A)",
                    0, 0, "nearcorr", "", "m:nearcorr:nargin");
    outs[0] = nearcorr(ctx.engine->resource(), args[0]);
}

} // namespace detail

} // namespace numkit::stats
