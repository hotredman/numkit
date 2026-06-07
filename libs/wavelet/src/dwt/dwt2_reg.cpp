// libs/wavelet/src/dwt/dwt2_reg.cpp
//
// Register half of the 2-D DWT pair: the CallContext builtins dwt2 / idwt2
// (argument parsing, optional `sx` output-size handling, multi-output
// packing) that delegate to the engine-free compute in dwt2.cpp. Keeping
// this separate lets the compute build against value+fs+ops alone.
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/dwt/dwt2.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <string>
#include <utility>

namespace numkit::wavelet {
namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected string argument",
                    0, 0, "", "", "numkit:wavelet:type");
    return v.toString();
}

void dwt2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dwt2: requires (X, wname)",
                    0, 0, "dwt2", "", "numkit:dwt2:nargin");
    auto *mr = ctx.engine->resource();
    auto r = dwt2(args[0], argString(args[1]), mr);
    if (outs.size() >= 1) outs[0] = std::move(r.cA);
    if (outs.size() >= 2) outs[1] = std::move(r.cH);
    if (outs.size() >= 3) outs[2] = std::move(r.cV);
    if (outs.size() >= 4) outs[3] = std::move(r.cD);
}

void idwt2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.size() < 5)
        throw Error("idwt2: requires (cA, cH, cV, cD, wname [, sx])",
                    0, 0, "idwt2", "", "numkit:idwt2:nargin");
    long long outRows = -1, outCols = -1;
    if (args.size() >= 6 && !args[5].isEmpty()) {
        // sx form: a length-2 vector [rows cols], or a scalar (square).
        if (args[5].numel() == 1) {
            outRows = outCols = static_cast<long long>(args[5].toScalar());
        } else if (args[5].numel() >= 2) {
            outRows = static_cast<long long>(args[5].elemAsDouble(0));
            outCols = static_cast<long long>(args[5].elemAsDouble(1));
        }
    }
    outs[0] = idwt2(args[0], args[1], args[2], args[3],
                    argString(args[4]), outRows, outCols,
                    ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
