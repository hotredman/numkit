#pragma once
// discrete.hpp moved to the math library (numkit/math/discrete/discrete.hpp) as
// step C1 of the builtin -> math+lang split. Forwarding stub so existing
// <numkit/builtin/math/discrete/discrete.hpp> includers compile unchanged; they
// retarget to <numkit/math/discrete/discrete.hpp> in the C4 cleanup. Functions
// are still in namespace numkit::builtin for now (ns rename is the C4 pass).
#include <numkit/math/discrete/discrete.hpp>
namespace numkit::builtin { using namespace numkit::math; }  // C4 re-export shim (dropped in C4c)
