// src/builtin/src/datafun/group.cpp
//
// Pure C++ group-based data operations:
//   findgroups     — assign 1-based group IDs to each element
//   groupcounts    — count elements per group
//   groupsummary   — per-group, per-column reduction
//   grouptransform — per-group data transformation

#include <numkit/builtin/datafun.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace numkit::builtin {

namespace {

// Build a sorted-unique value list with a parallel "group id per
// element" vector (1-based). Operates on numeric input via
// elemAsDouble (CHAR / LOGICAL fall through naturally).
//
// out_groups[i] is the group id of g[i]; out_unique is the sorted
// unique value list (length = max(out_groups)). NaN entries get
// out_groups[i] = 0 (sentinel for "missing"). The caller decides
// what to do with them (findgroups -> NaN; groupcounts -> trailing
// NaN bucket).
void groupOf(const Value &g, std::vector<std::size_t> &out_groups,
             std::vector<double> &out_unique)
{
    const std::size_t n = g.numel();
    out_groups.assign(n, 0);
    if (n == 0) { out_unique.clear(); return; }

    std::vector<double> vals(n);
    for (std::size_t i = 0; i < n; ++i) vals[i] = g.elemAsDouble(i);

    // Build sorted-unique list of finite values (skip NaN — handled
    // separately by callers).
    std::vector<double> sorted;
    sorted.reserve(n);
    for (double v : vals)
        if (!std::isnan(v)) sorted.push_back(v);
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end(),
                              [](double a, double b) {
                                  return a == b;
                              }), sorted.end());
    out_unique = sorted;
    for (std::size_t i = 0; i < n; ++i) {
        if (std::isnan(vals[i])) { out_groups[i] = 0; continue; }
        const auto it = std::lower_bound(sorted.begin(), sorted.end(), vals[i]);
        out_groups[i] = (std::size_t)(it - sorted.begin()) + 1;   // 1-based
    }
}

} // namespace

// ── Public C++ API ──────────────────────────────────────────────────────────

FindgroupsResult findgroups(const Value &g, std::pmr::memory_resource *mr)
{
    std::vector<std::size_t> groups;
    std::vector<double> uniqueVals;
    groupOf(g, groups, uniqueVals);
    auto G = Value::matrix(g.dims().rows(), g.dims().cols(),
                           ValueType::DOUBLE, mr);
    double *gd = G.doubleDataMut();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t i = 0; i < groups.size(); ++i)
        gd[i] = (groups[i] == 0) ? nan : double(groups[i]);
    auto ID = Value::matrix(uniqueVals.size(), 1, ValueType::DOUBLE, mr);
    if (!uniqueVals.empty())
        std::memcpy(ID.doubleDataMut(), uniqueVals.data(),
                    uniqueVals.size() * sizeof(double));
    return {std::move(G), std::move(ID)};
}

GroupcountsResult groupcounts(const Value &g, std::pmr::memory_resource *mr)
{
    std::vector<std::size_t> groups;
    std::vector<double> uniqueVals;
    groupOf(g, groups, uniqueVals);
    std::size_t nan_count = 0;
    for (auto gg : groups) if (gg == 0) ++nan_count;
    const bool have_nan = nan_count > 0;
    const std::size_t nGroups = uniqueVals.size() + (have_nan ? 1 : 0);
    GroupcountsResult R;
    if (nGroups == 0) {
        R.C  = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.GR = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        R.P  = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        return R;
    }
    std::vector<std::size_t> counts(nGroups, 0);
    for (auto gg : groups) {
        if (gg == 0) counts[uniqueVals.size()]++;  // trailing NaN bucket
        else         counts[gg - 1]++;
    }
    R.C = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
    {
        double *dst = R.C.doubleDataMut();
        for (std::size_t i = 0; i < nGroups; ++i) dst[i] = double(counts[i]);
    }
    R.GR = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
    {
        double *gd = R.GR.doubleDataMut();
        for (std::size_t i = 0; i < uniqueVals.size(); ++i) gd[i] = uniqueVals[i];
        if (have_nan)
            gd[uniqueVals.size()] = std::numeric_limits<double>::quiet_NaN();
    }
    const double total = double(groups.size());
    R.P = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
    {
        double *pd = R.P.doubleDataMut();
        for (std::size_t i = 0; i < nGroups; ++i)
            pd[i] = (total > 0.0) ? 100.0 * double(counts[i]) / total : 0.0;
    }
    return R;
}

GroupsummaryResult groupsummary(const Value &A, const Value &G,
                                const std::string &method,
                                std::pmr::memory_resource *mr)
{
    const std::size_t nRows = A.dims().rows();
    const std::size_t nCols = (A.dims().ndim() >= 2) ? A.dims().cols() : 1;
    if (G.numel() != nRows)
        throw Error("groupsummary: groupvars must have length size(A, 1)",
                    0, 0, "groupsummary", "", "numkit:groupsummary:shape");

    // Group the rows.
    std::vector<std::size_t> groups;
    std::vector<double> uniqueVals;
    groupOf(G, groups, uniqueVals);
    std::size_t nan_count = 0;
    for (auto g : groups) if (g == 0) ++nan_count;
    const bool have_nan = nan_count > 0;
    const std::size_t nGroups = uniqueVals.size() + (have_nan ? 1 : 0);

    // Bucket row indices by group ID (0 = NaN bucket -> trailing).
    std::vector<std::vector<std::size_t>> buckets(nGroups);
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const std::size_t gi = (groups[i] == 0) ? uniqueVals.size()
                                                : (groups[i] - 1);
        buckets[gi].push_back(i);
    }

    auto B = (nCols == 1) ? Value::matrix(nGroups, 1, ValueType::DOUBLE, mr)
                          : Value::matrix(nGroups, nCols, ValueType::DOUBLE, mr);
    double *bd = B.doubleDataMut();
    auto Aget = [&](std::size_t r, std::size_t c) {
        return A.elemAsDouble(r + c * nRows);
    };

    for (std::size_t c = 0; c < nCols; ++c) {
        for (std::size_t g = 0; g < nGroups; ++g) {
            const auto &rows = buckets[g];
            const std::size_t kn = rows.size();
            double out = 0.0;
            if (method == "sum") {
                for (auto r : rows) out += Aget(r, c);
            } else if (method == "mean") {
                if (kn == 0) out = std::numeric_limits<double>::quiet_NaN();
                else {
                    double s = 0.0;
                    for (auto r : rows) s += Aget(r, c);
                    out = s / double(kn);
                }
            } else if (method == "median") {
                std::vector<double> v; v.reserve(kn);
                for (auto r : rows) v.push_back(Aget(r, c));
                std::sort(v.begin(), v.end());
                out = (kn == 0)            ? std::numeric_limits<double>::quiet_NaN()
                    : (kn % 2 == 1)        ? v[kn / 2]
                                            : 0.5 * (v[kn / 2 - 1] + v[kn / 2]);
            } else if (method == "max") {
                out = -std::numeric_limits<double>::infinity();
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (v > out) out = v;
                }
                if (kn == 0) out = std::numeric_limits<double>::quiet_NaN();
            } else if (method == "min") {
                out = std::numeric_limits<double>::infinity();
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (v < out) out = v;
                }
                if (kn == 0) out = std::numeric_limits<double>::quiet_NaN();
            } else if (method == "std" || method == "var") {
                if (kn < 2) out = std::numeric_limits<double>::quiet_NaN();
                else {
                    double s = 0.0, ss = 0.0;
                    for (auto r : rows) { double v = Aget(r, c); s += v; ss += v*v; }
                    const double m = s / double(kn);
                    const double v = (ss - double(kn) * m * m) / double(kn - 1);
                    out = (method == "var") ? std::max(0.0, v)
                                             : std::sqrt(std::max(0.0, v));
                }
            } else if (method == "numunique") {
                std::vector<double> v; v.reserve(kn);
                for (auto r : rows) v.push_back(Aget(r, c));
                std::sort(v.begin(), v.end());
                v.erase(std::unique(v.begin(), v.end()), v.end());
                out = double(v.size());
            } else if (method == "nnz") {
                std::size_t k = 0;
                for (auto r : rows) if (Aget(r, c) != 0.0) ++k;
                out = double(k);
            } else if (method == "mode") {
                std::vector<double> v; v.reserve(kn);
                for (auto r : rows) v.push_back(Aget(r, c));
                std::sort(v.begin(), v.end());
                std::size_t best_run = 0, cur_run = 0;
                double best_val = std::numeric_limits<double>::quiet_NaN();
                for (std::size_t i = 0; i < v.size(); ++i) {
                    if (i == 0 || v[i] != v[i - 1]) cur_run = 1;
                    else                            ++cur_run;
                    if (cur_run > best_run) { best_run = cur_run; best_val = v[i]; }
                }
                out = best_val;
            } else if (method == "all") {
                bool ok = true;
                for (auto r : rows) if (Aget(r, c) == 0.0) { ok = false; break; }
                out = ok ? 1.0 : 0.0;
            } else if (method == "any") {
                bool ok = false;
                for (auto r : rows) if (Aget(r, c) != 0.0) { ok = true; break; }
                out = ok ? 1.0 : 0.0;
            } else {
                throw Error("groupsummary: method '" + method
                            + "' not supported in this revision",
                            0, 0, "groupsummary", "",
                            "numkit:groupsummary:badMethod");
            }
            bd[g + c * nGroups] = out;
        }
    }

    GroupsummaryResult R;
    R.B = std::move(B);
    R.BG = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
    {
        double *gd = R.BG.doubleDataMut();
        for (std::size_t i = 0; i < uniqueVals.size(); ++i) gd[i] = uniqueVals[i];
        if (have_nan)
            gd[uniqueVals.size()] = std::numeric_limits<double>::quiet_NaN();
    }
    R.BC = Value::matrix(nGroups, 1, ValueType::DOUBLE, mr);
    {
        double *cd = R.BC.doubleDataMut();
        for (std::size_t g = 0; g < nGroups; ++g) cd[g] = double(buckets[g].size());
    }
    return R;
}

Value grouptransform(const Value &A, const Value &G,
                     const std::string &method,
                     std::pmr::memory_resource *mr)
{
    const std::size_t nRows = A.dims().rows();
    const std::size_t nCols = (A.dims().ndim() >= 2) ? A.dims().cols() : 1;
    if (G.numel() != nRows)
        throw Error("grouptransform: groupvars must have length size(A,1)",
                    0, 0, "grouptransform", "",
                    "numkit:grouptransform:shape");

    if (method != "zscore" && method != "norm" &&
        method != "meancenter" && method != "rescale" &&
        method != "meanfill" && method != "linearfill")
    {
        throw Error("grouptransform: method must be 'zscore', 'norm', "
                    "'meancenter', 'rescale', 'meanfill', or 'linearfill'",
                    0, 0, "grouptransform", "",
                    "numkit:grouptransform:badMethod");
    }

    std::vector<std::size_t> groups;
    std::vector<double> uniqueVals;
    groupOf(G, groups, uniqueVals);
    std::size_t nan_count = 0;
    for (auto g : groups) if (g == 0) ++nan_count;
    const bool have_nan = nan_count > 0;
    const std::size_t nGroups = uniqueVals.size() + (have_nan ? 1 : 0);
    std::vector<std::vector<std::size_t>> buckets(nGroups);
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const std::size_t gi = (groups[i] == 0) ? uniqueVals.size()
                                                : (groups[i] - 1);
        buckets[gi].push_back(i);
    }

    auto B = (nCols == 1) ? Value::matrix(nRows, 1, ValueType::DOUBLE, mr)
                          : Value::matrix(nRows, nCols, ValueType::DOUBLE, mr);
    double *bd = B.doubleDataMut();
    for (std::size_t i = 0; i < nRows * nCols; ++i)
        bd[i] = std::numeric_limits<double>::quiet_NaN();

    auto Aget = [&](std::size_t r, std::size_t c) {
        return A.elemAsDouble(r + c * nRows);
    };

    for (std::size_t c = 0; c < nCols; ++c) {
        for (const auto &rows : buckets) {
            const std::size_t kn = rows.size();
            if (kn == 0) continue;

            if (method == "meancenter" || method == "zscore" ||
                method == "norm") {
                double sum = 0.0, sq = 0.0;
                std::size_t k = 0;
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (!std::isnan(v)) { sum += v; sq += v*v; ++k; }
                }
                const double mean = (k > 0) ? sum / double(k) : 0.0;
                if (method == "meancenter") {
                    for (auto r : rows)
                        bd[r + c * nRows] = Aget(r, c) - mean;
                } else if (method == "zscore") {
                    const double var = (k > 1)
                        ? (sq - double(k) * mean * mean) / double(k - 1)
                        : 0.0;
                    const double sd  = std::sqrt(std::max(0.0, var));
                    for (auto r : rows)
                        bd[r + c * nRows] = (sd > 0.0)
                            ? (Aget(r, c) - mean) / sd
                            : 0.0;
                } else {  // norm
                    const double nm = std::sqrt(sq);
                    for (auto r : rows)
                        bd[r + c * nRows] = (nm > 0.0)
                            ? Aget(r, c) / nm : 0.0;
                }
            } else if (method == "rescale") {
                double lo = std::numeric_limits<double>::infinity();
                double hi = -std::numeric_limits<double>::infinity();
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (std::isnan(v)) continue;
                    if (v < lo) lo = v;
                    if (v > hi) hi = v;
                }
                const double range = hi - lo;
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (std::isnan(v))            bd[r + c * nRows] = v;
                    else if (range == 0.0)         bd[r + c * nRows] = 0.0;
                    else                            bd[r + c * nRows] = (v - lo) / range;
                }
            } else if (method == "meanfill") {
                double sum = 0.0;
                std::size_t k = 0;
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    if (!std::isnan(v)) { sum += v; ++k; }
                }
                const double mean = (k > 0) ? sum / double(k)
                                            : std::numeric_limits<double>::quiet_NaN();
                for (auto r : rows) {
                    const double v = Aget(r, c);
                    bd[r + c * nRows] = std::isnan(v) ? mean : v;
                }
            } else if (method == "linearfill") {
                std::vector<double> vals(kn);
                for (std::size_t j = 0; j < kn; ++j) vals[j] = Aget(rows[j], c);
                std::vector<std::size_t> good;
                good.reserve(kn);
                for (std::size_t j = 0; j < kn; ++j)
                    if (!std::isnan(vals[j])) good.push_back(j);
                std::vector<double> out = vals;
                if (good.size() >= 2) {
                    for (std::size_t k = 0; k + 1 < good.size(); ++k) {
                        const std::size_t a = good[k];
                        const std::size_t b = good[k + 1];
                        if (b == a + 1) continue;
                        const double slope = (vals[b] - vals[a]) / double(b - a);
                        for (std::size_t j = a + 1; j < b; ++j)
                            out[j] = vals[a] + slope * double(j - a);
                    }
                    if (good.front() > 0) {
                        const std::size_t a = good[0];
                        const std::size_t b = good[1];
                        const double slope = (vals[b] - vals[a]) / double(b - a);
                        for (std::size_t j = 0; j < a; ++j)
                            out[j] = vals[a] - slope * double(a - j);
                    }
                    if (good.back() + 1 < kn) {
                        const std::size_t a = good[good.size() - 2];
                        const std::size_t b = good[good.size() - 1];
                        const double slope = (vals[b] - vals[a]) / double(b - a);
                        for (std::size_t j = b + 1; j < kn; ++j)
                            out[j] = vals[b] + slope * double(j - b);
                    }
                }
                for (std::size_t j = 0; j < kn; ++j)
                    bd[rows[j] + c * nRows] = out[j];
            }
        }
    }
    return B;
}

} // namespace numkit::builtin
