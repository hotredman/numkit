// numkit/graphics — bar.cpp
//
// graphics.bar.* builders (bar / barh / scatter / hist / histogram / pie / boxplot / patch / heatmap / bubblechart / swarmchart), carved out of plots.cpp. Core-free bodies (GraphicsContext); shared
// JSON/parse helpers come from plot_internal.hpp.

#include "plot_internal.hpp"
#include <numkit/graphics/graphics_context.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

namespace numkit {

void buildBarPlots(std::vector<PlotEntry> &table)
{
    using namespace detail;
    auto reg = [&](const char *sub, const char *name, GraphicsFn fn) {
        table.push_back(PlotEntry{sub, name, /*core=*/false, std::move(fn)});
    };

    reg("bar", "bar",
        [](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
            fm.prepareForPlot();

            // Identify the X (optional) and Y arguments.
            const Value *xArg = nullptr;
            const Value *yArg = nullptr;
            size_t nvStart = 1;
            if (args.size() >= 2 && !args[1].isChar()) {
                xArg = &args[0];
                yArg = &args[1];
                nvStart = 2;
            } else {
                yArg = &args[0];
            }
            // 'stacked' / 'grouped' / 'histc' specifier (string) after Y.
            std::string mode = "grouped";
            for (size_t i = nvStart; i < args.size(); ++i) {
                if (args[i].isChar()) {
                    std::string s = args[i].toString();
                    for (auto &c : s) c = (char)std::tolower((unsigned char)c);
                    if (s == "stacked" || s == "grouped"
                        || s == "histc"  || s == "hist") {
                        mode = s;
                    }
                }
            }

            const size_t Yr = yArg->dims().rows();
            const size_t Yc = yArg->dims().cols();
            const bool matrix = (Yr > 1 && Yc > 1);
            // Vector path (back-compat): single dataset.
            if (!matrix) {
                DatasetInfo ds;
                ds.type = "bar";
                if (xArg) {
                    ds.xJson = vecToJson(*xArg);
                    ds.yJson = vecToJson(*yArg);
                } else {
                    ds.xJson = makeIndexJson(yArg->numel());
                    ds.yJson = vecToJson(*yArg);
                }
                fm.pushDataset(std::move(ds));
                fm.emitModified();
                outs[0] = Value();
                return;
            }
            // Matrix path: one dataset per column. Stacked uses
            // cumulative sums (same trick as area-stacked); grouped
            // uses raw values + a fan-out X offset per series. Default
            // is 'grouped'.
            static const char *kBarPalette[8] = {
                "#7fd99a", "#5fb3d4", "#e9b870", "#9b8cf2",
                "#e26a6a", "#d4a5e6", "#f2a37e", "#6fcfbf",
            };
            // Build base X vector — explicit when given, else 1..Yr.
            std::vector<double> xb(Yr);
            if (xArg) {
                for (size_t r = 0; r < Yr; ++r) xb[r] = xArg->doubleData()[r];
            } else {
                for (size_t r = 0; r < Yr; ++r) xb[r] = (double)(r + 1);
            }
            const double *Yp = yArg->doubleData();
            if (mode == "stacked") {
                // Cumulative sums; emit column Yc-1 first so lower
                // bands overdraw the bottom of higher ones (same idea
                // as area-stacked).
                std::vector<std::vector<double>> S(Yr, std::vector<double>(Yc, 0.0));
                for (size_t r = 0; r < Yr; ++r) {
                    double acc = 0;
                    for (size_t c = 0; c < Yc; ++c) {
                        acc += Yp[c * Yr + r];
                        S[r][c] = acc;
                    }
                }
                std::ostringstream xs;
                xs << '[';
                for (size_t r = 0; r < Yr; ++r) {
                    if (r) xs << ',';
                    xs << xb[r];
                }
                xs << ']';
                const std::string xJsonStr = xs.str();
                for (long long cc = (long long)Yc - 1; cc >= 0; --cc) {
                    DatasetInfo ds;
                    ds.type = "bar";
                    ds.xJson = xJsonStr;
                    std::ostringstream ys;
                    ys << '[';
                    for (size_t r = 0; r < Yr; ++r) {
                        if (r) ys << ',';
                        ys << S[r][(size_t)cc];
                    }
                    ys << ']';
                    ds.yJson = ys.str();
                    std::ostringstream sty;
                    sty << "color=" << kBarPalette[(size_t)cc % 8];
                    ds.style = sty.str();
                    fm.pushDataset(std::move(ds));
                }
            } else {
                // Grouped: each column gets a small X offset so bars
                // don't overlap. The IDE renderer renders each dataset
                // with the same default width, so sub-pixel
                // positioning is the visible separator. With Yc bars
                // per group, offsets span ±0.4 around the base.
                const double groupHalf = 0.4;
                const double slot = (Yc > 1) ? (2 * groupHalf) / (Yc - 1) : 0.0;
                for (size_t cc = 0; cc < Yc; ++cc) {
                    DatasetInfo ds;
                    ds.type = "bar";
                    std::ostringstream xs, ys;
                    xs << '['; ys << '[';
                    const double off = -groupHalf + slot * cc;
                    for (size_t r = 0; r < Yr; ++r) {
                        if (r) { xs << ','; ys << ','; }
                        xs << (xb[r] + off);
                        ys << Yp[cc * Yr + r];
                    }
                    xs << ']'; ys << ']';
                    ds.xJson = xs.str();
                    ds.yJson = ys.str();
                    std::ostringstream sty;
                    sty << "color=" << kBarPalette[cc % 8];
                    ds.style = sty.str();
                    fm.pushDataset(std::move(ds));
                }
            }
            fm.emitModified();
            outs[0] = Value();
        });
    // ────────── Statistical chart wrappers ───────────────────────────
    // These all reduce to 1-2 existing dataset types (scatter / line /
    // bar / stairs) so the renderer doesn't need any new code.
    // cdfplot(x) / ecdf(x) — empirical cumulative distribution. Sorts
    // x ascending and renders a right-continuous step function from
    // 0 to 1. NaN inputs are dropped.
    auto cdfImpl = [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
        const auto &X = args[0];
        std::vector<double> xs;
        xs.reserve(X.numel());
        for (size_t i = 0; i < X.numel(); ++i) {
            const double v = X.doubleData()[i];
            if (std::isfinite(v)) xs.push_back(v);
        }
        if (xs.empty()) { outs[0] = Value(); return; }
        std::sort(xs.begin(), xs.end());
        const size_t N = xs.size();

        std::ostringstream sx, sy;
        sx << '['; sy << '[';
        for (size_t i = 0; i < N; ++i) {
            if (i) { sx << ','; sy << ','; }
            sx << xs[i];
            sy << ((double)(i + 1) / (double)N);
        }
        sx << ']'; sy << ']';

        auto &fm = gc.fm;
        fm.prepareForPlot();
        DatasetInfo ds;
        ds.type = "stairs";
        ds.xJson = sx.str();
        ds.yJson = sy.str();
        ds.style = "color=#1f77b4";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value();
    };
    reg("bar", "cdfplot", cdfImpl);
    // `ecdf` is already provided by toolboxes/stats as a computational
    // routine that returns (F, x) — registering a graphics version
    // here would duplicate the compat.<ecdf> alias and brick WASM
    // init. Users plot the empirical CDF via `cdfplot(x)`.
    // qqplot(x) — quantile-quantile plot vs the standard normal. The
    // observed sample quantile at rank i = (i - 0.5) / N is plotted
    // against the theoretical normal quantile Φ⁻¹((i - 0.5) / N).
    // Reference line passes through the 25th and 75th sample quantiles
    // — a robust IQR-based fit that beats a naive μ ± σ line on
    // skewed data. Two datasets: scatter (points) + line (reference).
    reg("bar", "qqplot",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
            std::vector<double> xs;
            for (size_t i = 0; i < args[0].numel(); ++i) {
                const double v = args[0].doubleData()[i];
                if (std::isfinite(v)) xs.push_back(v);
            }
            if (xs.size() < 2) { outs[0] = Value(); return; }
            std::sort(xs.begin(), xs.end());
            const size_t N = xs.size();

            // Probit (inverse normal CDF) — Abramowitz & Stegun 26.2.23
            // rational approx, good to ~4.5e-4 absolute error. Mirrors
            // the upper tail; the lower tail is its negative.
            const auto probit = [](double p) {
                p = std::max(1e-15, std::min(1.0 - 1e-15, p));
                const bool lower = (p < 0.5);
                const double q = lower ? p : (1.0 - p);
                const double t = std::sqrt(-2.0 * std::log(q));
                const double c0 = 2.515517, c1 = 0.802853, c2 = 0.010328;
                const double d1 = 1.432788, d2 = 0.189269, d3 = 0.001308;
                const double r = t - (c0 + c1*t + c2*t*t)
                                   / (1 + d1*t + d2*t*t + d3*t*t*t);
                return lower ? -r : r;
            };

            std::ostringstream tx, ty;
            tx << '['; ty << '[';
            for (size_t i = 0; i < N; ++i) {
                if (i) { tx << ','; ty << ','; }
                tx << probit((i + 0.5) / (double)N);
                ty << xs[i];
            }
            tx << ']'; ty << ']';

            // Reference line: passes through (probit(0.25), q1) and
            // (probit(0.75), q3) where q1, q3 are the 25th / 75th
            // percentiles of the sample.
            const double q1 = xs[(size_t)((N - 1) * 0.25)];
            const double q3 = xs[(size_t)((N - 1) * 0.75)];
            const double t1 = probit(0.25), t3 = probit(0.75);
            const double slope = (q3 - q1) / (t3 - t1);
            const double intercept = q1 - slope * t1;
            // Extend slightly past the data range so the ref line
            // visibly bisects the cloud.
            const double tlo = probit(0.5 / N) - 0.1;
            const double thi = probit(1.0 - 0.5 / N) + 0.1;
            std::ostringstream lx, ly;
            lx << '[' << tlo << ',' << thi << ']';
            ly << '[' << (intercept + slope * tlo) << ',' << (intercept + slope * thi) << ']';

            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo dsLine;
            dsLine.type = "line";
            dsLine.xJson = lx.str();
            dsLine.yJson = ly.str();
            dsLine.style = "color=#d62728";
            fm.pushDataset(std::move(dsLine));
            DatasetInfo dsDot;
            dsDot.type = "scatter";
            dsDot.xJson = tx.str();
            dsDot.yJson = ty.str();
            dsDot.style = "color=#1f77b4";
            dsDot.markerSize = 3;
            fm.pushDataset(std::move(dsDot));
            fm.emitModified();
            outs[0] = Value();
        });
    // pareto(Y) — bars in descending Y order + a cumulative-percent
    // line on the same axes. Useful for "80/20" / quality-control
    // visualisations. We emit two datasets:
    //   1. type=bar, X = 1..N (rank), Y sorted descending
    //   2. type=line, same X, Y = 100 * cumsum(Y) / sum(Y)
    reg("bar", "pareto",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
            std::vector<double> ys;
            for (size_t i = 0; i < args[0].numel(); ++i) {
                const double v = args[0].doubleData()[i];
                if (std::isfinite(v)) ys.push_back(v);
            }
            if (ys.empty()) { outs[0] = Value(); return; }
            std::sort(ys.begin(), ys.end(), std::greater<double>());
            double sum = 0;
            for (double v : ys) sum += v;
            if (sum == 0) sum = 1;

            std::ostringstream bx, by, lx, ly;
            bx << '['; by << '['; lx << '['; ly << '[';
            double cum = 0;
            for (size_t i = 0; i < ys.size(); ++i) {
                if (i) { bx << ','; by << ','; lx << ','; ly << ','; }
                cum += ys[i];
                bx << (i + 1); by << ys[i];
                lx << (i + 1); ly << (100.0 * cum / sum);
            }
            bx << ']'; by << ']'; lx << ']'; ly << ']';

            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo dsBar;
            dsBar.type = "bar";
            dsBar.xJson = bx.str();
            dsBar.yJson = by.str();
            dsBar.style = "color=#1f77b4";
            fm.pushDataset(std::move(dsBar));
            DatasetInfo dsLine;
            dsLine.type = "line";
            dsLine.xJson = lx.str();
            dsLine.yJson = ly.str();
            dsLine.style = "color=#d62728";
            fm.pushDataset(std::move(dsLine));
            fm.emitModified();
            outs[0] = Value();
        });
    // histfit(x[, nbins]) — histogram of x with a Gaussian fit overlay.
    // Default 10 bins. The fit uses sample mean/std; PDF is scaled by
    // (N * binWidth) so the curve is visually comparable to the bar
    // counts.
    reg("bar", "histfit",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) { outs[0] = Value(); return; }
            std::vector<double> xs;
            for (size_t i = 0; i < args[0].numel(); ++i) {
                const double v = args[0].doubleData()[i];
                if (std::isfinite(v)) xs.push_back(v);
            }
            if (xs.size() < 2) { outs[0] = Value(); return; }
            int nbins = 10;
            if (args.size() >= 2 && args[1].numel() == 1)
                nbins = std::max(1, (int)args[1].toScalar());

            double mn = xs[0], mx = xs[0];
            for (double v : xs) {
                if (v < mn) mn = v;
                if (v > mx) mx = v;
            }
            const double bw = (mx - mn) / nbins;
            const double bwSafe = (bw == 0) ? 1.0 : bw;
            std::vector<double> counts(nbins, 0.0), centers(nbins);
            for (int i = 0; i < nbins; ++i)
                centers[i] = mn + bwSafe * (i + 0.5);
            for (double v : xs) {
                int b = (int)((v - mn) / bwSafe);
                if (b >= nbins) b = nbins - 1;
                if (b < 0) b = 0;
                counts[b] += 1;
            }
            double mean = 0;
            for (double v : xs) mean += v;
            mean /= xs.size();
            double var = 0;
            for (double v : xs) var += (v - mean) * (v - mean);
            var /= (xs.size() > 1 ? xs.size() - 1 : 1);
            const double sd = std::sqrt(var);
            const double sdSafe = (sd > 0) ? sd : 1.0;

            // Sample the Gaussian on a fine grid for the fit curve.
            const int gridN = 100;
            std::ostringstream bx, by, fx, fy;
            bx << '['; by << '[';
            for (int i = 0; i < nbins; ++i) {
                if (i) { bx << ','; by << ','; }
                bx << centers[i]; by << counts[i];
            }
            bx << ']'; by << ']';
            fx << '['; fy << '[';
            const double scale = xs.size() * bwSafe;
            for (int g = 0; g < gridN; ++g) {
                if (g) { fx << ','; fy << ','; }
                const double xg = mn + (mx - mn) * g / (gridN - 1);
                const double pdf = std::exp(-0.5 * std::pow((xg - mean) / sdSafe, 2))
                                 / (sdSafe * std::sqrt(2 * 3.14159265358979323846));
                fx << xg; fy << (pdf * scale);
            }
            fx << ']'; fy << ']';

            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo dsBar;
            dsBar.type = "bar";
            dsBar.xJson = bx.str();
            dsBar.yJson = by.str();
            dsBar.style = "color=#aec7e8";
            fm.pushDataset(std::move(dsBar));
            DatasetInfo dsFit;
            dsFit.type = "line";
            dsFit.xJson = fx.str();
            dsFit.yJson = fy.str();
            dsFit.style = "color=#d62728";
            dsFit.lineWidth = 2;
            fm.pushDataset(std::move(dsFit));
            fm.emitModified();
            outs[0] = Value();
        });
    // gscatter(x, y, g) — scatter coloured by group label. Each unique
    // value of g becomes its own scatter dataset (so it picks up a
    // distinct color from the renderer's palette).
    reg("bar", "gscatter",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.size() < 3) { outs[0] = Value(); return; }
            const auto &X = args[0];
            const auto &Y = args[1];
            const auto &G = args[2];
            const size_t N = std::min({X.numel(), Y.numel(), G.numel()});
            if (N == 0) { outs[0] = Value(); return; }

            // Group indices: distinct values in G, preserving first-seen
            // order. G can be numeric or char (treat both via toScalar
            // when numeric; char-array support deferred — most users
            // pass numeric labels).
            std::vector<double> groupKeys;
            std::vector<int> groupIdx(N);
            for (size_t i = 0; i < N; ++i) {
                const double k = G.doubleData()[i];
                int found = -1;
                for (size_t j = 0; j < groupKeys.size(); ++j) {
                    if (groupKeys[j] == k) { found = (int)j; break; }
                }
                if (found < 0) {
                    found = (int)groupKeys.size();
                    groupKeys.push_back(k);
                }
                groupIdx[i] = found;
            }

            // Categorical palette (ColorBrewer Set1, 8 colors).
            static const char *kPalette[] = {
                "#1f77b4", "#d62728", "#2ca02c", "#9467bd",
                "#ff7f0e", "#17becf", "#e377c2", "#7f7f7f",
            };
            const size_t nGroups = groupKeys.size();

            auto &fm = gc.fm;
            fm.prepareForPlot();
            for (size_t gi = 0; gi < nGroups; ++gi) {
                std::ostringstream sx, sy;
                sx << '['; sy << '[';
                bool first = true;
                for (size_t i = 0; i < N; ++i) {
                    if (groupIdx[i] != (int)gi) continue;
                    if (!first) { sx << ','; sy << ','; }
                    first = false;
                    sx << X.doubleData()[i];
                    sy << Y.doubleData()[i];
                }
                sx << ']'; sy << ']';
                if (first) continue;
                DatasetInfo ds;
                ds.type = "scatter";
                ds.xJson = sx.str();
                ds.yJson = sy.str();
                std::ostringstream st;
                st << "color=" << kPalette[gi % 8];
                ds.style = st.str();
                ds.label = "group " + std::to_string((long long)groupKeys[gi]);
                fm.pushDataset(std::move(ds));
            }
            fm.emitModified();
            outs[0] = Value();
        });
    // spy(M) — sparsity pattern. Renders a marker at every (col, row)
    // where M is non-zero (and finite). Mirrors MATLAB convention of
    // axis ij so the matrix sits like the printed form (row 1 at top).
    reg("bar", "spy",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            const auto &M = args[0];
            const size_t R = M.dims().rows();
            const size_t C = M.dims().cols();
            std::ostringstream xs, ys;
            xs << '['; ys << '[';
            bool first = true;
            for (size_t c = 0; c < C; ++c) {
                for (size_t r = 0; r < R; ++r) {
                    const double v = M.doubleData()[c * R + r];
                    if (v == 0.0 || !std::isfinite(v)) continue;
                    if (!first) { xs << ','; ys << ','; }
                    first = false;
                    xs << (c + 1);
                    ys << (r + 1);
                }
            }
            xs << ']'; ys << ']';
            DatasetInfo ds;
            ds.type = "scatter";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.style = "color=#1f77b4";
            ds.markerSize = 3;
            fm.pushDataset(std::move(ds));
            // axis ij so row 1 is at the top, matching MATLAB's spy.
            fm.currentAxes().yDir = "reverse";
            fm.currentAxes().axisMode = "ij";
            fm.emitModified();
            outs[0] = Value();
        });
    // barh — horizontal bar chart, mirror of bar(). MATLAB convention:
    //   barh(y)    — y is vector of bar lengths, vertical positions = 1:N
    //   barh(x, y) — x is vertical positions (categories), y = lengths
    // We store xJson = positions (rendered along the Y axis) and
    // yJson = lengths (along X axis); the 'barh' mode in the renderer
    // swaps the axis roles compared to 'bar'.
    reg("bar", "barh",
        [](Span<const Value> args, size_t nargout,
                                   Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "barh";
            if (args.size() >= 2 && !args[1].isChar()) {
                ds.xJson = vecToJson(args[0]);
                ds.yJson = vecToJson(args[1]);
            } else {
                ds.xJson = makeIndexJson(args[0].numel());
                ds.yJson = vecToJson(args[0]);
            }
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    reg("bar", "scatter",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            if (args.size() < 2) {
                outs[0] = Value();
                return;
            }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "scatter";
            ds.xJson = vecToJson(args[0]);
            ds.yJson = vecToJson(args[1]);
            // scatter(x, y, ..., 'filled') → filled markers; MATLAB's default is
            // open (the renderer draws an outline unless the style says filled).
            for (size_t k = 2; k < args.size(); ++k) {
                if (args[k].isChar()) {
                    std::string s = args[k].toString();
                    for (auto &c : s) c = static_cast<char>(std::tolower(c));
                    if (s == "filled") { ds.style = "filled=1"; break; }
                }
            }
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    reg("bar", "hist",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto *mr = gc.mr;
            if (args.empty()) {
                outs[0] = Value();
                return;
            }
            auto &data = args[0];
            int bins = (args.size() >= 2) ? static_cast<int>(args[1].toScalar()) : 10;
            double mn = data.doubleData()[0], mx = data.doubleData()[0];
            for (size_t i = 1; i < data.numel(); ++i) {
                mn = std::min(mn, data.doubleData()[i]);
                mx = std::max(mx, data.doubleData()[i]);
            }
            double bw = (mx - mn) / bins;
            if (bw == 0)
                bw = 1;
            auto centers = Value::matrix(1, bins, ValueType::DOUBLE, mr);
            auto counts = Value::matrix(1, bins, ValueType::DOUBLE, mr);
            for (int b = 0; b < bins; ++b)
                centers.doubleDataMut()[b] = mn + bw * (b + 0.5);
            for (size_t i = 0; i < data.numel(); ++i) {
                int b = static_cast<int>((data.doubleData()[i] - mn) / bw);
                if (b >= bins)
                    b = bins - 1;
                if (b < 0)
                    b = 0;
                counts.doubleDataMut()[b] += 1;
            }
            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "bar";
            ds.xJson = vecToJson(centers);
            ds.yJson = vecToJson(counts);
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    // histogram2(X, Y) — 2-D histogram. Bins (X, Y) into an nx×ny grid
    // and renders the count matrix as an imagesc-style heatmap. Param
    // forms supported (positional, like MATLAB's classic call):
    //   histogram2(X, Y)              — 10×10 bins over data extent
    //   histogram2(X, Y, n)           — n×n
    //   histogram2(X, Y, [nx ny])     — explicit grid
    //   histogram2(X, Y, nx, ny)      — explicit grid (separate args)
    // Name-Value form (NumBins, BinEdges, …) is on the BACKLOG.
    reg("bar", "histogram2",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            auto *mr = gc.mr;
            if (args.size() < 2 || args[0].numel() == 0 || args[1].numel() == 0) {
                outs[0] = Value();
                return;
            }
            const auto &X = args[0];
            const auto &Y = args[1];
            const size_t N = std::min(X.numel(), Y.numel());

            int nx = 10, ny = 10;
            if (args.size() >= 3) {
                if (args[2].numel() >= 2) {
                    nx = (int)args[2].doubleData()[0];
                    ny = (int)args[2].doubleData()[1];
                } else if (args[2].numel() == 1) {
                    nx = ny = (int)args[2].toScalar();
                    if (args.size() >= 4 && args[3].numel() == 1)
                        ny = (int)args[3].toScalar();
                }
            }
            if (nx < 1) nx = 1;
            if (ny < 1) ny = 1;

            double xmn = X.doubleData()[0], xmx = xmn;
            double ymn = Y.doubleData()[0], ymx = ymn;
            for (size_t k = 1; k < N; ++k) {
                const double xv = X.doubleData()[k];
                const double yv = Y.doubleData()[k];
                if (std::isfinite(xv)) {
                    if (xv < xmn) xmn = xv;
                    if (xv > xmx) xmx = xv;
                }
                if (std::isfinite(yv)) {
                    if (yv < ymn) ymn = yv;
                    if (yv > ymx) ymx = yv;
                }
            }
            double bwx = (xmx - xmn) / nx;
            double bwy = (ymx - ymn) / ny;
            if (bwx == 0) bwx = 1;
            if (bwy == 0) bwy = 1;

            auto centers_x = Value::matrix(1, (size_t)nx, ValueType::DOUBLE, mr);
            auto centers_y = Value::matrix(1, (size_t)ny, ValueType::DOUBLE, mr);
            // counts is row-major-as-Y, col-major-as-X (i.e. ny rows, nx
            // cols). MATLAB stores column-major, so element (j, i) is at
            // linear index i * ny + j.
            auto counts = Value::matrix((size_t)ny, (size_t)nx, ValueType::DOUBLE, mr);
            for (int i = 0; i < nx; ++i)
                centers_x.doubleDataMut()[i] = xmn + bwx * (i + 0.5);
            for (int j = 0; j < ny; ++j)
                centers_y.doubleDataMut()[j] = ymn + bwy * (j + 0.5);
            for (size_t k = 0; k < N; ++k) {
                const double xv = X.doubleData()[k];
                const double yv = Y.doubleData()[k];
                if (!std::isfinite(xv) || !std::isfinite(yv))
                    continue;
                int i = (int)((xv - xmn) / bwx);
                int j = (int)((yv - ymn) / bwy);
                if (i >= nx) i = nx - 1;
                if (j >= ny) j = ny - 1;
                if (i < 0) i = 0;
                if (j < 0) j = 0;
                counts.doubleDataMut()[(size_t)i * (size_t)ny + (size_t)j] += 1.0;
            }
            // Delegate to the heatmap pipeline. typeName='imagesc' so the
            // adapter routes through the existing heatmap renderer with
            // its full quantization + LUT path.
            std::vector<Value> proxied = { centers_x, centers_y, counts };
            // Delegate to heatmapImpl. axisIj=false — histogram2 stays on
            // axis xy by default (Y up), unlike imagesc which auto-flips.
            heatmapImpl("imagesc", false,
                        Span<const Value>(proxied.data(), proxied.size()),
                        nargout, outs, gc);
        });
    // ── patch / fill — generic filled polygon ────────────────────────
    // Calling forms:
    //   patch(X, Y)             — single polygon, default fill
    //   patch(X, Y, C)          — C is a colour spec ('r', '#ff8800',
    //                              [r g b] triplet 0..1)
    //   patch(X, Y, C, 'EdgeColor', '...')   — N-V pairs (subset)
    // X and Y may be column-vectors (one polygon) or matrices (one
    // polygon per column). For matrix inputs we serialise polygons
    // separated by null markers in the wire JSON; the renderer breaks
    // sub-paths on null and closes each.
    auto patchImpl = [](Span<const Value> args, size_t nargout,
                        Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.size() < 2) { outs[0] = Value(); return; }
        const auto &X = args[0];
        const auto &Y = args[1];
        const size_t R = X.dims().rows();
        const size_t C = std::max<size_t>(1, X.dims().cols());
        if (R == 0) { outs[0] = Value(); return; }
        if (Y.dims().rows() != R || std::max<size_t>(1, Y.dims().cols()) != C) {
            outs[0] = Value();
            return;
        }

        std::ostringstream xs, ys;
        xs << '['; ys << '[';
        for (size_t c = 0; c < C; ++c) {
            if (c > 0) { xs << ",null,"; ys << ",null,"; }
            for (size_t r = 0; r < R; ++r) {
                if (r > 0) { xs << ','; ys << ','; }
                xs << X.doubleData()[c * R + r];
                ys << Y.doubleData()[c * R + r];
            }
        }
        xs << ']'; ys << ']';

        // Color: 3rd arg accepts a single-char colour, "#rrggbb",
        // or a 3-element [r g b] vector with values in [0, 1].
        std::string color = "#1f77b4";
        if (args.size() >= 3) {
            const auto &Carg = args[2];
            if (Carg.isChar()) {
                std::string s = Carg.toString();
                if (!s.empty()) {
                    static const std::map<char, const char*> kShort = {
                        {'r', "#d62728"}, {'g', "#2ca02c"}, {'b', "#1f77b4"},
                        {'y', "#ffd700"}, {'m', "#bf40bf"}, {'c', "#17becf"},
                        {'k', "#000000"}, {'w', "#ffffff"},
                    };
                    if (s.size() == 1) {
                        auto it = kShort.find(s[0]);
                        if (it != kShort.end()) color = it->second;
                    } else if (s.size() >= 4 && s[0] == '#') {
                        color = s;
                    }
                }
            } else if (Carg.numel() >= 3) {
                const int r = (int)std::round(255 * std::clamp(Carg.doubleData()[0], 0.0, 1.0));
                const int g = (int)std::round(255 * std::clamp(Carg.doubleData()[1], 0.0, 1.0));
                const int b = (int)std::round(255 * std::clamp(Carg.doubleData()[2], 0.0, 1.0));
                char buf[16];
                std::snprintf(buf, sizeof buf, "#%02x%02x%02x", r, g, b);
                color = buf;
            }
        }

        auto &fm = gc.fm;
        fm.prepareForPlot();
        DatasetInfo ds;
        ds.type = "polygon";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        std::ostringstream sty;
        sty << "color=" << color << ";fillOpacity=0.7";
        ds.style = sty.str();
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        outs[0] = Value();
    };
    reg("bar", "patch", patchImpl);
    reg("bar", "fill",  patchImpl);
    // pie(X) — circular slices proportional to X. Each wedge is its
    // own polygon dataset so it picks up a distinct palette colour.
    // Calling forms (subset of MATLAB's):
    //   pie(X)             — slice for every X(i) > 0
    //   pie(X, explode)    — explode(i) ≠ 0 displaces that wedge
    //                        radially by ~10% of the unit radius
    auto pieImpl = [](Span<const Value> args, size_t nargout, Span<Value> outs,
                      GraphicsContext &gc, bool tilt3d) {
        (void)nargout;
        if (args.empty() || args[0].numel() == 0) {
            outs[0] = Value();
            return;
        }
        const auto &X = args[0];
        const size_t N = X.numel();
        double total = 0;
        for (size_t i = 0; i < N; ++i) {
            const double v = X.doubleData()[i];
            if (std::isfinite(v) && v > 0) total += v;
        }
        if (total <= 0) { outs[0] = Value(); return; }

        const Value *expl = (args.size() >= 2) ? &args[1] : nullptr;
        static const char *kPalette[] = {
            "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728",
            "#9467bd", "#8c564b", "#e377c2", "#7f7f7f",
            "#bcbd22", "#17becf",
        };

        const double TAU = 2 * 3.14159265358979323846;
        const int arcSamples = 24;          // vertices per wedge arc
        const double tiltY = tilt3d ? 0.4 : 1.0;   // pie3 squashes Y

        auto &fm = gc.fm;
        fm.prepareForPlot();
        // Force equal axis so the pie stays circular.
        fm.currentAxes().axisMode = "equal";

        double angle = 0;
        for (size_t i = 0; i < N; ++i) {
            const double v = X.doubleData()[i];
            if (!std::isfinite(v) || v <= 0) continue;
            const double dθ = v / total * TAU;
            const double θ0 = angle;
            const double θ1 = angle + dθ;
            angle = θ1;

            // Explode: shift centre along the wedge bisector.
            double cx = 0, cy = 0;
            if (expl && expl->numel() > i && expl->doubleData()[i] != 0) {
                const double mid = (θ0 + θ1) / 2;
                const double off = 0.1;
                cx = off * std::cos(mid);
                cy = off * std::sin(mid) * tiltY;
            }

            std::ostringstream xs, ys;
            xs << '['; ys << '[';
            xs << cx; ys << cy;
            for (int s = 0; s <= arcSamples; ++s) {
                const double t = θ0 + (θ1 - θ0) * s / arcSamples;
                xs << ',' << (cx + std::cos(t));
                ys << ',' << (cy + std::sin(t) * tiltY);
            }
            xs << ']'; ys << ']';

            DatasetInfo ds;
            ds.type = "polygon";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            std::ostringstream sty;
            sty << "color=" << kPalette[i % 10] << ";fillOpacity=0.85";
            ds.style = sty.str();
            // Auto-label "p%" so the legend (if user calls legend())
            // shows percentages. MATLAB uses cell labels — we ship the
            // numeric form by default.
            char buf[16];
            std::snprintf(buf, sizeof buf, "%.0f%%", v / total * 100.0);
            ds.label = buf;
            fm.pushDataset(std::move(ds));
        }
        fm.emitModified();
        outs[0] = Value();
    };
    reg("bar", "pie",  [pieImpl](Span<const Value> a, size_t n, Span<Value> o, GraphicsContext &gc) {
        pieImpl(a, n, o, gc, false);
    });
    reg("bar", "pie3", [pieImpl](Span<const Value> a, size_t n, Span<Value> o, GraphicsContext &gc) {
        pieImpl(a, n, o, gc, true);
    });
    // boxplot(X) / boxchart(X) — Tukey box-and-whisker plot.
    // X as vector → one box at x=1.
    // X as matrix → one box per column at x = 1..C.
    // Per box we emit:
    //   • polygon for the IQR rectangle (Q1..Q3)
    //   • line for the median (horizontal across the box)
    //   • line dataset for the two whisker stems + the two caps
    //   • scatter dataset for outliers (beyond ±1.5·IQR)
    auto boxImpl = [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
        (void)nargout;
        if (args.empty() || args[0].numel() == 0) {
            outs[0] = Value();
            return;
        }
        const auto &M = args[0];
        const size_t R = std::max<size_t>(1, M.dims().rows());
        const size_t C = std::max<size_t>(1, M.dims().cols());
        // Treat 1×N or N×1 as a single column; matrix → C columns.
        const bool single = (R == 1 || C == 1);
        const size_t nBoxes = single ? 1 : C;
        const size_t nPerBox = single ? M.numel() : R;

        auto &fm = gc.fm;
        fm.prepareForPlot();

        const double w = 0.4;     // half-width of each box (so total is 0.8)
        for (size_t bi = 0; bi < nBoxes; ++bi) {
            std::vector<double> col;
            col.reserve(nPerBox);
            for (size_t r = 0; r < nPerBox; ++r) {
                const double v = single
                    ? M.doubleData()[r]
                    : M.doubleData()[bi * R + r];
                if (std::isfinite(v)) col.push_back(v);
            }
            if (col.size() < 2) continue;
            std::sort(col.begin(), col.end());
            const double q1 = col[(size_t)((col.size() - 1) * 0.25)];
            const double q2 = col[(size_t)((col.size() - 1) * 0.50)];
            const double q3 = col[(size_t)((col.size() - 1) * 0.75)];
            const double iqr = q3 - q1;
            const double loFence = q1 - 1.5 * iqr;
            const double hiFence = q3 + 1.5 * iqr;
            double whLo = col.front(), whHi = col.back();
            // Whiskers extend to the most extreme finite point WITHIN
            // the fence, not the fence itself.
            for (double v : col) {
                if (v >= loFence && v < whLo) whLo = v;
                if (v <= hiFence && v > whHi) whHi = v;
            }
            // The simple computation above seeds whLo/whHi with the
            // overall extremes; tighten if those are outside the fence.
            if (whLo < loFence) whLo = loFence;
            if (whHi > hiFence) whHi = hiFence;
            // Re-clamp to the closest in-fence point (more reliable
            // when there are no in-fence extremes).
            for (double v : col) {
                if (v >= loFence && v <= q1 && v < whLo) whLo = v;
                if (v <= hiFence && v >= q3 && v > whHi) whHi = v;
            }

            const double cx = (double)(bi + 1);
            const double xL = cx - w, xR = cx + w;

            // 1. IQR box (polygon).
            std::ostringstream bx, by;
            bx << '[' << xL << ',' << xR << ',' << xR << ',' << xL << ']';
            by << '[' << q1 << ',' << q1 << ',' << q3 << ',' << q3 << ']';
            DatasetInfo dsBox;
            dsBox.type = "polygon";
            dsBox.xJson = bx.str();
            dsBox.yJson = by.str();
            dsBox.style = "color=#1f77b4;fillOpacity=0.35";
            fm.pushDataset(std::move(dsBox));

            // 2. Median line (horizontal across the box).
            std::ostringstream mx, my;
            mx << '[' << xL << ',' << xR << ']';
            my << '[' << q2 << ',' << q2 << ']';
            DatasetInfo dsMed;
            dsMed.type = "line";
            dsMed.xJson = mx.str();
            dsMed.yJson = my.str();
            dsMed.style = "color=#d62728";
            dsMed.lineWidth = 2;
            fm.pushDataset(std::move(dsMed));

            // 3. Whisker stems + caps as one line dataset with null
            //    separators between segments.
            std::ostringstream wx, wy;
            wx << '['; wy << '[';
            // Lower stem: (cx, whLo) → (cx, q1)
            wx << cx << ',' << cx;
            wy << whLo << ',' << q1;
            // Lower cap
            wx << ",null," << (cx - w/2) << ',' << (cx + w/2);
            wy << ",null," << whLo << ',' << whLo;
            // Upper stem
            wx << ",null," << cx << ',' << cx;
            wy << ",null," << q3 << ',' << whHi;
            // Upper cap
            wx << ",null," << (cx - w/2) << ',' << (cx + w/2);
            wy << ",null," << whHi << ',' << whHi;
            wx << ']'; wy << ']';
            DatasetInfo dsW;
            dsW.type = "line";
            dsW.xJson = wx.str();
            dsW.yJson = wy.str();
            dsW.style = "color=#1f77b4";
            fm.pushDataset(std::move(dsW));

            // 4. Outliers — scatter at (cx, v) for v outside fences.
            std::ostringstream ox, oy;
            ox << '['; oy << '[';
            bool first = true;
            for (double v : col) {
                if (v >= loFence && v <= hiFence) continue;
                if (!first) { ox << ','; oy << ','; }
                first = false;
                ox << cx;
                oy << v;
            }
            ox << ']'; oy << ']';
            if (!first) {
                DatasetInfo dsO;
                dsO.type = "scatter";
                dsO.xJson = ox.str();
                dsO.yJson = oy.str();
                dsO.style = "color=#d62728";
                dsO.markerSize = 3;
                fm.pushDataset(std::move(dsO));
            }
        }
        fm.emitModified();
        outs[0] = Value();
    };
    reg("bar", "boxplot",  boxImpl);
    reg("bar", "boxchart", boxImpl);
    // violinplot(X) — Gaussian-KDE shape + slim box + median dot.
    // Layout mirrors boxplot: vector → one violin at x=1, matrix
    // → one violin per column at x = 1..C. KDE bandwidth uses
    // Silverman's rule (1.06 · σ · N^(-1/5)).
    reg("bar", "violinplot",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty() || args[0].numel() == 0) {
                outs[0] = Value();
                return;
            }
            const auto &M = args[0];
            const size_t R = std::max<size_t>(1, M.dims().rows());
            const size_t Cmat = std::max<size_t>(1, M.dims().cols());
            const bool single = (R == 1 || Cmat == 1);
            const size_t nViolins = single ? 1 : Cmat;
            const size_t nPerColumn = single ? M.numel() : R;

            auto &fm = gc.fm;
            fm.prepareForPlot();

            const int kdePts = 50;
            for (size_t bi = 0; bi < nViolins; ++bi) {
                std::vector<double> col;
                col.reserve(nPerColumn);
                for (size_t r = 0; r < nPerColumn; ++r) {
                    const double v = single
                        ? M.doubleData()[r]
                        : M.doubleData()[bi * R + r];
                    if (std::isfinite(v)) col.push_back(v);
                }
                if (col.size() < 2) continue;
                double mean = 0;
                for (double v : col) mean += v;
                mean /= col.size();
                double var = 0;
                for (double v : col) var += (v - mean) * (v - mean);
                var /= col.size() - 1;
                const double sd = std::sqrt(var);
                if (sd <= 0) continue;
                const double bw = 1.06 * sd * std::pow((double)col.size(), -0.2);
                if (bw <= 0) continue;

                std::sort(col.begin(), col.end());
                const double mn = col.front(), mx = col.back();
                const double q1 = col[(size_t)((col.size() - 1) * 0.25)];
                const double q2 = col[(size_t)((col.size() - 1) * 0.50)];
                const double q3 = col[(size_t)((col.size() - 1) * 0.75)];

                // Sample KDE density on a uniform y grid [mn, mx].
                std::vector<double> ys(kdePts), den(kdePts);
                double maxD = 0;
                for (int g = 0; g < kdePts; ++g) {
                    const double y = mn + (mx - mn) * g / (double)(kdePts - 1);
                    double s = 0;
                    for (double v : col) {
                        const double t = (y - v) / bw;
                        s += std::exp(-0.5 * t * t);
                    }
                    s /= (col.size() * bw * std::sqrt(2 * 3.14159265358979323846));
                    ys[g] = y;
                    den[g] = s;
                    if (s > maxD) maxD = s;
                }
                if (maxD <= 0) continue;
                const double cx = (double)(bi + 1);
                const double halfW = 0.4;
                const double scale = halfW / maxD;

                // Violin polygon: right side bottom-to-top + left side
                // top-to-bottom. Closed by the polygon renderer.
                std::ostringstream xs, vy;
                xs << '['; vy << '[';
                bool first = true;
                for (int g = 0; g < kdePts; ++g) {
                    if (!first) { xs << ','; vy << ','; }
                    first = false;
                    xs << (cx + den[g] * scale);
                    vy << ys[g];
                }
                for (int g = kdePts - 1; g >= 0; --g) {
                    xs << ',' << (cx - den[g] * scale);
                    vy << ',' << ys[g];
                }
                xs << ']'; vy << ']';
                DatasetInfo dsV;
                dsV.type = "polygon";
                dsV.xJson = xs.str();
                dsV.yJson = vy.str();
                dsV.style = "color=#9467bd;fillOpacity=0.45";
                fm.pushDataset(std::move(dsV));

                // Slim box (Q1..Q3) at the centre.
                const double bxW = 0.06;
                std::ostringstream bxs, bys;
                bxs << '[' << (cx - bxW) << ',' << (cx + bxW) << ','
                    << (cx + bxW) << ',' << (cx - bxW) << ']';
                bys << '[' << q1 << ',' << q1 << ',' << q3 << ',' << q3 << ']';
                DatasetInfo dsBx;
                dsBx.type = "polygon";
                dsBx.xJson = bxs.str();
                dsBx.yJson = bys.str();
                dsBx.style = "color=#222;fillOpacity=0.85";
                fm.pushDataset(std::move(dsBx));

                // Median dot.
                std::ostringstream mx2, my2;
                mx2 << '[' << cx << ']';
                my2 << '[' << q2 << ']';
                DatasetInfo dsM;
                dsM.type = "scatter";
                dsM.xJson = mx2.str();
                dsM.yJson = my2.str();
                dsM.style = "color=#ffffff";
                dsM.markerSize = 4;
                fm.pushDataset(std::move(dsM));
            }
            fm.emitModified();
            outs[0] = Value();
        });
    // bar3(Z) — 3-D bars. Emits raw 3-D coords so the WebGL renderer
    // can build cuboids with real depth + lighting; no more cabinet
    // pre-projection. Wire format: type='bar3' with the Z-matrix as
    // nested rows (same shape as surf), plus implicit (x, y) coords =
    // (1..C, 1..R).
    reg("bar", "bar3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.empty()) { outs[0] = Value(); return; }
            const auto &Z = args[0];
            const size_t R = Z.dims().rows();
            const size_t C = Z.dims().cols();
            if (R == 0 || C == 0) { outs[0] = Value(); return; }

            std::ostringstream xs, ys, zs;
            xs << '[';
            for (size_t c = 0; c < C; ++c) { if (c) xs << ','; xs << (c + 1); }
            xs << ']';
            ys << '[';
            for (size_t r = 0; r < R; ++r) { if (r) ys << ','; ys << (r + 1); }
            ys << ']';
            zs << '[';
            for (size_t r = 0; r < R; ++r) {
                if (r) zs << ',';
                zs << '[';
                for (size_t c = 0; c < C; ++c) {
                    if (c) zs << ',';
                    const double v = Z.doubleData()[c * R + r];
                    if (std::isfinite(v)) zs << v;
                    else                  zs << "null";
                }
                zs << ']';
            }
            zs << ']';

            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "bar3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    // fill3(X, Y, Z[, C]) — emits raw 3-D vertex arrays. Each column
    // of (X, Y, Z) is one polygon (one polygon for vector inputs).
    // The WebGL renderer builds a triangle fan per polygon under
    // perspective.
    reg("bar", "fill3",
        [](Span<const Value> args, size_t nargout, Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.size() < 3) { outs[0] = Value(); return; }
            const auto &X = args[0], &Y = args[1], &Z = args[2];
            const size_t R = X.dims().rows();
            const size_t C = std::max<size_t>(1, X.dims().cols());
            if (R == 0) { outs[0] = Value(); return; }
            if (Y.dims().rows() != R || Z.dims().rows() != R) {
                outs[0] = Value(); return;
            }

            // Wire format: x/y/z each as an array with `null`
            // separators between polygon groups (matches the existing
            // 2-D polygon convention).
            std::ostringstream xs, ys, zs;
            xs << '['; ys << '['; zs << '[';
            for (size_t c = 0; c < C; ++c) {
                if (c > 0) { xs << ",null,"; ys << ",null,"; zs << ",null,"; }
                for (size_t r = 0; r < R; ++r) {
                    if (r > 0) { xs << ','; ys << ','; zs << ','; }
                    const double xv = X.doubleData()[c * R + r];
                    const double yv = Y.doubleData()[c * R + r];
                    const double zv = Z.doubleData()[c * R + r];
                    if (std::isfinite(xv)) xs << xv; else xs << "null";
                    if (std::isfinite(yv)) ys << yv; else ys << "null";
                    if (std::isfinite(zv)) zs << zv; else zs << "null";
                }
            }
            xs << ']'; ys << ']'; zs << ']';

            std::string color = "#9467bd";
            if (args.size() >= 4 && args[3].isChar()) {
                std::string s = args[3].toString();
                if (s.size() >= 4 && s[0] == '#') color = s;
            }

            auto &fm = gc.fm;
            fm.prepareForPlot();
            DatasetInfo ds;
            ds.type = "fill3";
            ds.xJson = xs.str();
            ds.yJson = ys.str();
            ds.zJson = zs.str();
            std::ostringstream sty;
            sty << "color=" << color << ";fillOpacity=0.7";
            ds.style = sty.str();
            fm.pushDataset(std::move(ds));
            fm.emitModified();
            outs[0] = Value();
        });
    reg("bar", "geoscatter",
        [](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            geoForward("scatter", a, o, gc);
        });
    reg("bar", "geobubble",
        [](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            // bubblechart routing — scatter accepts sizes as its 3rd arg.
            geoForward("scatter", a, o, gc);
        });
    // ────────────────────────────────────────────────────────────────
    // bubblechart / bubblechart3 / swarmchart — variants of scatter
    // / scatter3 that emphasise size-modulated markers. We delegate
    // to the underlying scatter family; the size vector flows through
    // unchanged and the renderer applies it as marker radius.
    // ────────────────────────────────────────────────────────────────
    reg("bar", "bubblechart",
        [](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            delegateTo("scatter", a, o, gc);
        });
    reg("bar", "bubblechart3",
        [](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            delegateTo("scatter3", a, o, gc);
        });
    // swarmchart(x, y[, sizes[, color]]) — scatter with X-axis jitter
    // applied per unique categorical X. For each cluster of points
    // sharing the same X coordinate, points are spread along X within
    // a small interval; the spread amount scales with the cluster's
    // local rank (so the layout resembles a violin plot's swarm).
    reg("bar", "swarmchart",
        [](Span<const Value> args, size_t nargout,
           Span<Value> outs, GraphicsContext &gc) {
            (void)nargout;
            if (args.size() < 2) { outs[0] = Value(); return; }
            const auto &xv = args[0];
            const auto &yv = args[1];
            const size_t n = std::min(xv.numel(), yv.numel());
            auto *mr = gc.mr;
            auto Xjit = Value::matrix(1, n, ValueType::DOUBLE, mr);
            double *xj = Xjit.doubleDataMut();
            for (size_t i = 0; i < n; ++i) xj[i] = xv.elemAsDouble(i);
            // Group indices by integer-rounded X (categorical use case).
            std::map<long, std::vector<size_t>> buckets;
            for (size_t i = 0; i < n; ++i) {
                long key = (long)std::llround(xv.elemAsDouble(i));
                buckets[key].push_back(i);
            }
            // Per bucket, spread points uniformly in [-half, +half] of
            // the slot width. Slot = 0.35 × the smallest gap between
            // distinct X values, capped at 0.4 absolute.
            double minGap = 1.0;
            if (buckets.size() > 1) {
                long prev = LONG_MIN;
                for (const auto &[k, _] : buckets) {
                    if (prev != LONG_MIN) {
                        const double g = std::abs((double)(k - prev));
                        if (g < minGap || minGap == 1.0) minGap = g;
                    }
                    prev = k;
                }
            }
            const double half = std::min(0.4, 0.35 * minGap);
            for (auto &[k, idxs] : buckets) {
                const size_t m = idxs.size();
                if (m <= 1) continue;
                for (size_t j = 0; j < m; ++j) {
                    const double t = (m == 1) ? 0.0
                        : (-half + 2 * half * (double)j / (m - 1));
                    xj[idxs[j]] += t;
                }
            }
            std::vector<Value> proxied;
            proxied.push_back(std::move(Xjit));
            proxied.push_back(yv);
            for (size_t i = 2; i < args.size(); ++i) proxied.push_back(args[i]);
            std::array<Value, 1> outBuf;
            gc.callBuiltin("scatter", Span<const Value>(proxied.data(), proxied.size()), 0,
                           Span<Value>(outBuf.data(), 1));
            outs[0] = Value();
        });
    reg("bar", "swarmchart3",
        [](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            delegateTo("scatter3", a, o, gc);
        });
    // heatmap(C) — table-style heatmap. Same data layout as imagesc:
    // 2-D matrix of values mapped through a colormap. Optional row/
    // column label strings ('RowNames' / 'ColumnNames') are accepted
    // but currently flow through as labels only — full table-style
    // heatmap with cell-text overlays + row/col headers is BACKLOG.
    // Forms supported (subset):
    //   heatmap(C)
    //   heatmap(C, 'Colormap', name)
    // Helper — emit a text-annotation dataset at (x, y) with label.
    // Picks a colour that contrasts with the cell value (white on
    // dark bins, black on light bins) via a brightness threshold.
    auto pushCellText = [](FigureManager &fm, double x, double y,
                           const std::string &label, bool darkBg) {
        DatasetInfo t;
        t.type = "text";
        std::ostringstream xs, ys;
        xs << '[' << x << ']'; ys << '[' << y << ']';
        t.xJson = xs.str();
        t.yJson = ys.str();
        t.label = label;
        t.style = std::string("color=") + (darkBg ? "#f0f0f0" : "#202020")
                  + ";fontSize=11";
        fm.pushDataset(std::move(t));
    };
    reg("bar", "heatmap",
        [pushCellText](Span<const Value> a, size_t, Span<Value> o, GraphicsContext &gc) {
            // Drop trailing N-V pairs that the imagesc adapter doesn't
            // know about — keep only the first numeric argument as the
            // data matrix. 'Colormap' is honoured via a colormap() call
            // right after; 'CellLabelColor' / 'CellLabelFormat' etc.
            // are accepted as no-ops. Cell-label text is enabled by
            // default for heatmap(table); pass 'CellLabel','off' to
            // disable.
            std::vector<Value> proxied;
            std::string cmap;
            bool cellLabel = true;
            std::string labelFmt = "%g";
            for (size_t i = 0; i < a.size(); ++i) {
                if (a[i].isChar()) {
                    if (i + 1 < a.size()) {
                        std::string key = a[i].toString();
                        for (auto &cc : key)
                            cc = (char)std::tolower((unsigned char)cc);
                        if (key == "colormap" && a[i + 1].isChar()) {
                            cmap = a[i + 1].toString();
                            ++i; continue;
                        }
                        if (key == "celllabel" && a[i + 1].isChar()) {
                            std::string v = a[i + 1].toString();
                            for (auto &cc : v) cc = (char)std::tolower((unsigned char)cc);
                            cellLabel = (v != "off");
                            ++i; continue;
                        }
                        if (key == "celllabelformat" && a[i + 1].isChar()) {
                            labelFmt = a[i + 1].toString();
                            ++i; continue;
                        }
                    }
                    if (i + 1 < a.size()) ++i;
                    continue;
                }
                proxied.push_back(a[i]);
            }
            delegateTo("imagesc",
                       Span<const Value>(proxied.data(), proxied.size()), o, gc);
            if (!cmap.empty()) {
                gc.fm.currentAxes().colormapName = cmap;
            }
            // Cell-text overlay — one <text> per cell with the
            // formatted value. Skipped for very large matrices to
            // avoid clutter.
            if (cellLabel && !proxied.empty()) {
                const auto &C = proxied[0];
                const size_t R = C.dims().rows();
                const size_t W = C.dims().cols();
                if (R * W <= 400) {   // ≤ 20×20 grid — show labels
                    double cmn = std::numeric_limits<double>::infinity();
                    double cmx = -std::numeric_limits<double>::infinity();
                    for (size_t i = 0; i < R * W; ++i) {
                        const double v = C.elemAsDouble(i);
                        if (std::isfinite(v)) {
                            if (v < cmn) cmn = v;
                            if (v > cmx) cmx = v;
                        }
                    }
                    auto &fm = gc.fm;
                    for (size_t r = 0; r < R; ++r) {
                        for (size_t cc = 0; cc < W; ++cc) {
                            const double v = C.doubleData()[cc * R + r];
                            const double t_ = (cmx == cmn) ? 0.5
                                : (v - cmn) / (cmx - cmn);
                            // Dark background ↔ low t (blue/cyan side
                            // of parula). Threshold 0.4 picks readable
                            // colour against parula's gradient.
                            const bool dark = (t_ < 0.4);
                            char buf[32];
                            std::snprintf(buf, sizeof buf, labelFmt.c_str(), v);
                            pushCellText(fm,
                                         (double)(cc + 1),
                                         (double)(r + 1),
                                         std::string(buf), dark);
                        }
                    }
                }
            }
            gc.fm.current().modified = true;
            gc.fm.emitModified();
        });
    // confusionchart(C [, classNames]) — confusion matrix heatmap
    // with cell-value labels. Same as heatmap(C) but adds explicit
    // axis labels and (when classNames given) categorical tick
    // labels on both axes.
    reg("bar", "confusionchart",
        [](Span<const Value> args, size_t, Span<Value> outs, GraphicsContext &gc) {
            if (args.empty()) { outs[0] = Value(); return; }
            // Forward to heatmap (cellLabel default on, no Colormap).
            std::array<Value, 1> outBuf;
            std::array<Value, 1> proxied{ args[0] };
            if (!gc.callBuiltin("heatmap", Span<const Value>(proxied.data(), 1), 0,
                                Span<Value>(outBuf.data(), 1))) { outs[0] = Value(); return; }
            auto &fm = gc.fm;
            auto &ax = fm.currentAxes();
            ax.xlabel = "Predicted Class";
            ax.ylabel = "True Class";
            // Optional class names as 2nd arg (cell of chars / string array).
            if (args.size() >= 2) {
                const Value &cn = args[1];
                const size_t n = cn.numel();
                std::ostringstream js;
                js << '[';
                for (size_t i = 0; i < n; ++i) {
                    if (i) js << ',';
                    Value elem = cn.elemAt(i, gc.mr);
                    std::string s = elem.isChar() ? elem.toString() : "";
                    js << '"';
                    for (char ch : s) {
                        if (ch == '"' || ch == '\\') js << '\\';
                        js << ch;
                    }
                    js << '"';
                }
                js << ']';
                ax.xTickLabelsJson = js.str();
                ax.yTickLabelsJson = js.str();
                // Tick positions = 1..n so they align with cell centres.
                std::ostringstream ts; ts << '[';
                for (size_t i = 0; i < n; ++i) {
                    if (i) ts << ',';
                    ts << (i + 1);
                }
                ts << ']';
                ax.xTicksJson = ts.str();
                ax.yTicksJson = ts.str();
            }
            fm.current().modified = true;
            fm.emitModified();
            outs[0] = Value();
        });
}

}  // namespace numkit
