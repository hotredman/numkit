// toolboxes/wavelet/src/dwt/dyad_reg.cpp
//
// Register half of the dyadic resampling helpers: the CallContext builtins
// dyaddown / dyadup / wmaxlev (lax positional arg parsing) that delegate to
// the engine-free compute in dyad.cpp. parseDyadArgs lives here because it
// is a register-side parser (the compute entries take pre-parsed odd/type).
// library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/dwt/dyad.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <cctype>
#include <string>

namespace numkit::wavelet {

namespace {

// Parse trailing args of dyaddown / dyadup — accepts (X, evenodd[, type])
// in any positional order (MATLAB's lax parsing). `type` is 'c' / 'r' / 'm'.
void parseDyadArgs(Span<const Value> args, int defaultOdd, int &odd, char &type)
{
    odd = defaultOdd;
    type = 'c';   // matrix default; harmless on the vector path (ignored).
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            const std::string s = args[i].toString();
            if (s.empty()) continue;
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(s[0])));
            if (c != 'c' && c != 'r' && c != 'm')
                throw Error("dyad: type must be 'c', 'r', or 'm'",
                             0, 0, "dyad", "", "numkit:dyad:type");
            type = c;
        } else {
            odd = (static_cast<int>(args[i].toScalar()) != 0) ? 1 : 0;
        }
    }
}

} // anonymous

namespace detail {

// y = dyaddown(x[, ODD][, type])
//   ODD = 0 (default): keep EVEN-indexed (1-based)  → x(2:2:end)
//   ODD = 1:           keep ODD-indexed             → x(1:2:end)
//   type 'c' (matrix default): downsample columns
//   type 'r':                  downsample rows
//   type 'm':                  downsample both
// Vector input: type is ignored.
void dyaddown_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dyaddown: requires an input vector",
                    0, 0, "dyaddown", "", "numkit:dyaddown:nargin");
    int odd; char type;
    parseDyadArgs(args, /*defaultOdd=*/0, odd, type);
    outs[0] = dyaddown(args[0], odd, type, ctx.engine->resource());
}

// y = dyadup(x[, ODD][, type])
//   Vector default ODD = 1 (leading-zero pattern):
//     ODD = 0:           y = [x(1) 0 x(2) 0 … x(N)]      length 2N-1
//     ODD = 1 (default): y = [0 x(1) 0 x(2) 0 … x(N) 0]  length 2N+1
//   Matrix default ODD = 1, type = 'c'.
//
// Verified vs MATLAB R2025b: dyadup([1 2 3], 0) → [1 0 2 0 3];
//   dyadup([1 2 3], 1) → [0 1 0 2 0 3 0]; dyadup([1 2 3]) → same as ODD=1.
void dyadup_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dyadup: requires an input vector",
                    0, 0, "dyadup", "", "numkit:dyadup:nargin");
    int odd; char type;
    parseDyadArgs(args, /*defaultOdd=*/1, odd, type);
    outs[0] = dyadup(args[0], odd, type, ctx.engine->resource());
}

// L = wmaxlev(N, wname)
//   N can be a scalar or a 2-vector [r c]; in the latter MATLAB uses
//   min(r, c) for a 2-D image. wname identifies the wavelet family
//   (filter length is taken from toolboxes/wavelet/filter/wfilters.cpp).
//
//   L = floor(log2(N / (Lf - 1)))
//
// Verified vs MATLAB R2025b:
//   wmaxlev(64, 'db2')     → 4   (Lf = 4 → log2(64/3) ≈ 4.41 → 4)
//   wmaxlev([8 8], 'db1')  → 3   (Lf = 2 → log2(8/1) = 3)
void wmaxlev_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("wmaxlev: requires (N, wname)",
                    0, 0, "wmaxlev", "", "numkit:wmaxlev:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("wmaxlev: wname must be a character vector",
                    0, 0, "wmaxlev", "", "numkit:wmaxlev:type");
    outs[0] = wmaxlev(args[0], args[1].toString(), ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
