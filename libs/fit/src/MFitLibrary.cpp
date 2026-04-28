#include <numkit/m/fit/MFitLibrary.hpp>

#include <numkit/m/core/MTypes.hpp>

namespace numkit::m::fit::detail {
// Forward declarations for adapters in MFitInterp.cpp.
void interp1_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
void interp2_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx);
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
    engine.registerFunction("spline",  &fit::detail::spline_reg);
    engine.registerFunction("pchip",   &fit::detail::pchip_reg);
    engine.registerFunction("polyfit", &fit::detail::polyfit_reg);
    engine.registerFunction("polyval", &fit::detail::polyval_reg);
    engine.registerFunction("trapz",   &fit::detail::trapz_reg);
}

} // namespace numkit::m
