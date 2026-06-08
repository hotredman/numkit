// toolboxes/signal/src/lda/lda_reg.cpp
//
// CallContext register half of lda/lda.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/lda/lda.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

void classify_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("classify: requires (sample, training, group[, type])",
                    0, 0, "classify", "", "numkit:classify:nargin");
    std::string type;
    if (args.size() >= 4 && (args[3].isChar() || args[3].isString()))
        type = args[3].toString();
    auto [c, err, post, logp] = classify(args[0], args[1], args[2], type, ctx.engine->resource());
    outs[0] = std::move(c);
    if (nargout > 1) outs[1] = std::move(err);
    if (nargout > 2) outs[2] = std::move(post);
    if (nargout > 3) outs[3] = std::move(logp);
}

} // namespace detail

} // namespace numkit::stats
