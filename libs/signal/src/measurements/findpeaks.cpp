// libs/signal/src/measurements/findpeaks.cpp
//
// findpeaks — strict local maxima with MATLAB-compatible Name-Value
// options (MinPeakHeight, Threshold, MinPeakDistance, NPeaks, SortStr)
// plus the location form findpeaks(Y,X) / findpeaks(Y,Fs).

#include <numkit/signal/measurements/findpeaks.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>

namespace numkit::signal {

namespace {

struct PeakOpts {
    double minHeight  = -std::numeric_limits<double>::infinity(); // keep if v > minHeight
    double threshold  = 0.0;     // keep if min(v-left, v-right) >= threshold
    double minDist    = 0.0;     // in location units; <=0 => disabled
    bool   useMinDist = false;
    long   nPeaks     = -1;      // -1 => unlimited
    int    sortMode   = 0;       // 0 none(location), 1 ascend(height), 2 descend(height)
};

// Core detection + filtering. locArr == nullptr => locations are 1-based
// sample indices; otherwise locArr[i] is the location of sample i.
std::tuple<Value, Value>
findpeaksImpl(const Value &x, const double *locArr, const PeakOpts &opt,
              std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto val = ScratchVec<double>(&scratch);   // peak heights
    auto loc = ScratchVec<double>(&scratch);   // peak locations

    const std::size_t n = x.numel();
    if (n >= 3) {
        const double *p = x.doubleData();
        for (std::size_t i = 1; i + 1 < n; ++i) {
            const double v = p[i];
            if (std::isnan(v) || std::isnan(p[i - 1]) || std::isnan(p[i + 1]))
                continue;
            if (!(v > p[i - 1] && v > p[i + 1]))   // strict local maximum
                continue;
            if (!(v > opt.minHeight))              // MinPeakHeight (strictly greater)
                continue;
            const double drop = std::min(v - p[i - 1], v - p[i + 1]);
            if (drop < opt.threshold)              // Threshold (>= vertical drop)
                continue;
            val.push_back(v);
            loc.push_back(locArr ? locArr[i] : static_cast<double>(i + 1));
        }
    }

    const std::size_t m = val.size();
    auto keep = ScratchVec<unsigned char>(&scratch);
    keep.assign(m, 1u);

    // MinPeakDistance: greedy, tallest-first, remove peaks within the distance.
    if (opt.useMinDist && opt.minDist > 0.0 && m > 1) {
        auto order = ScratchVec<std::size_t>(&scratch);
        for (std::size_t k = 0; k < m; ++k) order.push_back(k);
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            if (val[a] < val[b]) return false;
            if (val[b] < val[a]) return true;
            return loc[a] < loc[b];               // ties: lower location first
        });
        for (std::size_t oi = 0; oi < m; ++oi) {
            const std::size_t a = order[oi];
            if (!keep[a]) continue;
            for (std::size_t oj = oi + 1; oj < m; ++oj) {
                const std::size_t b = order[oj];
                if (keep[b] && std::fabs(loc[a] - loc[b]) < opt.minDist)
                    keep[b] = 0u;
            }
        }
    }

    // Survivors, still in ascending-location order.
    auto surv = ScratchVec<std::size_t>(&scratch);
    for (std::size_t k = 0; k < m; ++k)
        if (keep[k]) surv.push_back(k);

    // SortStr: reorder by height (ties keep ascending location).
    if (opt.sortMode == 1) {
        std::stable_sort(surv.begin(), surv.end(), [&](std::size_t a, std::size_t b) {
            if (val[a] < val[b]) return true;
            if (val[b] < val[a]) return false;
            return loc[a] < loc[b];
        });
    } else if (opt.sortMode == 2) {
        std::stable_sort(surv.begin(), surv.end(), [&](std::size_t a, std::size_t b) {
            if (val[a] < val[b]) return false;
            if (val[b] < val[a]) return true;
            return loc[a] < loc[b];
        });
    }

    // NPeaks truncation (after sorting).
    std::size_t keepN = surv.size();
    if (opt.nPeaks >= 0 && static_cast<std::size_t>(opt.nPeaks) < keepN)
        keepN = static_cast<std::size_t>(opt.nPeaks);

    // Output orientation matches input column-vector orientation.
    const bool col = (x.dims().ndim() == 2 && x.dims().dim(0) > 1 && x.dims().dim(1) == 1);
    Value vals = col ? Value::matrix(keepN, 1, ValueType::DOUBLE, mr)
                     : Value::matrix(1, keepN, ValueType::DOUBLE, mr);
    Value locs = col ? Value::matrix(keepN, 1, ValueType::DOUBLE, mr)
                     : Value::matrix(1, keepN, ValueType::DOUBLE, mr);
    double *vp = vals.doubleDataMut();
    double *lp = locs.doubleDataMut();
    for (std::size_t k = 0; k < keepN; ++k) {
        vp[k] = val[surv[k]];
        lp[k] = loc[surv[k]];
    }
    return std::make_tuple(std::move(vals), std::move(locs));
}

} // namespace

// Public no-option entry point (strict local maxima, 1-based indices).
std::tuple<Value, Value>
findpeaks(const Value &x, std::pmr::memory_resource *mr)
{
    return findpeaksImpl(x, nullptr, PeakOpts{}, mr);
}

namespace detail {

void findpeaks_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("findpeaks: requires at least 1 argument",
                     0, 0, "findpeaks", "", "numkit:findpeaks:nargin");
    auto *mr = ctx.engine->resource();
    const Value &y = args[0];

    if (nargout > 2)
        throw Error("findpeaks: width/prominence outputs (w,p) not supported",
                     0, 0, "findpeaks", "", "numkit:findpeaks:nargout");

    PeakOpts opt;
    ScratchArena locScratch(mr);
    auto locStore = ScratchVec<double>(&locScratch); // for the Fs form
    const double *locArr = nullptr;

    std::size_t argi = 1;

    // Optional positional X-vector or Fs-scalar (before any Name-Value pair).
    if (args.size() > 1 && !args[1].isChar() && !args[1].isString()) {
        const Value &second = args[1];
        const std::size_t n = y.numel();
        if (second.numel() == 1) {
            // Sampling frequency: location(i) = i / Fs  (0-based sample index).
            const double fs = second.toScalar();
            if (!(fs > 0.0))
                throw Error("findpeaks: sample rate Fs must be positive",
                             0, 0, "findpeaks", "", "numkit:findpeaks:badFs");
            locStore.assign(n, 0.0);
            for (std::size_t i = 0; i < n; ++i)
                locStore[i] = static_cast<double>(i) / fs;
            locArr = locStore.data();
        } else {
            if (second.numel() != n)
                throw Error("findpeaks: X must have the same length as Y",
                             0, 0, "findpeaks", "", "numkit:findpeaks:badX");
            locArr = second.doubleData();
        }
        argi = 2;
    }

    // Name-Value options.
    for (; argi < args.size(); argi += 2) {
        if (!args[argi].isChar() && !args[argi].isString())
            throw Error("findpeaks: expected an option name",
                         0, 0, "findpeaks", "", "numkit:findpeaks:badOpt");
        std::string name = args[argi].toString();
        for (char &c : name)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (argi + 1 >= args.size())
            throw Error("findpeaks: option '" + args[argi].toString() + "' needs a value",
                         0, 0, "findpeaks", "", "numkit:findpeaks:badOpt");
        const Value &v = args[argi + 1];

        if (name == "minpeakheight") {
            opt.minHeight = v.toScalar();
        } else if (name == "threshold") {
            opt.threshold = v.toScalar();
        } else if (name == "minpeakdistance") {
            opt.minDist = v.toScalar();
            opt.useMinDist = true;
        } else if (name == "npeaks") {
            opt.nPeaks = static_cast<long>(v.toScalar());
        } else if (name == "sortstr") {
            std::string s = v.toString();
            for (char &c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if      (s == "none")    opt.sortMode = 0;
            else if (s == "ascend")  opt.sortMode = 1;
            else if (s == "descend") opt.sortMode = 2;
            else
                throw Error("findpeaks: SortStr must be 'none'/'ascend'/'descend'",
                             0, 0, "findpeaks", "", "numkit:findpeaks:badOpt");
        } else {
            // MinPeakProminence / MinPeakWidth / WidthReference / Annotate /
            // DoubleSided are not yet supported — fail loudly rather than
            // silently ignore.
            throw Error("findpeaks: option '" + args[argi].toString() + "' not supported",
                         0, 0, "findpeaks", "", "numkit:findpeaks:unsupportedOpt");
        }
    }

    auto [vals, idxs] = findpeaksImpl(y, locArr, opt, mr);
    outs[0] = std::move(vals);
    if (nargout > 1) outs[1] = std::move(idxs);
}

} // namespace detail

} // namespace numkit::signal
