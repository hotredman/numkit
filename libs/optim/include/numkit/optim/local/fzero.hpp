// libs/optim/include/numkit/optim/local/fzero.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit { class Engine; }

namespace numkit::optim {

using ::numkit::Engine;

/// fzero(fn, x0)   — scalar root near x0. Expands an initial bracket
///                    around x0 until sign change is found, then runs
///                    Brent's method.
/// fzero(fn, [a, b]) — root inside the interval [a, b]. Throws if
///                    sign(fn(a)) == sign(fn(b)) (no obvious root).
/// `fn` must be a function handle. Engine pointer is required to invoke
/// the callback — it's expected to come from the CallContext.
Value fzero(std::pmr::memory_resource *mr, const Value &fn, const Value &x0OrInterval,
             Engine *engine);

/// fminbnd(fn, lo, hi[, tol]) — bounded scalar minimum on [lo, hi]
/// using Brent's golden-section / parabolic-interpolation hybrid.
Value fminbnd(std::pmr::memory_resource *mr, const Value &fn,
              double lo, double hi, double tol, Engine *engine);

/// fminsearch(fn, x0[, tol]) — multi-dimensional minimum starting at
/// x0 (vector), Nelder-Mead simplex. Returns the minimizer vector.
Value fminsearch(std::pmr::memory_resource *mr, const Value &fn,
                 const Value &x0, double tol, Engine *engine);

} // namespace numkit::optim
