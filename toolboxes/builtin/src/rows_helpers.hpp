#pragma once
// rows_helpers.hpp moved to ops/ (numkit_ops) as part of step C0 of the
// builtin -> math+lang split: these engine-free (value-only) row-tuple helpers
// are shared by BOTH math/discrete and language/arrays, so they live below both
// in ops, in namespace numkit::ops. This forwarding shim re-exports them into
// numkit::builtin::detail so the existing in-tree `detail::rowLexCmp` /
// `collectRowsByIndex` call-sites compile unchanged during the migration; they
// retarget to numkit::ops as their TUs relocate (C1/C2) or in the C4 cleanup.
// Mirrors reduction_helpers.hpp.
#include <numkit/ops/rows_helpers.hpp>

namespace numkit::builtin::detail {
using numkit::ops::rowLexCmp;
using numkit::ops::rowLexCmpByCols;
using numkit::ops::collectRowsByIndex;
} // namespace numkit::builtin::detail
