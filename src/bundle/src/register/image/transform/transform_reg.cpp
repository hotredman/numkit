// toolboxes/image/src/transform/transform_reg.cpp
//
// Register half of the image transform builtins: the CallContext wrappers
// delegating to the engine-free compute in transform.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/transform/transform.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

void integralImage_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("integralImage: requires (I)",
                    0, 0, "integralImage", "", "numkit:integralImage:nargin");
    outs[0] = integralImage(args[0], ctx.engine->resource());
}

void integralImage3_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("integralImage3: requires (V)",
                    0, 0, "integralImage3", "", "numkit:integralImage3:nargin");
    outs[0] = integralImage3(args[0], ctx.engine->resource());
}

void dct2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("dct2: requires 1 argument",
                    0, 0, "dct2", "", "numkit:dct2:nargin");
    outs[0] = dct2(args[0], ctx.engine->resource());
}

void idct2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("idct2: requires 1 argument",
                    0, 0, "idct2", "", "numkit:idct2:nargin");
    outs[0] = idct2(args[0], ctx.engine->resource());
}

void dctmtx_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("dctmtx: requires 1 argument (N)",
                    0, 0, "dctmtx", "", "numkit:dctmtx:nargin");
    outs[0] = dctmtx(args[0].toScalar(), ctx.engine->resource());
}

void phantom_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    Value model_or_E;
    size_t n = 0;
    // Argument forms:
    //   phantom()                            -> defaults
    //   phantom(model_str | E)               -> single arg
    //   phantom(N)                           -> single numeric scalar
    //   phantom(model_str | E, N)            -> two args
    if (args.size() == 1) {
        const Value &a = args[0];
        if (!a.isEmpty() && (a.isChar() || a.isString() || a.numel() != 1))
            model_or_E = a;
        else if (!a.isEmpty())
            n = static_cast<size_t>(a.toScalar());
    } else if (args.size() >= 2) {
        if (!args[0].isEmpty()) model_or_E = args[0];
        if (!args[1].isEmpty()) n = static_cast<size_t>(args[1].toScalar());
    }
    auto [head, E] = phantom(model_or_E, n, mr);
    outs[0] = std::move(head);
    if (nargout > 1) outs[1] = std::move(E);
}

void normxcorr2_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("normxcorr2: requires (template, img)",
                    0, 0, "normxcorr2", "", "numkit:normxcorr2:nargin");
    outs[0] = normxcorr2(args[0], args[1], ctx.engine->resource());
}

void psf2otf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("psf2otf: requires (PSF [, outsize])",
                    0, 0, "psf2otf", "", "numkit:psf2otf:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<size_t> outsizeBuf(&scratch);
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const size_t n = args[1].numel();
        outsizeBuf.reserve(n);
        for (size_t i = 0; i < n; ++i)
            outsizeBuf.push_back(static_cast<size_t>(args[1].elemAsDouble(i)));
    }
    outs[0] = psf2otf(args[0], Span<const size_t>(outsizeBuf.data(), outsizeBuf.size()), mr);
}

void bestblk_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bestblk: requires (IMS [, k])",
                    0, 0, "bestblk", "", "numkit:bestblk:nargin");
    auto *mr = ctx.engine->resource();
    const double k = (args.size() >= 2 && !args[1].isEmpty())
                       ? args[1].toScalar() : 100.0;
    Value v = bestblk(args[0], k, mr);
    if (nargout <= 1) { outs[0] = std::move(v); return; }
    // Multi-output form: split row vector into scalars.
    const size_t nd = v.numel();
    const size_t M = std::min<size_t>(nargout, nd);
    for (size_t i = 0; i < M; ++i) {
        outs[i] = Value::scalar(v.elemAsDouble(i), mr);
    }
}

void fftconv2_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fftconv2: requires (A, B [, shape])",
                    0, 0, "fftconv2", "", "numkit:fftconv2:nargin");
    std::string shape = "full";
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("fftconv2: shape must be a string",
                        0, 0, "fftconv2", "", "numkit:fftconv2:shape");
        shape = args[2].toString();
    }
    outs[0] = fftconv2(args[0], args[1], shape, ctx.engine->resource());
}

void edgetaper_reg(Span<const Value> args, std::size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("edgetaper: requires (I, PSF)",
                    0, 0, "edgetaper", "", "numkit:edgetaper:nargin");
    outs[0] = edgetaper(args[0], args[1], ctx.engine->resource());
}

void deconvreg_reg(Span<const Value> args, std::size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args.size() > 5)
        throw Error("deconvreg: requires (I, PSF [, NP [, LRANGE [, REGOP]]])",
                    0, 0, "deconvreg", "", "numkit:deconvreg:nargin");
    auto *mr = ctx.engine->resource();
    double np = 0.0;
    double lo = 1e-9, hi = 1e9;
    Value regop = Value::Empty;
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (args[2].numel() != 1)
            throw Error("deconvreg: NP must be a scalar",
                        0, 0, "deconvreg", "", "numkit:deconvreg:np");
        np = args[2].toScalar();
    }
    if (args.size() >= 4 && !args[3].isEmpty()) {
        const Value &lr = args[3];
        if (lr.numel() == 1) {
            lo = hi = lr.toScalar();
        } else if (lr.numel() == 2) {
            lo = lr.elemAsDouble(0);
            hi = lr.elemAsDouble(1);
            if (hi < lo)
                throw Error("deconvreg: LRANGE must satisfy lo <= hi",
                            0, 0, "deconvreg", "", "numkit:deconvreg:lrange");
        } else {
            throw Error("deconvreg: LRANGE must be a scalar or 2-element vector",
                        0, 0, "deconvreg", "", "numkit:deconvreg:lrange");
        }
    }
    if (args.size() >= 5 && !args[4].isEmpty())
        regop = args[4];
    auto r = deconvreg(args[0], args[1], np, lo, hi, regop, mr);
    outs[0] = std::move(r.J);
    if (nargout > 1) outs[1] = Value::scalar(r.lagra, mr);
}

void deconvwnr_reg(Span<const Value> args, std::size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("deconvwnr: requires (I, PSF [, NSR | NCORR, ICORR])",
                    0, 0, "deconvwnr", "", "numkit:deconvwnr:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        outs[0] = deconvwnr(args[0], args[1], 0.0, mr);
    } else if (args.size() == 3) {
        // 3-arg form: scalar NSR (most common case).
        if (args[2].numel() != 1)
            throw Error("deconvwnr: 3-arg NSR must be a scalar; use the "
                        "4-arg (NCORR, ICORR) form for array spectra",
                        0, 0, "deconvwnr", "", "numkit:deconvwnr:nsr");
        outs[0] = deconvwnr(args[0], args[1], args[2].toScalar(), mr);
    } else {
        outs[0] = deconvwnr(args[0], args[1], args[2], args[3], mr);
    }
}

void otf2psf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("otf2psf: requires (OTF [, outsize])",
                    0, 0, "otf2psf", "", "numkit:otf2psf:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<size_t> outsizeBuf(&scratch);
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const size_t n = args[1].numel();
        outsizeBuf.reserve(n);
        for (size_t i = 0; i < n; ++i)
            outsizeBuf.push_back(static_cast<size_t>(args[1].elemAsDouble(i)));
    }
    outs[0] = otf2psf(args[0], Span<const size_t>(outsizeBuf.data(), outsizeBuf.size()), mr);
}

void checkerboard_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    size_t side = 10, M = 4, N = 4;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const double s = args[0].toScalar();
        if (s < 0.0 || s != std::floor(s))
            throw Error("checkerboard: SIDE must be a non-negative integer",
                        0, 0, "checkerboard", "", "numkit:checkerboard:side");
        side = static_cast<size_t>(s);
    }
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) { M = N = static_cast<size_t>(v.toScalar()); }
        else if (v.numel() >= 2) {
            M = static_cast<size_t>(v.elemAsDouble(0));
            N = static_cast<size_t>(v.elemAsDouble(1));
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        N = static_cast<size_t>(args[2].toScalar());
    outs[0] = checkerboard(side, M, N, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::image
