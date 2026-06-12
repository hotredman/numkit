// toolboxes/builtin/include/numkit/builtin/math/random/rng.hpp
//
// Transitional shim. The RNG compute (the shared MT19937 stream + the
// value-producing rand/randn/randi/randperm generators + rng() state control)
// moved to the L0.5 ops layer (<numkit/ops/rng.hpp>, numkit::ops) so toolboxes
// that need randomness share one seedable stream without depending on
// builtin/the engine. This re-exports it into numkit::builtin so existing
// callers — the rng builtins in rng.cpp and the toolbox samplers that use
// numkit::math::rand/randn/... — now take a RngContext (the Engine owns one).

#pragma once

#include <numkit/ops/rng.hpp>
#include <numkit/ops/rng_context.hpp>
#include <numkit/ops/matlab_mt19937.hpp>  // MatlabMT19937 (ops layer)

namespace numkit::math {

// RNG is now session-scoped: the Engine owns one RngContext (engine.rng()) and
// every generator draws from it — no process-global stream, no mutex. The rng()
// control surface (seed / shuffle / state / restore) lives on RngContext.
using numkit::ops::RngContext;
using numkit::ops::rand;
using numkit::ops::randn;
using numkit::ops::randND;
using numkit::ops::randnND;
using numkit::ops::randi;
using numkit::ops::randperm;

} // namespace numkit::math
