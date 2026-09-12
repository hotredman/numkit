// toolboxes/builtin/src/language/operators/unary_ops_reg.cpp
//
// CallContext register half (Phase 2b multi-block split).
#include <nk/core/engine.hpp>
#include <nk/builtin/ops.hpp>
#include <nk/value/error.hpp>
#include <nk/value/value.hpp>
#include <nk/ops/helpers.hpp>
#include "ops/unary_ops_detail.hpp"
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/span.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nk::builtin {
using namespace nk::builtin;  // C4c localized (umbrella removed)

namespace detail {

#define NK_UNOP_REG(MATLAB_NAME, CXX_FN)                                             \
    void MATLAB_NAME##_reg(Span<const Value> args, size_t /*nargout*/,              \
                           Span<Value> outs, CallContext &ctx)                       \
    {                                                                                 \
        if (args.empty())                                                             \
            throw Error(#MATLAB_NAME ": requires 1 argument",                        \
                         0, 0, #MATLAB_NAME, "", "nk:" #MATLAB_NAME ":nargin");        \
        outs[0] = CXX_FN(args[0], ctx.engine->resource());                           \
    }

NK_UNOP_REG(uminus,     uminus)
NK_UNOP_REG(uplus,      uplus)
NK_UNOP_REG(not,        logicalNot)
NK_UNOP_REG(ctranspose, ctranspose)

#undef NK_UNOP_REG

} // namespace detail

} // namespace nk::builtin
