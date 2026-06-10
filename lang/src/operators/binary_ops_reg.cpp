// toolboxes/builtin/src/language/operators/binary_ops_reg.cpp
//
// CallContext register half (Phase 2b multi-block split).
#include <numkit/core/engine.hpp>
#include <numkit/builtin/language/operators/binary_ops.hpp>
#include <numkit/builtin/library.hpp>
#include <numkit/ops/binary_ops.hpp>
#include <numkit/ops/compare.hpp>
#include <numkit/ops/la_solve.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "binary_ops_detail.hpp"
#include "helpers.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

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

namespace numkit::builtin {
using namespace numkit::lang;  // C4c localized (umbrella removed)

namespace detail {

#define NK_BINOP_REG(MATLAB_NAME, CXX_FN)                                            \
    void MATLAB_NAME##_reg(Span<const Value> args, size_t /*nargout*/,              \
                           Span<Value> outs, CallContext &ctx)                       \
    {                                                                                 \
        if (args.size() < 2)                                                          \
            throw Error(#MATLAB_NAME ": requires 2 arguments",                       \
                         0, 0, #MATLAB_NAME, "", "numkit:" #MATLAB_NAME ":nargin");        \
        outs[0] = CXX_FN(args[0], args[1], ctx.engine->resource());                  \
    }

NK_BINOP_REG(plus,     plus)
NK_BINOP_REG(minus,    minus)
NK_BINOP_REG(times,    times)
NK_BINOP_REG(mtimes,   mtimes)
NK_BINOP_REG(rdivide,  rdivide)
NK_BINOP_REG(mrdivide, mrdivide)
NK_BINOP_REG(mldivide, mldivide)
NK_BINOP_REG(power,    elementPower)   // MATLAB power(a,b) = a.^b → C++ elementPower
NK_BINOP_REG(mpower,   power)          // MATLAB mpower(a,b) = a^b  → C++ power
NK_BINOP_REG(eq,       eq)
NK_BINOP_REG(ne,       ne)
NK_BINOP_REG(lt,       lt)
NK_BINOP_REG(le,       le)
NK_BINOP_REG(gt,       gt)
NK_BINOP_REG(ge,       ge)

#undef NK_BINOP_REG

// ldivide(a, b) = b ./ a (MATLAB convention).
void ldivide_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ldivide: requires 2 arguments",
                     0, 0, "ldivide", "", "numkit:ldivide:nargin");
    outs[0] = rdivide(args[1], args[0], ctx.engine->resource());
}

// `and` / `or` builtins map to logicalAnd / logicalOr.
void and_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("and: requires 2 arguments",
                     0, 0, "and", "", "numkit:and:nargin");
    outs[0] = logicalAnd(args[0], args[1], ctx.engine->resource());
}

void or_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
            CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("or: requires 2 arguments",
                     0, 0, "or", "", "numkit:or:nargin");
    outs[0] = logicalOr(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin
