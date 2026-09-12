// toolboxes/optim/src/_callback_helpers.hpp
//
// Re-export shim. The FnHandle Value<->double callback-eval helpers moved to ops
// (nk/ops/callback_eval.hpp) when the iterative solver kernels migrated there
// (ops/root_solve). Optim solver files keep calling
// optim::detail::callback::evalScalar / evalVecToScalar through these aliases —
// no call-site churn.

#pragma once

#include <nk/ops/callback_eval.hpp>

namespace nk::optim::detail::callback {

using nk::ops::evalScalar;
using nk::ops::evalVecToScalar;

} // namespace nk::optim::detail::callback
