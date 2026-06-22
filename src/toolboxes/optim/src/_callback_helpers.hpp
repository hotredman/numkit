// toolboxes/optim/src/_callback_helpers.hpp
//
// Re-export shim. The FnHandle Value<->double callback-eval helpers moved to ops
// (numkit/ops/callback_eval.hpp) when the iterative solver kernels migrated there
// (ops/root_solve). Optim solver files keep calling
// optim::detail::callback::evalScalar / evalVecToScalar through these aliases —
// no call-site churn.

#pragma once

#include <numkit/ops/callback_eval.hpp>

namespace numkit::optim::detail::callback {

using numkit::ops::evalScalar;
using numkit::ops::evalVecToScalar;

} // namespace numkit::optim::detail::callback
