// bundle/src/register/io/type_reg.cpp
//
// Engine-registration adapter for `type(filename)`. The core-free compute body
// numkit::io::type(FsContext&, sink, filename) lives in toolboxes/io; this glue
// unwraps the CallContext and binds the output sink to engine.outputText.

#include <numkit/io/text/extras.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit::io::detail {

void type_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("type: requires a filename string",
                     0, 0, "type", "", "numkit:type:nargin");
    type(ctx.engine->fsContext(),
         [&ctx](const std::string &s) { ctx.engine->outputText(s); },
         args[0].toString());
    outs[0] = Value();
}

} // namespace numkit::io::detail
