// libs/stats/src/descriptive/cholcov.cpp
//
// MATLAB cholcov(SIGMA): Cholesky-like factor of a (possibly singular)
// covariance matrix.
//
//   [T, p] = cholcov(SIGMA)
//
// Returns T such that T'*T == SIGMA (within rounding) and the
// "non-PD count" p:
//   - SIGMA positive-definite      -> T = upper-tri n×n (chol),  p = 0
//   - SIGMA PSD, rank r < n        -> T = r×n,                   p = 0
//   - SIGMA has any negative eig   -> T = empty 0×0,             p = #(eig <= tol)
//
// Algorithm:
//   1. Try in-place upper-tri Cholesky. If sqrt(s) succeeds at every
//      step, return the resulting upper-triangular R.
//   2. Else eigendecompose SIGMA (treated as symmetric).
//   3. Tolerance tol = eps^(3/4) * max(|d|).
//   4. p = sum(d <= -tol) (count of "negative" eigenvalues).
//   5. If p > 0, return empty T.
//   6. Else (PSD), keep eigvals d > tol and rebuild T = sqrt(d_keep) .* V_keep'.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/builtin/language/arrays/matrix.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace {

// In-place upper-tri Cholesky attempt. Returns true if SIGMA is PD;
// fills `out` with R such that R'*R = SIGMA. On any non-positive
// pivot, returns false.
bool tryCholesky(const Value &SIGMA, std::vector<double> &out, size_t n)
{
    const double *a = SIGMA.doubleData();
    out.assign(n * n, 0.0);
    double *r = out.data();
    for (size_t j = 0; j < n; ++j) {
        double s = a[j + j * n];
        for (size_t k = 0; k < j; ++k)
            s -= r[k + j * n] * r[k + j * n];
        if (s <= 0.0) return false;
        r[j + j * n] = std::sqrt(s);
        const double inv_diag = 1.0 / r[j + j * n];
        for (size_t i = j + 1; i < n; ++i) {
            double t = a[j + i * n];
            for (size_t k = 0; k < j; ++k)
                t -= r[k + j * n] * r[k + i * n];
            r[j + i * n] = t * inv_diag;
        }
    }
    return true;
}

} // namespace

std::pair<Value, Value>
cholcov(const Value &SIGMA, std::pmr::memory_resource *mr)
{
    const size_t R = SIGMA.dims().rows();
    const size_t C = SIGMA.dims().cols();
    if (R != C)
        throw Error("cholcov: SIGMA must be square",
                    0, 0, "cholcov", "", "m:cholcov:NotSquare");
    if (R == 0) {
        Value T = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return {std::move(T), Value::scalar(0.0, mr)};
    }

    // 1. Try positive-definite Cholesky.
    std::vector<double> chol_out;
    if (tryCholesky(SIGMA, chol_out, R)) {
        Value T = Value::matrix(R, R, ValueType::DOUBLE, mr);
        std::copy(chol_out.begin(), chol_out.end(), T.doubleDataMut());
        return {std::move(T), Value::scalar(0.0, mr)};
    }

    // 2. Fall back to eigendecomposition. Treat SIGMA as symmetric;
    //    eig_symmetric throws if it's not symmetric.
    auto [V, D] = ::numkit::builtin::eig_symmetric(SIGMA, mr);

    // D is n×n diagonal; extract eigvals into a flat vector.
    std::vector<double> d(R);
    {
        const double *dd = D.doubleData();
        for (size_t i = 0; i < R; ++i) d[i] = dd[i + i * R];
    }
    double maxAbsD = 0.0;
    for (double v : d) maxAbsD = std::max(maxAbsD, std::abs(v));
    const double tol = std::pow(std::numeric_limits<double>::epsilon(),
                                0.75) * maxAbsD;

    // 3. Count "negative" eigenvalues beyond tolerance.
    size_t p = 0;
    for (double v : d) if (v <= -tol) ++p;
    if (p > 0) {
        Value T = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return {std::move(T), Value::scalar(static_cast<double>(p), mr)};
    }

    // 4. PSD: keep eigvals > tol, rebuild T = sqrt(d_keep) .* V_keep'.
    std::vector<size_t> keep;
    keep.reserve(R);
    for (size_t i = 0; i < R; ++i) if (d[i] > tol) keep.push_back(i);
    const size_t r = keep.size();
    Value T = Value::matrix(r, R, ValueType::DOUBLE, mr);
    double *o = T.doubleDataMut();
    const double *vd = V.doubleData();
    for (size_t k = 0; k < r; ++k) {
        const size_t col = keep[k];
        const double s = std::sqrt(d[col]);
        // T(k, j) = s * V(j, col)
        for (size_t j = 0; j < R; ++j)
            o[k + j * r] = s * vd[j + col * R];
    }
    return {std::move(T), Value::scalar(0.0, mr)};
}

namespace detail {

void cholcov_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cholcov: requires (SIGMA)",
                    0, 0, "cholcov", "", "m:cholcov:nargin");
    auto *mr = ctx.engine->resource();
    auto [T, p] = cholcov(args[0], mr);
    outs[0] = std::move(T);
    if (nargout > 1) outs[1] = std::move(p);
}

} // namespace detail

} // namespace numkit::stats
