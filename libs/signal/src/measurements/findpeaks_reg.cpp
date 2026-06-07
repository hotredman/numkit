// libs/signal/src/measurements/findpeaks_reg.cpp
//
// CallContext register half of measurements/findpeaks.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/measurements/findpeaks.hpp>
#include "findpeaks_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
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

namespace numkit::signal {

namespace detail {

void findpeaks_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("findpeaks: requires at least 1 argument",
                     0, 0, "findpeaks", "", "numkit:findpeaks:nargin");
    auto *mr = ctx.engine->resource();
    const Value &y = args[0];

    if (nargout > 4)
        throw Error("findpeaks: at most 4 outputs (pks, locs, w, p)",
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
        } else if (name == "minpeakprominence") {
            opt.minProm = v.toScalar();
            opt.useMinProm = true;
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
            // MinPeakWidth / WidthReference / Annotate / DoubleSided are not
            // yet supported — fail loudly rather than silently ignore.
            throw Error("findpeaks: option '" + args[argi].toString() + "' not supported",
                         0, 0, "findpeaks", "", "numkit:findpeaks:unsupportedOpt");
        }
    }

    auto [vals, idxs, wids, proms] = findpeaksImpl(y, locArr, opt, mr);
    outs[0] = std::move(vals);
    if (nargout > 1) outs[1] = std::move(idxs);
    if (nargout > 2) outs[2] = std::move(wids);
    if (nargout > 3) outs[3] = std::move(proms);
}

} // namespace detail

} // namespace numkit::signal
