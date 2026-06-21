// toolboxes/stats/src/anova/anova.cpp

#include <numkit/stats/anova/anova.hpp>

#include <numkit/stats/distributions/fisher_f.hpp>
#include <numkit/stats/distributions/chi2.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "anova_detail.hpp"

namespace numkit::stats {


std::tuple<double, double, double, double, double, double>
anova1(const Value &y, const Value &group, std::pmr::memory_resource *mr)
{
    auto buckets = bucket(y, group);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (buckets.size() < 2)
        return std::make_tuple(nan, nan, 0.0, 0.0, nan, nan);

    size_t N = 0;
    double grandSum = 0.0;
    std::vector<double> means(buckets.size());
    for (size_t k = 0; k < buckets.size(); ++k) {
        const auto &v = buckets[k].values;
        double s = 0.0;
        for (double x : v) s += x;
        means[k] = (v.empty() ? 0.0 : s / double(v.size()));
        N += v.size();
        grandSum += s;
    }
    const double grandMean = grandSum / double(N);

    double ssB = 0.0, ssW = 0.0;
    for (size_t k = 0; k < buckets.size(); ++k) {
        const auto &v = buckets[k].values;
        ssB += double(v.size()) * (means[k] - grandMean) * (means[k] - grandMean);
        for (double x : v) {
            const double d = x - means[k];
            ssW += d * d;
        }
    }
    const double dfB = double(buckets.size() - 1);
    const double dfW = double(N - buckets.size());
    if (dfW <= 0.0)
        return std::make_tuple(nan, nan, dfB, dfW, ssB, ssW);

    const double msB = ssB / dfB;
    const double msW = ssW / dfW;
    if (msW <= 0.0)
        return std::make_tuple(0.0, std::numeric_limits<double>::infinity(),
                               dfB, dfW, ssB, ssW);
    const double F = msB / msW;

    Value Fv = Value::scalar(F, mr);
    const double cdf = fcdf(Fv, dfB, dfW, mr).toScalar();
    const double p = std::max(0.0, 1.0 - cdf);
    return std::make_tuple(p, F, dfB, dfW, ssB, ssW);
}

// ── anova2 (two-way ANOVA without replication, reps=1) ───────────────
//
// Y is m × n: rows = levels of factor A, columns = levels of factor B.
// Returns (p_cols, p_rows, F_cols, F_rows, df_cols, df_rows, df_err,
//          ss_cols, ss_rows, ss_err).
// MATLAB convention: anova2 puts COLUMNS factor first (factor B),
// then ROWS factor (factor A).
std::tuple<double, double, double, double,
           double, double, double,
           double, double, double>
anova2(const Value &Y, std::pmr::memory_resource *mr)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (Y.dims().ndim() != 2)
        throw Error("anova2: input must be a 2D matrix",
                    0, 0, "anova2", "", "numkit:anova2:notMatrix");
    const std::size_t m = static_cast<std::size_t>(Y.dims().dim(0));
    const std::size_t n = static_cast<std::size_t>(Y.dims().dim(1));
    if (m < 2 || n < 2)
        throw Error("anova2: input must be at least 2x2",
                    0, 0, "anova2", "", "numkit:anova2:tooSmall");

    const double *yd = Y.doubleData();
    const double N = static_cast<double>(m * n);

    // Row means (size m), column means (size n), grand mean.
    std::vector<double> rowSum(m, 0.0);
    std::vector<double> colSum(n, 0.0);
    double grandSum = 0.0;
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < m; ++i) {
            const double v = yd[i + j * m];
            rowSum[i] += v;
            colSum[j] += v;
            grandSum += v;
        }
    const double grandMean = grandSum / N;
    std::vector<double> rowMean(m), colMean(n);
    for (std::size_t i = 0; i < m; ++i) rowMean[i] = rowSum[i] / static_cast<double>(n);
    for (std::size_t j = 0; j < n; ++j) colMean[j] = colSum[j] / static_cast<double>(m);

    // Sum of squares.
    double ssRows = 0.0, ssCols = 0.0, ssTotal = 0.0;
    for (std::size_t i = 0; i < m; ++i) {
        const double d = rowMean[i] - grandMean;
        ssRows += static_cast<double>(n) * d * d;
    }
    for (std::size_t j = 0; j < n; ++j) {
        const double d = colMean[j] - grandMean;
        ssCols += static_cast<double>(m) * d * d;
    }
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < m; ++i) {
            const double d = yd[i + j * m] - grandMean;
            ssTotal += d * d;
        }
    const double ssErr = ssTotal - ssRows - ssCols;

    const double dfRows = static_cast<double>(m - 1);
    const double dfCols = static_cast<double>(n - 1);
    const double dfErr  = static_cast<double>((m - 1) * (n - 1));
    if (dfErr <= 0.0)
        return std::make_tuple(nan, nan, nan, nan, dfCols, dfRows, dfErr,
                               ssCols, ssRows, ssErr);

    const double msErr = ssErr / dfErr;
    if (msErr <= 0.0) {
        const double inf = std::numeric_limits<double>::infinity();
        return std::make_tuple(0.0, 0.0, inf, inf, dfCols, dfRows, dfErr,
                               ssCols, ssRows, ssErr);
    }

    const double Fcols = (ssCols / dfCols) / msErr;
    const double Frows = (ssRows / dfRows) / msErr;
    Value FcolsV = Value::scalar(Fcols, mr);
    Value FrowsV = Value::scalar(Frows, mr);
    const double pCols = std::max(0.0, 1.0 - fcdf(FcolsV, dfCols, dfErr, mr).toScalar());
    const double pRows = std::max(0.0, 1.0 - fcdf(FrowsV, dfRows, dfErr, mr).toScalar());

    return std::make_tuple(pCols, pRows, Fcols, Frows,
                           dfCols, dfRows, dfErr,
                           ssCols, ssRows, ssErr);
}

std::tuple<Value, Value, Value, Value>
kruskalwallis(const Value &y, const Value &group, std::pmr::memory_resource *mr)
{
    auto buckets = bucket(y, group);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    if (buckets.size() < 2)
        return std::make_tuple(Value::scalar(nan, mr),
                               Value::scalar(nan, mr),
                               Value::scalar(0.0, mr),
                               Value::scalar(0.0, mr));

    // Flatten to single (value, group_idx) array for ranking.
    struct V { double val; size_t g; };
    std::vector<V> all;
    size_t N = 0;
    for (size_t k = 0; k < buckets.size(); ++k) {
        for (double v : buckets[k].values) all.push_back({v, k});
        N += buckets[k].values.size();
    }
    if (N == 0) return std::make_tuple(Value::scalar(nan, mr),
                                       Value::scalar(nan, mr),
                                       Value::scalar(0.0, mr),
                                       Value::scalar(0.0, mr));

    std::vector<size_t> ord(N);
    for (size_t i = 0; i < N; ++i) ord[i] = i;
    std::sort(ord.begin(), ord.end(),
              [&](size_t a, size_t b) { return all[a].val < all[b].val; });

    // Assign mid-ranks for ties.
    std::vector<double> ranks(N);
    std::vector<size_t> tieGroupSizes;
    size_t i = 0;
    while (i < N) {
        size_t j = i + 1;
        while (j < N && all[ord[j]].val == all[ord[i]].val) ++j;
        const double avg = static_cast<double>(i + j + 1) / 2.0;
        for (size_t k = i; k < j; ++k) ranks[ord[k]] = avg;
        if (j - i > 1) tieGroupSizes.push_back(j - i);
        i = j;
    }

    // Sum ranks by group.
    std::vector<double> R(buckets.size(), 0.0);
    std::vector<size_t> ng(buckets.size(), 0);
    for (size_t k = 0; k < N; ++k) {
        R[all[k].g] += ranks[k];
        ng[all[k].g]++;
    }

    double sumR2_n = 0.0;
    for (size_t g = 0; g < buckets.size(); ++g)
        if (ng[g] > 0) sumR2_n += R[g] * R[g] / double(ng[g]);

    const double Nd = double(N);
    double H = (12.0 / (Nd * (Nd + 1.0))) * sumR2_n - 3.0 * (Nd + 1.0);

    // Tie correction.
    double tieSum = 0.0;
    for (size_t t : tieGroupSizes) {
        const double td = double(t);
        tieSum += td * td * td - td;
    }
    if (tieSum > 0.0 && Nd > 1.0)
        H /= (1.0 - tieSum / (Nd * Nd * Nd - Nd));

    const double df = double(buckets.size() - 1);
    Value Hv = Value::scalar(H, mr);
    const double cdf = chi2cdf(Hv, df, mr).toScalar();
    const double p = std::max(0.0, 1.0 - cdf);

    return std::make_tuple(Value::scalar(p, mr),
                           Value::scalar(H, mr),
                           Value::scalar(df, mr),
                           Value::scalar(sumR2_n, mr));
}

std::tuple<Value, Value, Value>
friedman(const Value &x, int reps, std::pmr::memory_resource *mr)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const size_t nrows = x.dims().rows();
    const size_t k     = x.dims().cols();
    const size_t R     = (reps >= 1) ? static_cast<size_t>(reps) : 1;   // replicates per cell
    if (k < 2 || nrows < 2 || R == 0 || (nrows % R) != 0)
        return std::make_tuple(Value::scalar(nan, mr), Value::scalar(nan, mr),
                               Value::scalar(0.0, mr));
    const size_t n = nrows / R;   // blocks (rows of the Friedman layout)

    // Cell value (block b, treatment col c): mean over the R replicate rows.
    auto cell = [&](size_t b, size_t c) {
        double s = 0.0;
        for (size_t r = 0; r < R; ++r)
            s += x.elemAsDouble(c * nrows + (b * R + r));   // column-major
        return s / static_cast<double>(R);
    };

    std::vector<double> Rj(k, 0.0);      // column rank sums
    double              tieTerm = 0.0;   // Σ (t³ − t) over every tie group in every block
    std::vector<double> row(k), rank(k);
    std::vector<size_t> ord(k);
    for (size_t b = 0; b < n; ++b) {
        for (size_t c = 0; c < k; ++c) { row[c] = cell(b, c); ord[c] = c; }
        std::sort(ord.begin(), ord.end(), [&](size_t a, size_t bb) { return row[a] < row[bb]; });
        size_t i = 0;                    // mid-ranks within the block
        while (i < k) {
            size_t j = i + 1;
            while (j < k && row[ord[j]] == row[ord[i]]) ++j;
            const double avg = static_cast<double>(i + j + 1) / 2.0;
            for (size_t t = i; t < j; ++t) rank[ord[t]] = avg;
            const double tg = static_cast<double>(j - i);
            if (tg > 1.0) tieTerm += tg * tg * tg - tg;
            i = j;
        }
        for (size_t c = 0; c < k; ++c) Rj[c] += rank[c];
    }

    const double nd = static_cast<double>(n), kd = static_cast<double>(k);
    double sumRj2 = 0.0;
    for (double v : Rj) sumRj2 += v * v;
    double Q = 12.0 / (nd * kd * (kd + 1.0)) * sumRj2 - 3.0 * nd * (kd + 1.0);
    const double C = 1.0 - tieTerm / (nd * (kd * kd * kd - kd));   // tie correction
    if (C > 0.0) Q /= C;

    const double df = kd - 1.0;
    Value        Qv = Value::scalar(Q, mr);
    const double p  = std::max(0.0, 1.0 - chi2cdf(Qv, df, mr).toScalar());
    return std::make_tuple(Value::scalar(p, mr), Value::scalar(Q, mr), Value::scalar(df, mr));
}

Value dummyvar(const Value &group, std::pmr::memory_resource *mr)
{
    const size_t N = group.numel();
    if (N == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Find unique labels, sorted ascending (matches MATLAB).
    std::vector<double> uniq;
    uniq.reserve(N);
    for (size_t i = 0; i < N; ++i) uniq.push_back(group.elemAsDouble(i));
    std::sort(uniq.begin(), uniq.end());
    uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
    const size_t K = uniq.size();

    Value out = Value::matrix(N, K, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N * K; ++i) od[i] = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double li = group.elemAsDouble(i);
        // Linear search across at most K labels.
        for (size_t k = 0; k < K; ++k) {
            if (uniq[k] == li) { od[i + k * N] = 1.0; break; }
        }
    }
    return out;
}

} // namespace numkit::stats
