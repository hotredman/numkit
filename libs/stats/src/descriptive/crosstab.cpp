// libs/stats/src/descriptive/crosstab.cpp
//
// MATLAB crosstab: contingency table.
//
//   T = crosstab(x)             frequency vector for unique(x)
//   T = crosstab(x, y)          rows=unique(x), cols=unique(y), entries=counts
//   [T, chi2, p] = crosstab(x, y)
//   [T, chi2, p, labels] = crosstab(x [, y])
//
// labels: a (maxCategories × numVars) cell array. Column j holds the
// num2str of each sorted-unique value of variable j (a char row), top
// to bottom; rows beyond that variable's category count are padded with
// [] (an empty 0x0 double), matching MATLAB R2025b.
//
// chi2 = sum (O - E)^2 / E   over cells with E > 0
// E(i,j) = row_total(i) * col_total(j) / N
// p = 1 - chi2cdf(chi2, df), where df = (rows - 1) * (cols - 1).
//
// Numeric input only for v1; cell/string/categorical inputs deferred.
// NaN values are excluded from the row/col index sets and from the
// total N.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/stats/distributions/chi2.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace {

// Sorted-unique values from x (NaN excluded).
std::vector<double> sortedUniqueNoNaN(const Value &x)
{
    const size_t N = x.numel();
    std::vector<double> v;
    v.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        const double xi = x.elemAsDouble(i);
        if (std::isnan(xi)) continue;
        v.push_back(xi);
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

// Find index of value in sorted unique list (binary search).
size_t indexOf(const std::vector<double> &u, double val)
{
    auto it = std::lower_bound(u.begin(), u.end(), val);
    return static_cast<size_t>(it - u.begin());
}

// num2str-style label for a (typically integer) group value, matching
// MATLAB crosstab's `labels` output. Integer values print without a
// decimal point; non-integers fall back to %.5g (num2str's default
// significant-digit count for the simple decimals seen in group ids).
std::string labelString(double v)
{
    char buf[64];
    if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 1e15)
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
    else
        std::snprintf(buf, sizeof(buf), "%.5g", v);
    return std::string(buf);
}

} // namespace

std::tuple<Value, double, double>
crosstab(const Value &x, const Value &y_opt, std::pmr::memory_resource *mr)
{
    const size_t Nx = x.numel();
    const bool have_y = !y_opt.isEmpty();
    if (have_y && y_opt.numel() != Nx)
        throw Error("crosstab: x and y must have the same length",
                    0, 0, "crosstab", "", "numkit:crosstab:LenMismatch");

    auto u_x = sortedUniqueNoNaN(x);
    const size_t R = u_x.size();

    if (!have_y) {
        // Single-arg form: column vector with frequency counts of unique x.
        std::vector<size_t> count(R, 0);
        for (size_t k = 0; k < Nx; ++k) {
            const double xk = x.elemAsDouble(k);
            if (std::isnan(xk)) continue;
            count[indexOf(u_x, xk)]++;
        }
        Value T = Value::matrix(R, 1, ValueType::DOUBLE, mr);
        double *o = T.doubleDataMut();
        for (size_t i = 0; i < R; ++i) o[i] = static_cast<double>(count[i]);
        return {std::move(T), 0.0, 1.0};
    }

    // Two-arg form.
    auto u_y = sortedUniqueNoNaN(y_opt);
    const size_t C = u_y.size();

    Value T = Value::matrix(R, C, ValueType::DOUBLE, mr);
    double *o = T.doubleDataMut();
    std::fill(o, o + R * C, 0.0);

    size_t Ntot = 0;
    for (size_t k = 0; k < Nx; ++k) {
        const double xk = x.elemAsDouble(k);
        const double yk = y_opt.elemAsDouble(k);
        if (std::isnan(xk) || std::isnan(yk)) continue;
        const size_t i = indexOf(u_x, xk);
        const size_t j = indexOf(u_y, yk);
        o[i + j * R] += 1.0;
        ++Ntot;
    }

    // Chi-square test.
    std::vector<double> row_tot(R, 0.0), col_tot(C, 0.0);
    for (size_t j = 0; j < C; ++j)
        for (size_t i = 0; i < R; ++i) {
            row_tot[i] += o[i + j * R];
            col_tot[j] += o[i + j * R];
        }
    double chi2 = 0.0;
    if (Ntot > 0) {
        for (size_t j = 0; j < C; ++j) {
            for (size_t i = 0; i < R; ++i) {
                const double E = row_tot[i] * col_tot[j]
                               / static_cast<double>(Ntot);
                if (E > 0.0) {
                    const double diff = o[i + j * R] - E;
                    chi2 += (diff * diff) / E;
                }
            }
        }
    }
    double p = 1.0;
    if (R > 1 && C > 1) {
        const double df = static_cast<double>((R - 1) * (C - 1));
        // p = 1 - chi2cdf(chi2, df); chi2cdf returns a Value of input shape.
        Value chi2_v = Value::scalar(chi2, mr);
        Value cdf_v  = chi2cdf(chi2_v, df, mr);
        p = 1.0 - cdf_v.toScalar();
    }
    return {std::move(T), chi2, p};
}

namespace detail {

void crosstab_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("crosstab: requires (x [, y])",
                    0, 0, "crosstab", "", "numkit:crosstab:nargin");
    auto *mr = ctx.engine->resource();
    const Value &y_opt = (args.size() >= 2) ? args[1] : Value::Empty;
    auto [T, chi2, p] = crosstab(args[0], y_opt, mr);
    outs[0] = std::move(T);
    if (nargout > 1) outs[1] = Value::scalar(chi2, mr);
    if (nargout > 2) outs[2] = Value::scalar(p, mr);
    if (nargout > 3) {
        // labels: (maxCategories × numVars) cell. Column j = num2str of
        // each sorted-unique value of variable j (char row); rows beyond
        // a variable's category count are padded with [] (empty double),
        // matching MATLAB R2025b.
        std::vector<std::vector<double>> uniques;
        uniques.push_back(sortedUniqueNoNaN(args[0]));
        if (!y_opt.isEmpty())
            uniques.push_back(sortedUniqueNoNaN(y_opt));
        size_t maxR = 0;
        for (const auto &u : uniques) maxR = std::max(maxR, u.size());
        const size_t numVars = uniques.size();
        Value labels = Value::cell(maxR, numVars, mr);
        for (size_t j = 0; j < numVars; ++j) {
            const auto &u = uniques[j];
            for (size_t i = 0; i < maxR; ++i) {
                Value &cell = labels.cellAt(i + j * maxR);
                cell = (i < u.size())
                           ? Value::fromString(labelString(u[i]), mr)
                           : Value::Empty;
            }
        }
        outs[3] = std::move(labels);
    }
}

} // namespace detail

} // namespace numkit::stats
