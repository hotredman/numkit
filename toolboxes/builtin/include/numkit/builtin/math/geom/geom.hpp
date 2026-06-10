#pragma once
// geom.hpp relocated to the math library (numkit/math/geom/geom.hpp) and its
// compute renamed numkit::builtin -> numkit::math (Phase 3-A step C4). This
// forwarding stub re-includes the relocated header AND re-exports numkit::math
// into numkit::builtin, so existing <numkit/builtin/math/geom/geom.hpp> includers
// that reference `numkit::builtin::<geom fn>` keep compiling unchanged. Both the
// stub and the using-shim are dropped in step C4c (call-site migration).
#include <numkit/math/geom/geom.hpp>
namespace numkit::builtin { using namespace numkit::math; }
