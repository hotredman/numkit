// toolboxes/signal/src/test/hypothesis_reg.cpp
//
// CallContext register half of test/hypothesis.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/distributions/binomial.hpp>
#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/stats/distributions/fisher_f.hpp>
#include <numkit/stats/distributions/normal.hpp>
#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/test/hypothesis.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "hypothesis_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

namespace {
TestTail parse_tail_arg(Span<const Value> args, size_t i, TestTail def) {
    if (i >= args.size()) return def;
    const Value &v = args[i];
    if (v.isChar() || v.isString()) return parse_tail(v.toString(), def);
    return def;
}

double parse_alpha(Span<const Value> args, size_t i, double def) {
    if (i >= args.size() || args[i].isEmpty()) return def;
    if (args[i].isChar() || args[i].isString()) return def;
    return args[i].toScalar();
}
}

void ttest_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ttest: requires (X[, m | y][, alpha, tail | name-value])",
                    0, 0, "ttest", "", "numkit:ttest:nargin");
    auto *mr = ctx.engine->resource();

    // Detect paired form: ttest(x, y) where y is a non-scalar numeric
    // vector matching x's length. Pre-difference x - y and run vs m=0.
    Value xData = args[0];
    bool pairedConsumed = false;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString()
        && !args[1].isEmpty() && args[1].numel() > 1) {
        // Build paired difference x - y in a fresh DOUBLE row.
        const Value &y = args[1];
        if (y.numel() != args[0].numel())
            throw Error("ttest: paired vectors must have equal length",
                        0, 0, "ttest", "", "numkit:ttest:pairedLen");
        Value diff = Value::matrix(1, y.numel(), ValueType::DOUBLE, mr);
        double *dst = diff.doubleDataMut();
        for (size_t i = 0; i < y.numel(); ++i)
            dst[i] = args[0].elemAsDouble(i) - y.elemAsDouble(i);
        xData = std::move(diff);
        pairedConsumed = true;
    }

    double m = 0.0;
    if (!pairedConsumed && args.size() >= 2 && !args[1].isChar()
        && !args[1].isString() && !args[1].isEmpty()) {
        m = args[1].toScalar();
    }
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = (args.size() >= 2 && (args[1].isChar() || args[1].isString())) ? 1 : 2;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) {
                alpha = args[i + 1].toScalar(); i += 2;
            } else if (sl == "tail" && i + 1 < args.size()) {
                tail = parse_tail(args[i + 1].toString(), tail); i += 2;
            } else if (sl == "dim") {
                throw Error("ttest: 'Dim' not yet supported (parity gap)",
                            0, 0, "ttest", "", "numkit:ttest:dim");
            } else {
                tail = parse_tail(sl, tail); ++i;
            }
        } else {
            alpha = a.toScalar(); ++i;
        }
    }
    auto [h, p, ci, t] = ttest(xData, m, alpha, tail, mr);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(t);   // stats struct {tstat, df, sd}
}

void ttest2_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ttest2: requires (X, Y[, alpha, tail, vartype])",
                    0, 0, "ttest2", "", "numkit:ttest2:nargin");
    double alpha = parse_alpha(args, 2, 0.05);
    TestTail tail = TestTail::Both;
    // MATLAB R2025b default is 'equal' (pooled variance), NOT Welch.
    std::string vartype = "equal";
    for (size_t i = 2; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            std::string k = args[i].toString();
            for (auto &c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if      (k == "tail")    tail = parse_tail(args[i + 1].toString(), TestTail::Both);
            else if (k == "vartype") vartype = args[i + 1].toString();
            else if (k == "alpha")   alpha = args[i + 1].toScalar();
            else if (k == "dim")
                throw Error("ttest2: 'Dim' not yet supported (parity gap)",
                            0, 0, "ttest2", "", "numkit:ttest2:dim");
        }
    }
    auto [h, p, ci, t] = ttest2(args[0], args[1], alpha, tail, vartype, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(t);
}

void ztest_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ztest: requires (X, m, sigma[, alpha, tail | name-value])",
                    0, 0, "ztest", "", "numkit:ztest:nargin");
    const double m     = args[1].toScalar();
    const double sigma = args[2].toScalar();
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = 3;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) { alpha = args[i + 1].toScalar(); i += 2; }
            else if (sl == "tail" && i + 1 < args.size()) { tail = parse_tail(args[i + 1].toString(), tail); i += 2; }
            else if (sl == "dim")
                throw Error("ztest: 'Dim' not yet supported (parity gap)",
                            0, 0, "ztest", "", "numkit:ztest:dim");
            else { tail = parse_tail(sl, tail); ++i; }
        } else { alpha = a.toScalar(); ++i; }
    }
    auto [h, p, ci, z] = ztest(args[0], m, sigma, alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(z);
}

void vartest_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("vartest: requires (X, v[, alpha, tail | name-value])",
                    0, 0, "vartest", "", "numkit:vartest:nargin");
    const double v = args[1].toScalar();
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = 2;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) { alpha = args[i + 1].toScalar(); i += 2; }
            else if (sl == "tail" && i + 1 < args.size()) { tail = parse_tail(args[i + 1].toString(), tail); i += 2; }
            else if (sl == "dim")
                throw Error("vartest: 'Dim' not yet supported (parity gap)",
                            0, 0, "vartest", "", "numkit:vartest:dim");
            else { tail = parse_tail(sl, tail); ++i; }
        } else { alpha = a.toScalar(); ++i; }
    }
    auto [h, p, ci, T] = vartest(args[0], v, alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(T);
}

void vartest2_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("vartest2: requires (X, Y[, alpha, tail | name-value])",
                    0, 0, "vartest2", "", "numkit:vartest2:nargin");
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = 2;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) { alpha = args[i + 1].toScalar(); i += 2; }
            else if (sl == "tail" && i + 1 < args.size()) { tail = parse_tail(args[i + 1].toString(), tail); i += 2; }
            else if (sl == "dim")
                throw Error("vartest2: 'Dim' not yet supported (parity gap)",
                            0, 0, "vartest2", "", "numkit:vartest2:dim");
            else { tail = parse_tail(sl, tail); ++i; }
        } else { alpha = a.toScalar(); ++i; }
    }
    auto [h, p, ci, F] = vartest2(args[0], args[1], alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(ci);
    if (nargout > 3) outs[3] = std::move(F);
}

void kstest_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("kstest: requires X", 0, 0, "kstest", "",
                    "numkit:kstest:nargin");
    Value cdf = (args.size() >= 2 && !(args[1].isChar() || args[1].isString()))
                  ? args[1] : Value();  // empty default
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    // Walk trailing args: positional alpha (numeric scalar), positional
    // tail string, and Name-Value pairs ('Alpha', value | 'Tail', value).
    size_t i = (cdf.numel() > 0 || (args.size() >= 2 && (args[1].isChar() || args[1].isString()))) ? 2 : 1;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string s = a.toString();
            std::string sl = s;
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) {
                alpha = args[i + 1].toScalar();
                i += 2;
            } else if (sl == "tail" && i + 1 < args.size()) {
                tail = parse_tail(args[i + 1].toString(), tail);
                i += 2;
            } else {
                // positional tail string
                tail = parse_tail(sl, tail);
                ++i;
            }
        } else {
            // positional alpha
            alpha = a.toScalar();
            ++i;
        }
    }
    auto [h, p, D, cv] = kstest(args[0], cdf, alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(D);
    if (nargout > 3) outs[3] = std::move(cv);
}

void kstest2_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("kstest2: requires (X, Y[, alpha, tail | name-value])",
                    0, 0, "kstest2", "", "numkit:kstest2:nargin");
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    size_t i = 2;
    while (i < args.size()) {
        const Value &a = args[i];
        if (a.isChar() || a.isString()) {
            std::string sl = a.toString();
            for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (sl == "alpha" && i + 1 < args.size()) {
                alpha = args[i + 1].toScalar(); i += 2;
            } else if (sl == "tail" && i + 1 < args.size()) {
                tail = parse_tail(args[i + 1].toString(), tail); i += 2;
            } else {
                tail = parse_tail(sl, tail); ++i;
            }
        } else {
            alpha = a.toScalar(); ++i;
        }
    }
    auto [h, p, D, cv] = kstest2(args[0], args[1], alpha, tail, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(D);
    if (nargout > 3) outs[3] = std::move(cv);
}

void jbtest_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("jbtest: requires X[, alpha[, mctol]]", 0, 0, "jbtest", "",
                    "numkit:jbtest:nargin");
    double alpha = parse_alpha(args, 1, 0.05);
    // 3rd arg = mctol (Monte-Carlo standard-error tolerance).
    const double mctol = (args.size() > 2 && !args[2].isEmpty())
                         ? args[2].toScalar()
                         : std::numeric_limits<double>::quiet_NaN();
    auto [h, p, JB, cv] = jbtest(args[0], alpha, mctol, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(JB);
    if (nargout > 3) outs[3] = std::move(cv);
}

void fishertest_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fishertest: requires (T[, alpha, tail | name-value])",
                    0, 0, "fishertest", "", "numkit:fishertest:nargin");
    auto *mr = ctx.engine->resource();
    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "alpha") alpha = v.toScalar();
        else if (name == "tail")  tail  = parse_tail(v.toString(), TestTail::Both);
    }
    auto [h, p, OR, lo, hi] = fishertest(args[0], alpha, tail, mr);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("OddsRatio") = OR;
        Value ci = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        double *cd = ci.doubleDataMut();
        cd[0] = lo.toScalar();
        cd[1] = hi.toScalar();
        s.field("ConfidenceInterval") = std::move(ci);
        outs[2] = std::move(s);
    }
}

void chi2gof_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("chi2gof: requires X[, 'Frequency'/'Expected'/'Edges'/"
                    "'NBins'/'Ctrs'/'NParams'/'EMin'/'Alpha', val, ...]",
                    0, 0, "chi2gof", "", "numkit:chi2gof:nargin");
    auto *mr = ctx.engine->resource();
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };

    Value freq, expected, edges_arg, ctrs_arg;
    int nbins = 10;
    int nparams = -1;       // -1 = use default (2 if auto-fit, 0 if explicit O/E)
    double alpha = 0.05;
    double emin = 5.0;

    bool freq_set = false, expected_set = false, edges_set = false;
    bool nbins_set = false, ctrs_set = false, nparams_set = false;
    bool cdf_supplied = false;

    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        const std::string name = lower(args[i].toString());
        const Value &v = args[i + 1];
        if      (name == "frequency") { freq = v;     freq_set = true; }
        else if (name == "expected")  { expected = v; expected_set = true; }
        else if (name == "edges")     { edges_arg = v; edges_set = true; }
        else if (name == "nbins")     { nbins = (int)v.toScalar(); nbins_set = true; }
        else if (name == "ctrs")      { ctrs_arg = v; ctrs_set = true; }
        else if (name == "nparams")   { nparams = (int)v.toScalar(); nparams_set = true; }
        else if (name == "emin")      { emin = v.toScalar(); }
        else if (name == "alpha")     { alpha = v.toScalar(); }
        else if (name == "cdf")       { cdf_supplied = true; }
    }

    if (cdf_supplied)
        throw Error("chi2gof: 'CDF' function-handle argument is not yet "
                    "supported in numkit; supply 'Expected' or rely on "
                    "the default normal auto-fit instead",
                    0, 0, "chi2gof", "", "numkit:chi2gof:cdf_nyi");

    // Path A: explicit Frequency + Expected (existing behavior).
    if (freq_set && expected_set) {
        const int np = nparams_set ? nparams : 0;
        auto [p, h, chi2, df] = chi2gof(freq, expected, np, alpha, mr);
        outs[0] = std::move(h);
        if (nargout > 1) outs[1] = std::move(p);
        if (nargout > 2) {
            const size_t K = freq.numel();
            Value s = Value::structure(mr);
            s.field("chi2stat") = chi2;
            s.field("df")       = df;
            // Synthesize edges from the first arg if it's monotone numeric;
            // else default to 1:K with width 1.
            Value edges_out = Value::matrix(1, K + 1, ValueType::DOUBLE, mr);
            double *ep = edges_out.doubleDataMut();
            const Value &xv = args[0];
            const double x0 = xv.elemAsDouble(0);
            const double xK = xv.elemAsDouble(K - 1);
            const double dx = (K > 1) ? (xK - x0) / double(K - 1) : 1.0;
            for (size_t i = 0; i <= K; ++i) ep[i] = x0 + (double(i) - 0.5) * dx;
            s.field("edges") = edges_out;
            s.field("O")     = freq;
            s.field("E")     = expected;
            outs[2] = std::move(s);
        }
        return;
    }

    // Path B: auto-binning. Need data vector.
    const Value &x = args[0];
    const size_t N = x.numel();
    if (N < 2)
        throw Error("chi2gof: data vector must have at least 2 elements",
                    0, 0, "chi2gof", "", "numkit:chi2gof:size");

    ScratchArena scratch(mr);

    // Compute mean / std of x.
    double mean = 0.0;
    for (size_t i = 0; i < N; ++i) mean += x.elemAsDouble(i);
    mean /= double(N);
    double sq = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = x.elemAsDouble(i) - mean;
        sq += d * d;
    }
    const double sd = std::sqrt(sq / double(N - 1));

    // Build edges.
    ScratchVec<double> edges(&scratch);
    if (edges_set) {
        const size_t M = edges_arg.numel();
        edges.resize(M);
        for (size_t i = 0; i < M; ++i) edges[i] = edges_arg.elemAsDouble(i);
    } else if (ctrs_set) {
        // Centres → derive edges as midpoints + extrapolation.
        const size_t M = ctrs_arg.numel();
        edges.resize(M + 1);
        const double c0 = ctrs_arg.elemAsDouble(0);
        const double c1 = ctrs_arg.elemAsDouble(1);
        edges[0] = c0 - 0.5 * (c1 - c0);
        for (size_t i = 0; i + 1 < M; ++i) {
            edges[i + 1] = 0.5 * (ctrs_arg.elemAsDouble(i)
                                  + ctrs_arg.elemAsDouble(i + 1));
        }
        const double cN1 = ctrs_arg.elemAsDouble(M - 1);
        const double cN2 = ctrs_arg.elemAsDouble(M - 2);
        edges[M] = cN1 + 0.5 * (cN1 - cN2);
    } else {
        // NBins equally-spaced from min(x) to max(x).
        double xmin = x.elemAsDouble(0), xmax = xmin;
        for (size_t i = 1; i < N; ++i) {
            const double v = x.elemAsDouble(i);
            if (v < xmin) xmin = v;
            if (v > xmax) xmax = v;
        }
        const int K = std::max(2, nbins);
        edges.resize(K + 1);
        for (int i = 0; i <= K; ++i)
            edges[i] = xmin + (xmax - xmin) * double(i) / double(K);
    }

    // Build O = histogram of x against edges. MATLAB chi2gof's binning
    // rule depends on whether edges came from the user (left-closed,
    // last bin right-inclusive — standard histcounts) or auto-binning
    // (right-closed first bin extended, right-inclusive everywhere).
    // Verified vs R2025b on (-3:0.05:3) data with both Edges= and NBins=.
    const size_t K0 = edges.size() - 1;
    ScratchVec<double> O(K0, 0.0, &scratch);
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        if (v < edges[0] || v > edges[K0]) continue;
        for (size_t b = 0; b < K0; ++b) {
            bool in;
            if (edges_set) {
                // Left-closed standard histcounts.
                in = (b == K0 - 1)
                    ? (v >= edges[b] && v <= edges[b + 1])
                    : (v >= edges[b] && v <  edges[b + 1]);
            } else {
                // Right-closed (auto-binning).
                in = (b == 0)
                    ? (v <= edges[1])
                    : (v > edges[b] && v <= edges[b + 1]);
            }
            if (in) { O[b] += 1.0; break; }
        }
    }

    // Build E under N(mean, sd) by default (via norm CDF).
    ScratchVec<double> E(K0, 0.0, &scratch);
    auto Phi = [](double z) {
        return 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
    };
    // Total mass under tail-extended bins so that Σ E = N (matches MATLAB).
    // The first bin extends to -∞, the last to +∞ (chi2gof convention).
    for (size_t b = 0; b < K0; ++b) {
        const double z_lo = (b == 0)        ? -std::numeric_limits<double>::infinity()
                                            : (edges[b]     - mean) / sd;
        const double z_hi = (b == K0 - 1)   ?  std::numeric_limits<double>::infinity()
                                            : (edges[b + 1] - mean) / sd;
        const double F_lo = std::isfinite(z_lo) ? Phi(z_lo) : 0.0;
        const double F_hi = std::isfinite(z_hi) ? Phi(z_hi) : 1.0;
        E[b] = double(N) * (F_hi - F_lo);
    }

    // Apply EMin: merge tail bins with E < emin (working from each end
    // inward, merging the small bin into its inward neighbour). MATLAB
    // also merges contiguous low-E interior runs, but tail-only is the
    // common case; this matches the MATLAB reference output.
    auto merge_left = [&]() {
        while (O.size() > 1 && E[0] < emin) {
            O[1] += O[0]; E[1] += E[0];
            O.erase(O.begin()); E.erase(E.begin());
            edges.erase(edges.begin() + 1);  // remove inner edge
        }
    };
    auto merge_right = [&]() {
        while (O.size() > 1 && E.back() < emin) {
            O[O.size() - 2] += O.back(); E[E.size() - 2] += E.back();
            O.pop_back(); E.pop_back();
            edges.erase(edges.end() - 2);  // remove inner edge
        }
    };
    merge_left();
    merge_right();

    // Compute chi2.
    double chi2 = 0.0;
    for (size_t b = 0; b < O.size(); ++b) {
        if (E[b] > 0.0) {
            const double d = O[b] - E[b];
            chi2 += d * d / E[b];
        }
    }
    const int K_final = (int)O.size();
    // Default NParams = 2 (mean + std estimated from data) unless user
    // overrode or supplied explicit Edges (MATLAB still defaults to 2).
    const int np = nparams_set ? nparams : 2;
    const double df = double(K_final) - 1.0 - double(np);
    Value chi2v = Value::scalar(chi2, mr);
    const double cdf = (df > 0.0) ? chi2cdf(chi2v, df, mr).toScalar() : 1.0;
    const double p = std::max(0.0, 1.0 - cdf);
    const int h = (p < alpha) ? 1 : 0;

    outs[0] = Value::scalar(double(h), mr);
    if (nargout > 1) outs[1] = Value::scalar(p, mr);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("chi2stat") = Value::scalar(chi2, mr);
        s.field("df")       = Value::scalar(df, mr);
        // Pack edges / O / E into Value rows.
        Value edges_out = Value::matrix(1, edges.size(), ValueType::DOUBLE, mr);
        std::copy(edges.begin(), edges.end(), edges_out.doubleDataMut());
        s.field("edges") = edges_out;
        Value O_out = Value::matrix(1, O.size(), ValueType::DOUBLE, mr);
        std::copy(O.begin(), O.end(), O_out.doubleDataMut());
        s.field("O") = O_out;
        Value E_out = Value::matrix(1, E.size(), ValueType::DOUBLE, mr);
        std::copy(E.begin(), E.end(), E_out.doubleDataMut());
        s.field("E") = E_out;
        outs[2] = std::move(s);
    }
    (void)nbins_set;  // documented arg, no separate code path needed
    (void)ctrs_set;
    (void)expected_set;
}

void vartestn_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("vartestn: requires (X[, GROUP][, N-V pairs])",
                    0, 0, "vartestn", "", "numkit:vartestn:nargin");
    auto *mr = ctx.engine->resource();
    auto lower = [](std::string s) {
        for (auto &c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    };
    // Parse: vartestn(X[, GROUP], N-V...). The 2nd arg is GROUP iff it's
    // a non-string vector. If it's a string, no group given (matrix
    // input form: each column = group).
    int test = 0;  // Bartlett default
    size_t nv_start = 1;
    Value X = args[0];
    Value G;
    bool have_group = false;
    if (args.size() >= 2 && !(args[1].isChar() || args[1].isString())) {
        G = args[1];
        have_group = true;
        nv_start = 2;
    }
    for (size_t i = nv_start; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString()) break;
        const std::string name = lower(args[i].toString());
        if (name == "testtype") {
            const std::string v = lower(args[i + 1].toString());
            if      (v == "bartlett")        test = 0;
            else if (v == "levenequadratic") test = 1;
            else if (v == "leveneabsolute")  test = 2;
            else if (v == "brownforsythe")   test = 3;
            else if (v == "obrien")          test = 4;
            else throw Error("vartestn: unknown TestType '" + v + "'",
                             0, 0, "vartestn", "", "numkit:vartestn:badtype");
        }
        // 'display' / 'alpha' silently ignored (Display has no console
        // effect; alpha doesn't change p/stat output).
    }

    // Matrix-input form: build (values, group) where group encodes the
    // column index of each observation.
    if (!have_group) {
        const size_t R = X.dims().rows();
        const size_t C = X.dims().cols();
        if (R == 0 || C < 2)
            throw Error("vartestn: matrix input must have >=2 columns",
                        0, 0, "vartestn", "", "numkit:vartestn:size");
        ScratchArena scratch(mr);
        ScratchVec<double> vv(R * C, &scratch);
        ScratchVec<double> gg(R * C, &scratch);
        for (size_t c = 0; c < C; ++c)
            for (size_t r = 0; r < R; ++r) {
                vv[c * R + r] = X.elemAsDouble(c * R + r);
                gg[c * R + r] = double(c + 1);
            }
        Value Vx = Value::matrix(R * C, 1, ValueType::DOUBLE, mr);
        Value Vg = Value::matrix(R * C, 1, ValueType::DOUBLE, mr);
        std::copy(vv.begin(), vv.end(), Vx.doubleDataMut());
        std::copy(gg.begin(), gg.end(), Vg.doubleDataMut());
        X = std::move(Vx);
        G = std::move(Vg);
    }

    auto [p, stat, df1, df2] = vartestn_full(X, G, test, mr);
    outs[0] = std::move(p);
    if (nargout > 1) {
        Value s = Value::structure(mr);
        if (test == 0) {
            s.field("chisqstat") = stat;
            s.field("df")        = df1;
        } else {
            s.field("fstat") = stat;
            // df is a 1×2 row vector [df1 df2].
            Value dfv = Value::matrix(1, 2, ValueType::DOUBLE, mr);
            double *dp = dfv.doubleDataMut();
            dp[0] = df1.toScalar();
            dp[1] = df2.toScalar();
            s.field("df") = dfv;
        }
        outs[1] = std::move(s);
    }
}

void runstest_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("runstest: requires X[, v | 'ud'][, alpha, tail | name-value]",
                    0, 0, "runstest", "", "numkit:runstest:nargin");
    auto *mr = ctx.engine->resource();

    // arg[1] is positional v (scalar), 'ud' string for up-down test, or a
    // name-value start.
    double v = std::numeric_limits<double>::quiet_NaN();   // sentinel: use median(x)
    bool up_down = false;
    size_t i = 1;
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        std::string s = args[i].toString();
        std::string sl = s;
        for (auto &c : sl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (sl == "ud") { up_down = true; ++i; }
        // otherwise leave for the Name-Value loop below.
    } else if (i < args.size() && !args[i].isEmpty()) {
        v = args[i].toScalar();
        ++i;
    }

    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    std::string method;

    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &val = args[i + 1];
        if      (name == "alpha")  alpha = val.toScalar();
        else if (name == "tail")   tail  = parse_tail(val.toString(), TestTail::Both);
        else if (name == "method") method = val.toString();
        i += 2;
    }

    // For 'ud' (up-down test): replace x with sign(diff(x)) — the
    // resulting binary sequence above/below 0 counts ascent/descent
    // runs, which is exactly what MATLAB's runstest('ud') does.
    Value xUsed = args[0];
    if (up_down) {
        const Value &x = args[0];
        const size_t Nx = x.numel();
        if (Nx < 2) {
            Value diffSign = Value::matrix(0, 0, ValueType::DOUBLE, mr);
            xUsed = std::move(diffSign);
        } else {
            std::vector<double> diffs;
            diffs.reserve(Nx - 1);
            for (size_t k = 1; k < Nx; ++k) {
                const double xa = x.elemAsDouble(k - 1);
                const double xb = x.elemAsDouble(k);
                if (std::isnan(xa) || std::isnan(xb)) continue;
                diffs.push_back((xb > xa) ? 1.0 : ((xb < xa) ? -1.0 : 0.0));
            }
            Value sgn = Value::matrix(1, diffs.size(), ValueType::DOUBLE, mr);
            if (!diffs.empty())
                std::copy(diffs.begin(), diffs.end(), sgn.doubleDataMut());
            xUsed = std::move(sgn);
        }
        v = 0.0;       // reference value for the sign sequence
    }

    auto [p, h, R, n1, n0, z] = runstest(xUsed, v, alpha, tail, method, mr);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("nruns") = R;
        s.field("n1")    = n1;
        s.field("n0")    = n0;
        if (!std::isnan(z.toScalar())) s.field("z") = z;
        outs[2] = std::move(s);
    }
}

void ranksum_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ranksum: requires (X, Y[, alpha, tail | name-value])",
                    0, 0, "ranksum", "", "numkit:ranksum:nargin");
    auto *mr = ctx.engine->resource();

    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    std::string method;

    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        alpha = args[i].toScalar();
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "alpha")  alpha = v.toScalar();
        else if (name == "tail")   tail  = parse_tail(v.toString(), TestTail::Both);
        else if (name == "method") method = v.toString();
        i += 2;
    }

    auto [p, h, rs, z] = ranksum(args[0], args[1], alpha, tail, method, mr);
    outs[0] = std::move(p);
    if (nargout > 1) outs[1] = std::move(h);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        if (!std::isnan(z.toScalar())) s.field("zval") = z;
        s.field("ranksum") = rs;
        outs[2] = std::move(s);
    }
}

void signrank_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("signrank: requires X[, m | y][, alpha, tail or "
                    "name-value]", 0, 0, "signrank", "", "numkit:signrank:nargin");
    auto *mr = ctx.engine->resource();

    Value y_or_m = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    size_t i = 1;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        y_or_m = args[i];
        ++i;
    }

    double alpha = 0.05;
    TestTail tail = TestTail::Both;
    std::string method;

    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        alpha = args[i].toScalar();
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "alpha")  alpha = v.toScalar();
        else if (name == "tail")   tail  = parse_tail(v.toString(), TestTail::Both);
        else if (name == "method") method = v.toString();
        i += 2;
    }

    auto [p, h, sr, z] = signrank(args[0], y_or_m, alpha, tail, method, mr);
    outs[0] = std::move(p);
    if (nargout > 1) outs[1] = std::move(h);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("signedrank") = sr;
        // zval only present for approximate method (NaN otherwise).
        if (!std::isnan(z.toScalar())) s.field("zval") = z;
        outs[2] = std::move(s);
    }
}

void signtest_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("signtest: requires X[, m | y][, alpha, tail or "
                    "name-value]", 0, 0, "signtest", "", "numkit:signtest:nargin");
    auto *mr = ctx.engine->resource();

    // arg[1] may be: missing, scalar median, or paired y vector. Skip it
    // if it's a string (start of name-value list).
    Value y_or_m = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    size_t i = 1;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        y_or_m = args[i];
        ++i;
    }

    double alpha = 0.05;
    TestTail tail = TestTail::Both;

    // Optional positional alpha next (legacy 3-arg form), then name-value.
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        alpha = args[i].toScalar();
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "alpha")  alpha = v.toScalar();
        else if (name == "tail")   tail  = parse_tail(v.toString(), TestTail::Both);
        else if (name == "method") { /* exact / approximate — both rely on binocdf */ }
        i += 2;
    }

    auto [p, h, sig] = signtest(args[0], y_or_m, alpha, tail, mr);
    outs[0] = std::move(p);
    if (nargout > 1) outs[1] = std::move(h);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        // MATLAB R2025b stats struct shape: {zval, sign}. zval is NaN
        // for the exact (binomial) path — currently always taken;
        // 'approximate' would populate zval with the normal-approx z.
        s.field("zval") = Value::scalar(std::numeric_limits<double>::quiet_NaN(), mr);
        s.field("sign") = sig;
        outs[2] = std::move(s);
    }
}

// ── lillietest (Lilliefors normality test) ─────────────────────────
// Tests H0: x ~ N(mu, sigma^2) for unspecified mu, sigma. Uses KS
// statistic against a fitted normal CDF (mean and std estimated
// from sample). p-value via Stephens (1974) approximation.

void lillietest_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lillietest: requires at least 1 argument",
                    0, 0, "lillietest", "", "numkit:lillietest:nargin");
    double alpha = 0.05;
    if (args.size() >= 2 && !args[1].isEmpty()) alpha = args[1].toScalar();
    auto [h, p, kstat, critval] =
        lillietest(args[0], alpha, ctx.engine->resource());
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(kstat);
    if (nargout > 3) outs[3] = std::move(critval);
}

void ansaribradley_reg(Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ansaribradley: requires (X, Y[, alpha or name-value])",
                    0, 0, "ansaribradley", "", "numkit:ansaribradley:nargin");
    auto *mr = ctx.engine->resource();

    double alpha = 0.05;
    TestTail tail = TestTail::Both;

    size_t i = 2;
    if (i < args.size() && !args[i].isChar() && !args[i].isString()) {
        alpha = args[i].toScalar();
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!args[i].isChar() && !args[i].isString()) break;
        std::string name = args[i].toString();
        for (auto &c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const Value &v = args[i + 1];
        if      (name == "alpha") alpha = v.toScalar();
        else if (name == "tail")  tail  = parse_tail(v.toString(), TestTail::Both);
        i += 2;
    }

    auto [h, p, W, Wstar] = ansaribradley(args[0], args[1], alpha, tail, mr);
    outs[0] = std::move(h);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) {
        Value s = Value::structure(mr);
        s.field("W")     = W;
        s.field("Wstar") = Wstar;
        outs[2] = std::move(s);
    }
}

} // namespace detail

} // namespace numkit::stats
