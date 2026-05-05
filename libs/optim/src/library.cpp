// libs/optim/src/library.cpp
//
// Registration hub for libs/optim. Three currently-implemented MATLAB-base
// optimization functions live here: fzero, fminbnd, fminsearch — all
// promoted to the top-level (no namespace) so MATLAB-base UX is preserved
// without requiring `import optim.*` (same precedent as Signal's 6
// cross-domain promotions for fft / conv / xcorr / etc.).
//
// Future toolbox-level entries (lsqnonneg, fmincon, linprog, ga, ...) will
// register here too. Toolbox-grade entries may live under namespaced
// `optim.*` instead — that decision is made per function as they land.

#include <numkit/optim/library.hpp>

#include <numkit/core/types.hpp>

namespace numkit::optim::detail {
void fzero_reg     (Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void fminbnd_reg   (Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void fminsearch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
} // namespace numkit::optim::detail

namespace numkit {

void OptimLibrary::install(Engine &engine)
{
    // MATLAB-base: available top-level (no namespace, no import needed).
    engine.registerFunction("fzero",      &optim::detail::fzero_reg);
    engine.registerFunction("fminbnd",    &optim::detail::fminbnd_reg);
    engine.registerFunction("fminsearch", &optim::detail::fminsearch_reg);
}

} // namespace numkit
