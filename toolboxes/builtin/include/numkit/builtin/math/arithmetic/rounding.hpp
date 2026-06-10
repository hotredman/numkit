#pragma once
// rounding.hpp (public API) moved to the math library
// (numkit/math/arithmetic/rounding.hpp) — Phase 3-A step C1. Forwarding stub; ns
// still numkit::builtin (rename is C4). (Distinct from the src-internal
// rounding.hpp that travels inside math/src/arithmetic/.)
#include <numkit/math/arithmetic/rounding.hpp>
namespace numkit::builtin { using namespace numkit::math; }  // C4 re-export shim (dropped in C4c)
