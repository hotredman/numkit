// toolboxes/stats/src/descriptive/tiedrank.cpp
//
// MATLAB tiedrank: assign average ranks within tied value groups.
//
//   [r, tieadj] = tiedrank(x)
//
// Vector input: r is a vector of the same shape as x. Equal values
// (ties) get the average of their would-be sequential ranks; e.g.
// tiedrank([10 20 30 20 10 40]) = [1.5 3.5 5 3.5 1.5 6].
//
// Matrix input: tiedrank applies along columns; r has the same
// shape; tieadj is a 1-by-cols row vector of per-column tie
// adjustments.
//
// Tie adjustment per column: sum over each tied group of size t of
//   (t^3 - t) / 2
// (matches MATLAB's `(t-1)*t*(t+1)/2`).
//
// NaN handling: NaN values get NaN rank and don't participate in
// the ranking sequence (the non-NaN values are ranked among
// themselves, then NaN slots are written back as NaN).

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace {

// Rank one column-vector worth of values.
// Output: ranks vector (length N, NaN for NaN inputs); tieadj scalar.
double rankOneColumn(const double *col, size_t N, double *ranks)
{
    // Build (value, original_index) pairs for non-NaN entries; NaN
    // gets NaN in the rank output and skipped in sorting.
    std::vector<std::pair<double, size_t>> nz;
    nz.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        if (std::isnan(col[i])) {
            ranks[i] = std::numeric_limits<double>::quiet_NaN();
        } else {
            nz.emplace_back(col[i], i);
        }
    }
    // Stable sort by value (preserves original order on ties; MATLAB
    // ranks ties by averaging so order doesn't change the output).
    std::stable_sort(nz.begin(), nz.end(),
                     [](const auto &a, const auto &b) {
                         return a.first < b.first;
                     });

    double tieadj = 0.0;
    const size_t M = nz.size();
    size_t i = 0;
    while (i < M) {
        size_t j = i + 1;
        while (j < M && nz[j].first == nz[i].first) ++j;
        const size_t group_len = j - i;
        // Average rank for positions i..j-1 (1-based ranks).
        const double avg_rank = (static_cast<double>(i + 1)
                                + static_cast<double>(j))
                              * 0.5;
        for (size_t k = i; k < j; ++k) {
            ranks[nz[k].second] = avg_rank;
        }
        if (group_len > 1) {
            const double t = static_cast<double>(group_len);
            tieadj += (t * t * t - t) * 0.5;
        }
        i = j;
    }
    return tieadj;
}

bool isVector(const Value &v)
{
    return v.dims().rows() == 1 || v.dims().cols() == 1;
}

} // namespace

std::pair<Value, Value>
tiedrank(const Value &x, std::pmr::memory_resource *mr)
{
    const size_t R = x.dims().rows();
    const size_t C = x.dims().cols();

    if (isVector(x)) {
        const size_t N = x.numel();
        // Output ranks: same shape as x.
        Value r = Value::matrix(R, C, ValueType::DOUBLE, mr);
        std::vector<double> col(N);
        for (size_t i = 0; i < N; ++i) col[i] = x.elemAsDouble(i);
        const double tieadj = rankOneColumn(col.data(), N,
                                            r.doubleDataMut());
        Value ta = Value::scalar(tieadj, mr);
        return {std::move(r), std::move(ta)};
    }

    // Matrix: column-wise, tieadj is 1-by-C row.
    Value r  = Value::matrix(R, C, ValueType::DOUBLE, mr);
    Value ta = Value::matrix(1, C, ValueType::DOUBLE, mr);
    double *ro = r.doubleDataMut();
    double *to = ta.doubleDataMut();
    std::vector<double> col(R);
    for (size_t c = 0; c < C; ++c) {
        for (size_t i = 0; i < R; ++i)
            col[i] = x.elemAsDouble(c * R + i);
        to[c] = rankOneColumn(col.data(), R, ro + c * R);
    }
    return {std::move(r), std::move(ta)};
}

} // namespace numkit::stats
