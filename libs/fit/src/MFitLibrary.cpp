#include <numkit/m/fit/MFitLibrary.hpp>

#include <numkit/m/core/MTypes.hpp>

namespace numkit::m::fit::detail {
// Forward declarations for adapters in MFitInterp.cpp.
void interp1_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void interp2_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void interp3_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void spline_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void pchip_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void polyfit_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void polyval_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void trapz_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
} // namespace numkit::m::fit::detail

namespace numkit::m {

void FitLibrary::install(Engine &engine)
{
    engine.registerFunction("interp1", &fit::detail::interp1_reg);
    engine.registerFunction("interp2", &fit::detail::interp2_reg);
    engine.registerFunction("interp3", &fit::detail::interp3_reg);
    // interpn dispatches to interp2 / interp3 based on dim count.
    engine.registerFunction("interpn",
        [](Span<const MValue> args, size_t nargout,
           Span<MValue> outs, CallContext &ctx) {
            // Heuristic: V is always followed by N coordinate vectors.
            // Form A: interpn(V, Xq1..XqN[, method]) → V is args[0],
            //         then N query arrays. Output dim = ndims(V).
            // Form B: interpn(X1..XN, V, Xq1..XqN[, method]) → 2N+1
            //         core args ± method.
            if (args.empty())
                throw numkit::m::MError(
                    "interpn: requires at least 2 arguments",
                    0, 0, "interpn", "", "m:interpn:nargin");
            const auto &V0 = args[0];
            const int ndV = V0.dims().is3D() ? 3
                          : (V0.dims().ndim() <= 2 ? 2 : V0.dims().ndim());
            if (ndV == 2) {
                fit::detail::interp2_reg(args, nargout, outs, ctx);
                return;
            }
            if (ndV == 3) {
                fit::detail::interp3_reg(args, nargout, outs, ctx);
                return;
            }
            throw numkit::m::MError(
                "interpn: 4+-D inputs are not yet supported",
                0, 0, "interpn", "", "m:interpn:rank");
        });
    engine.registerFunction("spline",  &fit::detail::spline_reg);
    engine.registerFunction("pchip",   &fit::detail::pchip_reg);
    engine.registerFunction("polyfit", &fit::detail::polyfit_reg);
    engine.registerFunction("polyval", &fit::detail::polyval_reg);
    engine.registerFunction("trapz",   &fit::detail::trapz_reg);
}

} // namespace numkit::m
