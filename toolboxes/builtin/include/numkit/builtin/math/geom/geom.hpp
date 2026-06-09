#pragma once
// geom.hpp moved to the math library (numkit/math/geom/geom.hpp) as step C1 of
// the builtin -> math+lang split. This forwarding stub keeps the existing
// <numkit/builtin/math/geom/geom.hpp> includers (other toolboxes + tests)
// compiling unchanged during the migration; they retarget to
// <numkit/math/geom/geom.hpp> in the C4 cleanup. The functions are still in
// namespace numkit::builtin for now (the ns rename to numkit::math is the
// separate C4 pass), so no namespace shim is needed here.
#include <numkit/math/geom/geom.hpp>
