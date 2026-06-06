// libs/builtin/include/numkit/builtin/math/random/matlab_mt19937.hpp
//
// Transitional shim. MatlabMT19937 (the MATLAB-canonical MT19937 engine) moved
// to the L0.5 ops layer (<numkit/ops/matlab_mt19937.hpp>, numkit::ops) so the
// toolboxes can use the shared RNG without pulling in builtin/the engine. This
// re-exports it into numkit::builtin::detail so existing callers are unchanged.

#pragma once

#include <numkit/ops/matlab_mt19937.hpp>

namespace numkit::builtin::detail {
using numkit::ops::MatlabMT19937;
} // namespace numkit::builtin::detail
