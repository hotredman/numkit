// toolboxes/builtin/src/reduction_helpers.hpp
//
// Transitional shim. The reduce-along-dim infrastructure moved to the L0.5
// ops layer (<numkit/ops/reductions.hpp>, namespace numkit::ops) so the
// toolboxes can use it without pulling in builtin/the engine. This header
// re-exports it into numkit::builtin::detail so the existing builtin call
// sites (and the toolbox files that still #include "reduction_helpers.hpp")
// are unchanged. Toolboxes going pure switch to <numkit/ops/reductions.hpp>
// directly, after which this shim can be removed.

#pragma once

#include <numkit/ops/reductions.hpp>

namespace numkit::builtin::detail {

using numkit::ops::firstNonSingletonDim;
using numkit::ops::validateDim;
using numkit::ops::outShapeForDim;
using numkit::ops::sliceLenForDim;
using numkit::ops::sliceCountForDim;
using numkit::ops::forEachSlice;
using numkit::ops::outShapeForDimND;
using numkit::ops::forEachSliceND;
using numkit::ops::applyAlongDim;
using numkit::ops::applyAlongDimWithIndex;
using numkit::ops::resolveDim;
using numkit::ops::compactNonNan;

} // namespace numkit::builtin::detail
