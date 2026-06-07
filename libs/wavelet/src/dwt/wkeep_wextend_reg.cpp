// libs/wavelet/src/dwt/wkeep_wextend_reg.cpp
//
// Register half of the boundary helpers wkeep / wextend: the CallContext
// builtins (argument parsing, optional OPT / side) that delegate to the
// engine-free compute in wkeep_wextend.cpp. The 1-D extension core
// detail::extend1D stays in the compute TU (it is a compute helper, not a
// register-side parser). library.cpp forward-declares + registers these.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/dwt/wkeep_wextend.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::wavelet {
namespace detail {

// y = wkeep(x, n[, OPT])             — 1-D form
// y = wkeep(x, [R C][, [fr fc]])     — 2-D form (matrix sub-extraction)
//   OPT == 'c' (default) → centred:   start = floor((N-n)/2) + 1   (1-based)
//   OPT == 'l'           → first n
//   OPT == 'r'           → last n
//   OPT numeric (FIRST)  → x(FIRST : FIRST+n-1)   (1-based start)
//
// 2-D: when args[1] has numel()==2, we extract a central [R x C] sub-matrix
// (default) or an explicit corner [fr fc] when args[2] is a 2-vector.
//
// Verified vs MATLAB R2025b:
//   wkeep(1:10, 4)            → [4 5 6 7]
//   wkeep(1:10, 4, 'l')       → [1 2 3 4]
//   wkeep(magic(5), [3 3])    → [5 7 14; 6 13 20; 12 19 21]   (central)
//   wkeep(magic(5), [3 3], [1 1]) → [17 24 1; 23 5 7; 4 6 13] (top-left)
//
// Bug fix 2026-05-08: 2-D form was throwing "Cannot convert double to scalar"
// because adapter did args[1].toScalar() unconditionally.
void wkeep_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("wkeep: requires (x, n[, OPT]) or (X, [R C][, [fr fc]])",
                    0, 0, "wkeep", "", "numkit:wkeep:nargin");
    outs[0] = wkeep(args[0], args[1],
                    args.size() >= 3 ? args[2] : Value::Empty,
                    ctx.engine->resource());
}

// y = wextend(type, mode, x, lf[, side])
//   type = 1                 — 1-D vector extension
//   type = 2                 — 2-D matrix extension (extend rows AND cols)
//   type = 'ar' (along row)  — extend cols only
//   type = 'ac' (along col)  — extend rows only
// Bug fix 2026-05-08: added 'symw' / 'asym' / 'asymw' / 'sp0' / 'sp1'
// modes and the type=2 / 'ar' / 'ac' forms.
void wextend_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("wextend: requires (type, mode, x, lf[, side])",
                    0, 0, "wextend", "", "numkit:wextend:nargin");

    if (!args[1].isChar() && !args[1].isString())
        throw Error("wextend: mode must be a character vector",
                    0, 0, "wextend", "", "numkit:wextend:mode");
    std::string side = "b";
    if (args.size() >= 5 && (args[4].isChar() || args[4].isString()))
        side = args[4].toString();
    outs[0] = wextend(args[0], args[1].toString(), args[2],
                      static_cast<long long>(args[3].toScalar()), side,
                      ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::wavelet
