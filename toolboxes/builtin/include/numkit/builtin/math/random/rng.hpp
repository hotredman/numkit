#pragma once
// rng.hpp moved to the math library (numkit/math/random/rng.hpp) — Phase 3-A
// step C1. Forwarding stub; ns still numkit::builtin (rename is C4). (The
// matlab_mt19937.hpp shim stays here — it re-exports the ops MT19937 engine and
// is included by comm/image as well as rng.hpp.)
#include <numkit/math/random/rng.hpp>
namespace numkit::builtin { using namespace numkit::math; }  // C4 re-export shim (dropped in C4c)
