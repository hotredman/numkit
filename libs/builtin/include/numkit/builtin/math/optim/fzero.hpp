// libs/builtin/include/numkit/builtin/math/optim/fzero.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit { class Engine; }

namespace numkit::builtin {

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

} // namespace numkit::builtin
