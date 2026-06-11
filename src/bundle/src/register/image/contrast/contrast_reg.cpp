// toolboxes/image/src/contrast/contrast_reg.cpp
//
// Register half of the image contrast builtins: the CallContext wrappers
// delegating to the engine-free compute in contrast.cpp. library.cpp
// forward-declares + registers these by name. Note imcontrast-style entries
// touch the engine's FigureManager, so core/figure_manager.hpp lives here on
// the register side (never in the engine-free compute TU).
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/contrast/contrast.hpp>
#include <numkit/image/filter/filter.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/figure/figure_manager.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <cctype>
#include <cstddef>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace numkit::image {

namespace detail {

void imhistmatch_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imhistmatch: requires (I, ref [, nbins])",
                    0, 0, "imhistmatch", "", "numkit:imhistmatch:nargin");
    int n = (args.size() >= 3 && !args[2].isEmpty())
            ? (int)args[2].toScalar() : 0;
    // 2nd output `hgram` (= imhist(ref, nbins)') only when requested.
    Value hgram;
    Value *hgp = (nargout >= 2) ? &hgram : nullptr;
    outs[0] = imhistmatch(args[0], args[1], n, hgp, ctx.engine->resource());
    if (nargout >= 2) outs[1] = std::move(hgram);
}

void imhist_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imhist: requires (I[, n])", 0, 0, "imhist", "",
                    "numkit:imhist:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 0;
    auto [c, x] = imhist(args[0], n, ctx.engine->resource());

    // Auto-plot when called without LHS — MATLAB convention.
    // imhist(I) draws a vertical bar chart of the bin counts.
    if (nargout == 0) {
        auto &fm = ctx.engine->figureManager();
        fm.prepareForPlot();
        const std::size_t nb = c.numel();
        std::ostringstream xs, ys;
        xs << '['; ys << '[';
        for (std::size_t i = 0; i < nb; ++i) {
            if (i) { xs << ','; ys << ','; }
            xs << x.elemAsDouble(i);
            ys << c.elemAsDouble(i);
        }
        xs << ']'; ys << ']';
        DatasetInfo ds;
        ds.type  = "bar";
        ds.xJson = xs.str();
        ds.yJson = ys.str();
        ds.style = "color=#7fa6c6";
        fm.pushDataset(std::move(ds));
        fm.emitModified();
        return;
    }

    if (nargout > 0) outs[0] = std::move(c);
    if (nargout > 1) outs[1] = std::move(x);
}

void stretchlim_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("stretchlim: requires (I[, tol])", 0, 0, "stretchlim", "",
                    "numkit:stretchlim:nargin");
    double lo = 0.01, hi = 0.99;
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &t = args[1];
        if (t.numel() == 1) {
            lo = t.toScalar();
            hi = 1.0 - lo;
        } else if (t.numel() >= 2) {
            lo = t.elemAsDouble(0);
            hi = t.elemAsDouble(1);
        }
    }
    outs[0] = stretchlim(args[0], lo, hi, ctx.engine->resource());
}

void imadjust_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imadjust: requires (I[, [low_in high_in]][, [low_out high_out]][, gamma])",
                    0, 0, "imadjust", "", "numkit:imadjust:nargin");
    double low_in  = std::numeric_limits<double>::quiet_NaN();
    double high_in = std::numeric_limits<double>::quiet_NaN();
    double low_out = 0.0;
    double high_out = 1.0;
    double gamma = 1.0;

    // In-range endpoints [low_in high_in]:
    //   - absent (1-arg imadjust(I))   -> NaN sentinel -> stretchlim auto
    //                                      (1% saturation, the default-contrast form).
    //   - empty [] (explicitly passed) -> MATLAB default [0 1] (identity, NOT
    //                                      stretchlim) — distinct from the absent case.
    //   - explicit [lo hi]             -> use as given.
    if (args.size() >= 2) {
        const Value &v = args[1];
        if (v.isEmpty()) {
            low_in  = 0.0;
            high_in = 1.0;
        }
        else if (v.numel() >= 2) {
            low_in  = v.elemAsDouble(0);
            high_in = v.elemAsDouble(1);
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty()) {
        const Value &v = args[2];
        if (v.numel() >= 2) {
            low_out  = v.elemAsDouble(0);
            high_out = v.elemAsDouble(1);
        }
    }
    if (args.size() >= 4 && !args[3].isEmpty()) gamma = args[3].toScalar();

    outs[0] = imadjust(args[0], low_in, high_in, low_out, high_out, gamma, ctx.engine->resource());
}

void histeq_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("histeq: requires (I[, n])", 0, 0, "histeq", "",
                    "numkit:histeq:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 64;
    outs[0] = histeq(args[0], n, ctx.engine->resource());
}

void adapthisteq_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("adapthisteq: requires (I[, NV-pairs...])",
                     0, 0, "adapthisteq", "", "numkit:adapthisteq:nargin");

    AdaptHistEqOptions opts;

    auto eqIgnoreCase = [](const std::string &a, const char *b) {
        if (a.size() != std::strlen(b)) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::tolower(a[i]) != std::tolower(b[i])) return false;
        return true;
    };
    for (size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar())
            throw Error("adapthisteq: NV-pair name must be a string",
                         0, 0, "adapthisteq", "", "numkit:adapthisteq:badNVName");
        const std::string key = args[i].toString();
        const Value &v        = args[i + 1];
        if (eqIgnoreCase(key, "NumTiles")) {
            if (v.numel() < 2)
                throw Error("adapthisteq: NumTiles must be 2-element",
                             0, 0, "adapthisteq", "", "numkit:adapthisteq:badNumTiles");
            opts.numTilesR = (int)v.elemAsDouble(0);
            opts.numTilesC = (int)v.elemAsDouble(1);
        } else if (eqIgnoreCase(key, "ClipLimit"))    opts.clipLimit    = v.toScalar();
        else if (eqIgnoreCase(key, "NBins"))          opts.nBins        = (int)v.toScalar();
        else if (eqIgnoreCase(key, "Distribution"))   opts.distribution = v.toString();
        else if (eqIgnoreCase(key, "Alpha"))          opts.alpha        = v.toScalar();
        else if (eqIgnoreCase(key, "Range"))          opts.range        = v.toString();
        else {
            throw Error("adapthisteq: unknown NV-pair key '" + key + "'",
                         0, 0, "adapthisteq", "", "numkit:adapthisteq:badNVKey");
        }
    }
    outs[0] = adapthisteq(args[0], opts, ctx.engine->resource());
}

void graythresh_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("graythresh: requires I", 0, 0, "graythresh", "",
                    "numkit:graythresh:nargin");
    auto [t, em] = graythresh(args[0], ctx.engine->resource());
    outs[0] = std::move(t);
    if (nargout > 1) outs[1] = std::move(em);
}

void otsuthresh_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("otsuthresh: requires counts", 0, 0, "otsuthresh", "",
                    "numkit:otsuthresh:nargin");
    auto [t, em] = otsuthresh(args[0], ctx.engine->resource());
    outs[0] = std::move(t);
    if (nargout > 1) outs[1] = std::move(em);
}

void multithresh_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("multithresh: requires (I[, N])", 0, 0, "multithresh", "",
                    "numkit:multithresh:nargin");
    int N = (args.size() >= 2 && !args[1].isEmpty()) ? (int)args[1].toScalar() : 1;
    auto [t, em] = multithresh(args[0], N, ctx.engine->resource());
    outs[0] = std::move(t);
    if (nargout > 1) outs[1] = std::move(em);
}

void imbinarize_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imbinarize: requires (I[, thresh])", 0, 0, "imbinarize", "",
                    "numkit:imbinarize:nargin");
    auto *mr = ctx.engine->resource();

    // Method-string form: imbinarize(I, 'global') | imbinarize(I, 'adaptive', ...).
    if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
        std::string m = args[1].toString();
        for (char &c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (m == "global") {
            auto [t, em] = graythresh(args[0], mr);
            (void)em;
            outs[0] = imbinarize(args[0], t.toScalar(), mr);
            return;
        }
        if (m == "adaptive") {
            // imbinarize(I,'adaptive') = binarize(I, adaptthresh(I, sens, polarity)).
            double sens = 0.5;
            std::string polarity = "bright";
            for (std::size_t i = 2; i + 1 < args.size(); i += 2) {
                if (!args[i].isChar() && !args[i].isString())
                    throw Error("imbinarize: expected a Name-Value pair",
                                0, 0, "imbinarize", "", "numkit:imbinarize:nv");
                std::string nm = args[i].toString();
                for (char &c : nm) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (nm == "sensitivity")
                    sens = args[i + 1].toScalar();
                else if (nm == "foregroundpolarity") {
                    polarity = args[i + 1].toString();
                    for (char &c : polarity)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                } else
                    throw Error("imbinarize: unknown option '" + args[i].toString() + "'",
                                0, 0, "imbinarize", "", "numkit:imbinarize:opt");
            }
            if (polarity != "bright")
                throw Error("imbinarize: ForegroundPolarity 'dark' is not yet supported",
                            0, 0, "imbinarize", "", "numkit:imbinarize:polarity");
            Value T = adaptthresh(args[0], sens, 0, "mean", mr);
            outs[0] = imbinarize(args[0], T, mr);
            return;
        }
        throw Error("imbinarize: method must be 'global' or 'adaptive'",
                    0, 0, "imbinarize", "", "numkit:imbinarize:method");
    }

    if (args.size() >= 2 && !args[1].isEmpty()) {
        // Dispatch on T's shape: scalar → fast path, matrix → per-pixel.
        if (args[1].numel() == 1) {
            outs[0] = imbinarize(args[0], args[1].toScalar(), mr);
        } else {
            outs[0] = imbinarize(args[0], args[1], mr);
        }
    } else {
        // No threshold given: pick Otsu's automatically.
        auto [t, _] = graythresh(args[0], mr);
        outs[0] = imbinarize(args[0], t.toScalar(), mr);
    }
}

void imquantize_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imquantize: requires (I, levels[, values])",
                    0, 0, "imquantize", "", "numkit:imquantize:nargin");
    auto *mr = ctx.engine->resource();
    // idx = the 1-based quantization index in 1..numel(levels)+1.
    Value idx = imquantize(args[0], args[1], mr);

    const bool hasValues = (args.size() >= 3 && !args[2].isEmpty());
    if (hasValues) {
        // quant = values(idx). values must hold numel(levels)+1 entries.
        const std::size_t Lcount = args[1].numel();
        if (args[2].numel() != Lcount + 1)
            throw Error("imquantize: values must have numel(levels)+1 elements",
                        0, 0, "imquantize", "", "numkit:imquantize:values");
        Value quant = idx;                 // same shape, DOUBLE
        double *qd = quant.doubleDataMut();
        const double *id = idx.doubleData();
        const std::size_t N = idx.numel();
        for (std::size_t i = 0; i < N; ++i)
            qd[i] = args[2].elemAsDouble(static_cast<std::size_t>(id[i]) - 1);
        outs[0] = std::move(quant);
        if (nargout >= 2) outs[1] = std::move(idx);
    } else {
        if (nargout >= 2) { outs[0] = idx; outs[1] = std::move(idx); }
        else              outs[0] = std::move(idx);
    }
}

void adaptthresh_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("adaptthresh: requires (I [, sensitivity [, n [, stat]]])",
                    0, 0, "adaptthresh", "", "numkit:adaptthresh:nargin");
    const double sens = (args.size() >= 2 && !args[1].isEmpty())
                        ? args[1].toScalar() : 0.5;
    const int nbh     = (args.size() >= 3 && !args[2].isEmpty())
                        ? static_cast<int>(args[2].toScalar()) : 0;
    std::string stat  = "mean";
    if (args.size() >= 4 && !args[3].isEmpty()) {
        if (!args[3].isChar() && !args[3].isString())
            throw Error("adaptthresh: statistic must be a string",
                        0, 0, "adaptthresh", "", "numkit:adaptthresh:type");
        stat = args[3].toString();
    }
    outs[0] = adaptthresh(args[0], sens, nbh, stat, ctx.engine->resource());
}

void imflatfield_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("imflatfield: requires (I, sigma [, mask])",
                    0, 0, "imflatfield", "", "numkit:imflatfield:nargin");
    const double sigma = args[1].toScalar();
    Value mask;
    if (args.size() >= 3 && !args[2].isEmpty()) mask = args[2];
    outs[0] = imflatfield(args[0], sigma, mask, ctx.engine->resource());
}

void wcodemat_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("wcodemat: requires (X [, nb [, opt [, absol]]])",
                    0, 0, "wcodemat", "", "numkit:wcodemat:nargin");
    int nb = (args.size() >= 2 && !args[1].isEmpty())
             ? static_cast<int>(args[1].toScalar()) : 16;
    std::string opt = "mat";
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("wcodemat: opt must be a string",
                        0, 0, "wcodemat", "", "numkit:wcodemat:opt");
        opt = args[2].toString();
    }
    int absol = (args.size() >= 4 && !args[3].isEmpty())
                ? static_cast<int>(args[3].toScalar()) : 1;
    outs[0] = wcodemat(args[0], nb, opt, absol, ctx.engine->resource());
}

void entropy_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("entropy: requires (I [, nbins])", 0, 0, "entropy", "",
                    "numkit:entropy:nargin");
    int nbins = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        nbins = static_cast<int>(args[1].toScalar());
    outs[0] = entropy(args[0], nbins, ctx.engine->resource());
}

void grayslice_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("grayslice: requires (I [, n])", 0, 0, "grayslice", "",
                    "numkit:grayslice:nargin");
    auto *mr = ctx.engine->resource();
    // Decide which overload to dispatch on, mirroring MATLAB's
    // magic-polymorphism of grayslice's 2nd argument.
    //   - missing / empty         → default 10 levels
    //   - scalar ≥ 1              → level-count overload
    //   - vector or scalar 0<n<1  → explicit-thresholds overload
    if (args.size() < 2 || args[1].isEmpty()) {
        outs[0] = grayslice(args[0], 10, mr);
        return;
    }
    const Value &n = args[1];
    if (n.numel() == 1) {
        const double nv = n.toScalar();
        if (nv >= 1.0) {
            outs[0] = grayslice(args[0], static_cast<int>(nv), mr);
        } else if (nv > 0.0) {
            const double levels[1] = { nv };
            outs[0] = grayslice(args[0], Span<const double>(levels, 1), mr);
        } else {
            throw Error("grayslice: N must be a positive number",
                        0, 0, "grayslice", "", "numkit:grayslice:n");
        }
    } else if (n.numel() > 1) {
        ScratchArena scratch(mr);
        ScratchVec<double> buf(n.numel(), &scratch);
        for (size_t i = 0; i < n.numel(); ++i) buf[i] = n.elemAsDouble(i);
        outs[0] = grayslice(args[0], Span<const double>(buf.data(), buf.size()),
                            mr);
    } else {
        throw Error("grayslice: N must be scalar >= 1 or a vector",
                    0, 0, "grayslice", "", "numkit:grayslice:nargin");
    }
}

} // namespace detail
} // namespace numkit::image
