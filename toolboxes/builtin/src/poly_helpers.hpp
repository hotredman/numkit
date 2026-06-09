#pragma once
// poly_helpers.hpp moved to ops/ (numkit_ops) as part of step C0 of the
// builtin -> math+lang split: these engine-free (value+ops only) polynomial
// helpers are shared by math/poly AND the signal toolbox (tf2sos / butter), so
// they live below both in ops, in namespace numkit::ops. (The stale
// <numkit/core/types.hpp> include was dropped — Complex is just
// std::complex<double>.) This forwarding re-export shim keeps the existing
// numkit::builtin::detail call-sites compiling unchanged during the migration;
// they retarget to numkit::ops as their TUs relocate (C1) or in the C4 cleanup.
#include <numkit/ops/poly_helpers.hpp>

namespace numkit::builtin::detail {
using numkit::ops::Complex;
using numkit::ops::polyEvalHorner;
using numkit::ops::polyRootsDurandKerner;
using numkit::ops::polyExpandFromRoots;
} // namespace numkit::builtin::detail
