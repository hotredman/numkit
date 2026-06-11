// toolboxes/comm/src/channel/channel_reg.cpp
//
// Register half of the comm channel/metric builtins: the CallContext
// wrappers awgn / wgn / bsc / qfunc / qfuncinv / marcumq / berawgn /
// noisebw / berconfint / convertSNR that parse args and delegate to the
// engine-free compute in channel.cpp. library.cpp forward-declares +
// registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/channel/channel.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>
#include <utility>

namespace numkit::comm {
namespace detail {

void awgn_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("awgn: requires (signal, snr_dB[, sigpower_dB])",
                    0, 0, "awgn", "", "numkit:awgn:nargin");
    const double snr = args[1].toScalar();
    double sp = -1e10;  // sentinel = "measured"
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (args[2].isChar() || args[2].isString()) {
            // "measured" or absent → keep sentinel.
        } else {
            sp = args[2].toScalar();
        }
    }
    outs[0] = awgn(args[0], snr, sp, ctx.engine->resource());
}

void wgn_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("wgn: requires (m, n, p[, type, complexity])",
                    0, 0, "wgn", "", "numkit:wgn:nargin");
    const int m = (int)args[0].toScalar();
    const int n = (int)args[1].toScalar();
    const double p = args[2].toScalar();
    std::string type = "dBW";
    bool complex_out = false;
    for (size_t i = 3; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto s = args[i].toString();
            if      (s == "dBW" || s == "dBm" || s == "linear") type = s;
            else if (s == "complex") complex_out = true;
            else if (s == "real")    complex_out = false;
        }
    }
    outs[0] = wgn(m, n, p, type, complex_out, ctx.engine->resource());
}

void bsc_reg(Span<const Value> args, size_t /*nargout*/,
             Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bsc: requires (input, p)", 0, 0, "bsc", "",
                    "numkit:bsc:nargin");
    outs[0] = bsc(args[0], args[1].toScalar(), ctx.engine->resource());
}

void qfunc_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("qfunc: requires x", 0, 0, "qfunc", "", "numkit:qfunc:nargin");
    outs[0] = qfunc(args[0], ctx.engine->resource());
}

void qfuncinv_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("qfuncinv: requires p", 0, 0, "qfuncinv", "",
                    "numkit:qfuncinv:nargin");
    outs[0] = qfuncinv(args[0], ctx.engine->resource());
}

void marcumq_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("marcumq: requires (a, b[, m])", 0, 0, "marcumq", "",
                    "numkit:marcumq:nargin");
    const int m = (args.size() >= 3 && !args[2].isEmpty())
                  ? (int)args[2].toScalar() : 1;
    outs[0] = marcumq(args[0], args[1], m, ctx.engine->resource());
}

void berawgn_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("berawgn: requires (EbNo_dB, mod, M)", 0, 0, "berawgn", "",
                    "numkit:berawgn:nargin");
    std::string mod = args[1].toString();
    const int M = (int)args[2].toScalar();
    outs[0] = berawgn(args[0], mod, M, ctx.engine->resource());
}

void noisebw_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("noisebw: requires (num, den, Nsamp, fs)", 0, 0, "noisebw",
                    "", "numkit:noisebw:nargin");
    const int    n  = (int)args[2].toScalar();
    const double fs = args[3].toScalar();
    outs[0] = noisebw(args[0], args[1], n, fs, ctx.engine->resource());
}

void berconfint_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("berconfint: requires (numErrs, numBits[, level])",
                    0, 0, "berconfint", "", "numkit:berconfint:nargin");
    const double k     = args[0].toScalar();
    const double n     = args[1].toScalar();
    const double level = (args.size() >= 3 && !args[2].isEmpty())
                         ? args[2].toScalar() : 0.95;
    auto [ber, ci] = berconfint(k, n, level, ctx.engine->resource());
    outs[0] = std::move(ber);
    if (nargout > 1) outs[1] = std::move(ci);
}

void convertSNR_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("convertSNR: requires snr_dB[, options]", 0, 0,
                    "convertSNR", "", "numkit:convertSNR:nargin");
    std::string in_type = "ebno", out_type = "esno";
    int k = 1;
    // Parse keyword pairs.
    for (size_t i = 1; i + 1 < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const auto key = args[i].toString();
            if (key == "BitsPerSymbol") k = (int)args[i + 1].toScalar();
            else if (key == "From")     in_type = args[i + 1].toString();
            else if (key == "To")       out_type = args[i + 1].toString();
        }
    }
    outs[0] = convertSNR(args[0], in_type, out_type, k, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::comm
