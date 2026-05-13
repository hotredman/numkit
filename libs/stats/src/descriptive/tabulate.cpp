// libs/stats/src/descriptive/tabulate.cpp
//
// MATLAB tabulate(x): frequency table.
//
//   T = tabulate(x)
//
// Output is a 3-column double matrix. Row layout:
//   T(i, 1) = value
//   T(i, 2) = count
//   T(i, 3) = 100 * count / N    (percent of non-NaN total)
//
// Two output shapes (matching MATLAB R2025b):
//   - All non-NaN values are positive integers:
//     dense rows for k = 1 .. max(x)  (zeros for missing k)
//   - Otherwise:
//     one row per unique non-NaN value, sorted ascending
//
// NaN values are excluded both from the row set and from the
// percentage denominator.
//
// Empty input -> 0-by-1 matrix (matches MATLAB).

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::stats {

Value tabulate(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t N0 = x.numel();
    if (N0 == 0)
        return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    // Collect non-NaN values; check positive-integer property.
    std::vector<double> v;
    v.reserve(N0);
    bool all_pos_int = true;
    double max_val = 0.0;
    for (size_t i = 0; i < N0; ++i) {
        const double xi = x.elemAsDouble(i);
        if (std::isnan(xi)) continue;
        if (xi <= 0.0 || std::floor(xi) != xi)
            all_pos_int = false;
        else if (xi > max_val)
            max_val = xi;
        v.push_back(xi);
    }
    const size_t N = v.size();
    if (N == 0)
        return Value::matrix(0, 1, ValueType::DOUBLE, mr);

    if (all_pos_int) {
        const size_t K = static_cast<size_t>(max_val);
        std::vector<size_t> count(K, 0);
        for (double xi : v) {
            count[static_cast<size_t>(xi) - 1]++;
        }
        Value T = Value::matrix(K, 3, ValueType::DOUBLE, mr);
        double *o = T.doubleDataMut();
        const double inv_N = 100.0 / static_cast<double>(N);
        for (size_t k = 0; k < K; ++k) {
            o[k]            = static_cast<double>(k + 1);   // value
            o[k + K]        = static_cast<double>(count[k]); // count
            o[k + 2 * K]    = static_cast<double>(count[k]) * inv_N;
        }
        return T;
    }

    // Sparse layout: unique sorted values.
    std::sort(v.begin(), v.end());
    // Build unique list with counts.
    std::vector<double>  uniq;
    std::vector<size_t>  cnt;
    uniq.reserve(N);
    cnt.reserve(N);
    size_t i = 0;
    while (i < N) {
        size_t j = i + 1;
        while (j < N && v[j] == v[i]) ++j;
        uniq.push_back(v[i]);
        cnt.push_back(j - i);
        i = j;
    }
    const size_t K = uniq.size();
    Value T = Value::matrix(K, 3, ValueType::DOUBLE, mr);
    double *o = T.doubleDataMut();
    const double inv_N = 100.0 / static_cast<double>(N);
    for (size_t k = 0; k < K; ++k) {
        o[k]            = uniq[k];
        o[k + K]        = static_cast<double>(cnt[k]);
        o[k + 2 * K]    = static_cast<double>(cnt[k]) * inv_N;
    }
    return T;
}

namespace detail {

void tabulate_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("tabulate: requires (x)",
                    0, 0, "tabulate", "", "m:tabulate:nargin");
    if (args[0].isChar() || args[0].isString())
        throw Error("tabulate: string/cell inputs not yet supported",
                    0, 0, "tabulate", "", "m:tabulate:NotSupported");
    outs[0] = tabulate(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::stats
