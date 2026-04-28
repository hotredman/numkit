// libs/builtin/include/numkit/m/builtin/math/optim/fzero.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m { class Engine; }

namespace numkit::m::builtin {

using ::numkit::m::Engine;

/// fzero(fn, x0)   — scalar root near x0. Expands an initial bracket
///                    around x0 until sign change is found, then runs
///                    Brent's method.
/// fzero(fn, [a, b]) — root inside the interval [a, b]. Throws if
///                    sign(fn(a)) == sign(fn(b)) (no obvious root).
/// `fn` must be a function handle. Engine pointer is required to invoke
/// the callback — it's expected to come from the CallContext.
MValue fzero(Allocator &alloc, const MValue &fn, const MValue &x0OrInterval,
             Engine *engine);

} // namespace numkit::m::builtin
