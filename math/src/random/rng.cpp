// toolboxes/builtin/src/math/random/rng.cpp
// RNG builtins (the CallContext wrappers): rand / randn / randi / randperm /
// rng. These parse MATLAB shape/type arguments and delegate to the engine-free
// RNG compute in numkit::ops (ops/rng.cpp) — included via rng.hpp, which
// re-exports the ops:: generators into numkit::builtin so the unqualified calls
// below resolve.

#include <numkit/builtin/math/random/rng.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"

#include <cstdint>
#include <mutex>
#include <random>
#include <string>

namespace numkit::math {

} // namespace numkit::math
