// toolboxes/builtin/include/numkit/builtin/math/random/rng.hpp
//
// Transitional shim. The RNG compute (the shared MT19937 stream + the
// value-producing rand/randn/randi/randperm generators + rng() state control)
// moved to the L0.5 ops layer (<numkit/ops/rng.hpp>, numkit::ops) so toolboxes
// that need randomness share one seedable stream without depending on
// builtin/the engine. This re-exports it into numkit::builtin so existing
// callers — the rng builtins in rng.cpp and the toolbox samplers that use
// builtin::sharedEngine() / builtin::rand() — are unchanged.

#pragma once

#include <numkit/ops/rng.hpp>
#include <numkit/builtin/math/random/matlab_mt19937.hpp>  // MatlabMT19937 → builtin::detail

namespace numkit::math {

using numkit::ops::rand;
using numkit::ops::randn;
using numkit::ops::randND;
using numkit::ops::randnND;
using numkit::ops::sharedEngine;
using numkit::ops::rngMutex;
using numkit::ops::rngSeed;
using numkit::ops::rngShuffle;
using numkit::ops::rngState;
using numkit::ops::rngRestore;
using numkit::ops::randi;
using numkit::ops::randperm;

} // namespace numkit::math
