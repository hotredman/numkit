// libs/builtin/include/numkit/builtin/internal/la_solve.hpp
//
// Transitional shim. The linear-solve kernel moved to the L0.5 ops layer
// (<numkit/ops/la_solve.hpp>, numkit::ops) — its proper home, below both
// builtin and linalg. This re-exports it into numkit::builtin::detail so the
// existing callers (libs/linalg: inv / linsolve / pageinv / eig …, and the
// builtin `\` operator) are unchanged. New code includes the ops header and
// calls numkit::ops::la_solve directly.

#pragma once

#include <numkit/ops/la_solve.hpp>

namespace numkit::builtin::detail {
using numkit::ops::la_solve;
} // namespace numkit::builtin::detail
