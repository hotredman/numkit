#pragma once
// types.hpp moved to the lang library (numkit/lang/types/types.hpp) — Phase 3-A
// step C2. Forwarding stub; ns still numkit::builtin (rename is C4).
#include <numkit/lang/types/types.hpp>
namespace numkit::builtin { using namespace numkit::lang; }  // C4 re-export shim (dropped in C4c)
