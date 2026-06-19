// toolboxes/optim/src/library.cpp
//
// Registration hub for toolboxes/optim. Three currently-implemented MATLAB-base
// optimization functions live here: fzero, fminbnd, fminsearch — all
// promoted to the top-level (no namespace) so MATLAB-base UX is preserved
// without requiring `import optim.*` (same precedent as Signal's 6
// cross-domain promotions for fft / conv / xcorr / etc.).
//
// Future toolbox-level entries (lsqnonneg, fmincon, linprog, ga, ...) will
// register here too. Toolbox-grade entries may live under namespaced
// `optim.*` instead — that decision is made per function as they land.

#include <numkit/optim/library.hpp>
#include <numkit/core/engine.hpp>

#include <numkit/core/types.hpp>

namespace numkit::optim::detail {
void fminbnd_reg   (Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
void fminsearch_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx);
} // namespace numkit::optim::detail

namespace numkit::optim {
// Defined in local/fzero.cpp — install the `.m` optimizer wrappers (pausable
// objective; the C++ `Value fzero/fminsearch(...)` APIs remain the synchronous
// embedder path).
void registerFzeroM(Engine &engine);
void registerFminsearchM(Engine &engine);
void registerFsolveM(Engine &engine);       // fsolve_reg.cpp
void registerLsqnonlinM(Engine &engine);    // lsqnonlin_reg.cpp
void registerFminuncM(Engine &engine);      // fminunc_reg.cpp
void registerQuadprogM(Engine &engine);     // quadprog_reg.cpp
void registerLinprogM(Engine &engine);      // linprog_reg.cpp
} // namespace numkit::optim

namespace numkit {

void OptimLibrary::install(Engine &engine)
{
    // MATLAB-base: available top-level (no namespace, no import needed).
    // fzero is an embedded `.m` wrapper (VM_CALLBACKS_PLAN.md) so the objective
    // is called from bytecode and is pausable under the debugger.
    optim::registerFzeroM(engine);
    engine.registerFunction("fminbnd",    &optim::detail::fminbnd_reg);
    // fminsearch is an embedded `.m` wrapper (pausable objective); shadows the
    // C++ external on both backends. The `Value fminsearch(...)` API is retained.
    optim::registerFminsearchM(engine);
    // fsolve — embedded `.m` Levenberg-Marquardt nonlinear system solver
    // (pausable objective; solution-parity with MATLAB).
    optim::registerFsolveM(engine);
    // lsqnonlin / lsqcurvefit — embedded `.m` nonlinear least squares (LM;
    // pausable residual; solution-parity with MATLAB).
    optim::registerLsqnonlinM(engine);
    // fminunc — embedded `.m` BFGS unconstrained minimizer (pausable
    // objective; solution-parity with MATLAB).
    optim::registerFminuncM(engine);
    // quadprog — embedded `.m` primal active-set quadratic program
    // (strictly-convex H; solution-parity with MATLAB).
    optim::registerQuadprogM(engine);
    // linprog — embedded `.m` linear program via proximal regularization
    // over quadprog (exact vertex for unique optima).
    optim::registerLinprogM(engine);
}

} // namespace numkit
