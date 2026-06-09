#pragma once
// complex.hpp moved to the math library (numkit/math/complex/complex.hpp) as
// step C1 of the builtin -> math+lang split. Forwarding stub so existing
// <numkit/builtin/math/complex/complex.hpp> includers compile unchanged; they
// retarget to <numkit/math/complex/complex.hpp> in the C4 cleanup. Functions
// are still in namespace numkit::builtin for now (ns rename is the C4 pass).
#include <numkit/math/complex/complex.hpp>
