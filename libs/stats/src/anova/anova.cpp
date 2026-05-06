// libs/stats/src/anova/anova.cpp

#include <numkit/stats/anova/anova.hpp>

#include <numkit/stats/distributions/fisher_f.hpp>
#include <numkit/stats/distributions/chi2.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

namespace {

// Bucket observations by group label (preserve first-seen order to match
// MATLAB-style ascending numeric order most of the time; for strict
// matches, callers can sort labels first).
struct Group {
    double label;
    std::vector<double> values;
};

std::vector<Group> bucket(const Value &y, const Value &group)
{
    const size_t N = y.numel();
    if (group.numel() != N)
        throw Error("anova1: y and group must be the same length",
                    0, 0, "anova1", "", "m:anova1:size");
    std::vector<Group> g;
    for (size_t i = 0; i < N; ++i) {
        const double yi = y.elemAsDouble(i);
        const double li = group.elemAsDouble(i);
        if (std::isnan(yi) || std::isnan(li)) continue;
        bool found = false;
        for (auto &gg : g) if (gg.label == li) { gg.values.push_back(yi); found = true; break; }
        if (!found) g.push_back({li, {yi}});
    }
    // Sort by label ascending — matches MATLAB's anova1 default ordering.
    std::sort(g.begin(), g.end(),
              [](const Group &a, const Group &b) { return a.label < b.label; });
    return g;
}

} // anonymous

std::tuple<double, double, double, double, double, double>
anova1(std::pmr::memory_resource *mr, const Value &y, const Value &group)
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
    const double cdf = fcdf(mr, Fv, dfB, dfW).toScalar();
    const double p = std::max(0.0, 1.0 - cdf);
    return std::make_tuple(p, F, dfB, dfW, ssB, ssW);
}

std::tuple<Value, Value, Value, Value>
kruskalwallis(std::pmr::memory_resource *mr, const Value &y, const Value &group)
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
    const double cdf = chi2cdf(mr, Hv, df).toScalar();
    const double p = std::max(0.0, 1.0 - cdf);

    return std::make_tuple(Value::scalar(p, mr),
                           Value::scalar(H, mr),
                           Value::scalar(df, mr),
                           Value::scalar(sumR2_n, mr));
}

Value dummyvar(std::pmr::memory_resource *mr, const Value &group)
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

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void anova1_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("anova1: requires (y, group[, 'off'])",
                    0, 0, "anova1", "", "m:anova1:nargin");
    auto *mr = ctx.engine->resource();
    auto [p, F, dfB, dfW, ssB, ssW] = anova1(mr, args[0], args[1]);
    outs[0] = Value::scalar(p, mr);
    if (nargout > 1) {
        // 4×6 cell table { 'Source','SS','df','MS','F','Prob>F'
        //                  'Groups',ssB,dfB,ssB/dfB,F,p
        //                  'Error', ssW,dfW,ssW/dfW,[],[]
        //                  'Total', ssB+ssW,dfB+dfW,[],[],[] }.
        Value tbl = Value::cell(4, 6, mr);
        const double nan = std::numeric_limits<double>::quiet_NaN();
        auto strV = [&](const char *s){ return Value::fromString(s, mr); };
        auto numV = [&](double v){ return Value::scalar(v, mr); };
        auto setCell = [&](size_t r, size_t c, const Value &v) {
            // column-major linear index for 4×6 cell: r + c*4
            tbl.cellAt(r + c * 4) = v;
        };
        setCell(0, 0, strV("Source"));
        setCell(0, 1, strV("SS"));
        setCell(0, 2, strV("df"));
        setCell(0, 3, strV("MS"));
        setCell(0, 4, strV("F"));
        setCell(0, 5, strV("Prob>F"));
        setCell(1, 0, strV("Groups"));
        setCell(1, 1, numV(ssB));
        setCell(1, 2, numV(dfB));
        setCell(1, 3, numV(dfB > 0 ? ssB / dfB : nan));
        setCell(1, 4, numV(F));
        setCell(1, 5, numV(p));
        setCell(2, 0, strV("Error"));
        setCell(2, 1, numV(ssW));
        setCell(2, 2, numV(dfW));
        setCell(2, 3, numV(dfW > 0 ? ssW / dfW : nan));
        setCell(2, 4, numV(nan));
        setCell(2, 5, numV(nan));
        setCell(3, 0, strV("Total"));
        setCell(3, 1, numV(ssB + ssW));
        setCell(3, 2, numV(dfB + dfW));
        setCell(3, 3, numV(nan));
        setCell(3, 4, numV(nan));
        setCell(3, 5, numV(nan));
        outs[1] = std::move(tbl);
    }
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("F")        = Value::scalar(F, mr);
        s.field("df")       = Value::scalar(dfW, mr);
        outs[2] = std::move(s);
    }
}

void kruskalwallis_reg(Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("kruskalwallis: requires (y[, group][, 'off'])",
                    0, 0, "kruskalwallis", "", "m:kruskalwallis:nargin");
    auto *mr = ctx.engine->resource();

    // Matrix-only form: when group is omitted (or the 2nd arg is the
    // 'off'/'on' display flag string), infer groups from columns. Build
    // a flat y column and a same-length group index column.
    Value yArg = args[0];
    Value gArg;
    bool haveExplicitGroup = false;
    size_t flagPos = 1;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()
        && !args[1].isEmpty()) {
        gArg = args[1];
        haveExplicitGroup = true;
        flagPos = 2;
    }
    if (!haveExplicitGroup && !args[0].dims().isVector() && !args[0].isScalar()) {
        const auto &dd = args[0].dims();
        const size_t R = dd.rows(), C = dd.cols();
        Value yFlat = Value::matrix(R * C, 1, ValueType::DOUBLE, mr);
        Value gFlat = Value::matrix(R * C, 1, ValueType::DOUBLE, mr);
        double *yp = yFlat.doubleDataMut();
        double *gp = gFlat.doubleDataMut();
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                yp[c * R + r] = args[0].elemAsDouble(c * R + r);
                gp[c * R + r] = static_cast<double>(c + 1);
            }
        yArg = std::move(yFlat);
        gArg = std::move(gFlat);
    } else if (!haveExplicitGroup) {
        throw Error("kruskalwallis: vector y requires explicit group argument",
                    0, 0, "kruskalwallis", "", "m:kruskalwallis:noGroup");
    }
    (void)flagPos;   // 'off'/'on' display flag accepted but ignored
    auto [p, H, df, sumR2] = kruskalwallis(mr, yArg, gArg);
    outs[0] = std::move(p);
    if (nargout > 1) {
        // 4×6 cell table { Source, SS, df, MS, Chi-sq, Prob>Chi-sq } —
        // mirror MATLAB shape; SS / MS pieces aren't strictly defined for
        // the K-W rank statistic, but populate what we can.
        Value tbl = Value::cell(4, 6, mr);
        const double nan = std::numeric_limits<double>::quiet_NaN();
        auto strV = [&](const char *s){ return Value::fromString(s, mr); };
        auto numV = [&](double v){ return Value::scalar(v, mr); };
        auto setCell = [&](size_t r, size_t c, const Value &v) {
            tbl.cellAt(r + c * 4) = v;
        };
        setCell(0, 0, strV("Source"));
        setCell(0, 1, strV("SS"));
        setCell(0, 2, strV("df"));
        setCell(0, 3, strV("MS"));
        setCell(0, 4, strV("Chi-sq"));
        setCell(0, 5, strV("Prob>Chi-sq"));
        setCell(1, 0, strV("Groups"));
        setCell(1, 1, sumR2);
        setCell(1, 2, df);
        setCell(1, 3, numV(nan));
        setCell(1, 4, H);
        setCell(1, 5, p);
        setCell(2, 0, strV("Error"));
        for (size_t c = 1; c < 6; ++c) setCell(2, c, numV(nan));
        setCell(3, 0, strV("Total"));
        for (size_t c = 1; c < 6; ++c) setCell(3, c, numV(nan));
        outs[1] = std::move(tbl);
    }
    if (nargout > 2) {
        // Compute per-group n[] and meanRanks[] for the documented stats
        // struct shape {gnames, n, source, meanranks, sumt}.
        const size_t N = yArg.numel();
        struct Pair { double v; size_t idx; double g; };
        std::vector<Pair> all;
        all.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            const double v = yArg.elemAsDouble(i);
            const double g = gArg.elemAsDouble(i);
            if (std::isnan(v) || std::isnan(g)) continue;
            all.push_back({v, i, g});
        }
        const size_t M = all.size();
        std::vector<size_t> ord(M);
        for (size_t i = 0; i < M; ++i) ord[i] = i;
        std::sort(ord.begin(), ord.end(),
                  [&](size_t a, size_t b){ return all[a].v < all[b].v; });
        std::vector<double> rk(M);
        std::vector<size_t> tieSizes;
        size_t i = 0;
        while (i < M) {
            size_t j = i + 1;
            while (j < M && all[ord[j]].v == all[ord[i]].v) ++j;
            const double avg = static_cast<double>(i + j + 1) / 2.0;
            for (size_t k = i; k < j; ++k) rk[ord[k]] = avg;
            if (j - i > 1) tieSizes.push_back(j - i);
            i = j;
        }
        // Bucket per group label (sorted unique).
        std::vector<double> uniq;
        uniq.reserve(M);
        for (auto &p : all) uniq.push_back(p.g);
        std::sort(uniq.begin(), uniq.end());
        uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
        const size_t K = uniq.size();
        std::vector<size_t> n(K, 0);
        std::vector<double> sumR(K, 0.0);
        for (size_t k = 0; k < M; ++k) {
            auto it = std::lower_bound(uniq.begin(), uniq.end(), all[k].g);
            const size_t gi = static_cast<size_t>(it - uniq.begin());
            n[gi] += 1;
            sumR[gi] += rk[k];
        }
        double sumt = 0.0;
        for (size_t t : tieSizes) {
            const double td = static_cast<double>(t);
            sumt += td * td * td - td;
        }

        Value nVec = Value::matrix(K, 1, ValueType::DOUBLE, mr);
        Value mrVec = Value::matrix(K, 1, ValueType::DOUBLE, mr);
        Value gnames = Value::cell(K, 1, mr);
        for (size_t k = 0; k < K; ++k) {
            nVec.doubleDataMut()[k] = static_cast<double>(n[k]);
            mrVec.doubleDataMut()[k] = (n[k] > 0) ? sumR[k] / static_cast<double>(n[k]) : 0.0;
            // gnames as decimal string of the label (matches MATLAB)
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%g", uniq[k]);
            gnames.cellAt(k) = Value::fromString(buf, mr);
        }
        Value s = Value::structure(mr);
        s.field("gnames")    = std::move(gnames);
        s.field("n")         = std::move(nVec);
        s.field("source")    = Value::fromString("kruskalwallis", mr);
        s.field("meanranks") = std::move(mrVec);
        s.field("sumt")      = Value::scalar(sumt, mr);
        // Keep legacy fields for callers that already used them.
        s.field("chi2stat")  = H;
        s.field("df")        = df;
        outs[2] = std::move(s);
    }
}

void dummyvar_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dummyvar: requires GROUP",
                    0, 0, "dummyvar", "", "m:dummyvar:nargin");
    outs[0] = dummyvar(ctx.engine->resource(), args[0]);
}

} // namespace detail
} // namespace numkit::stats
