// bundle/src/register/io/type_reg.cpp
//
// Engine-registration adapter for `type(filename)`. The core-free compute body
// nk::io::type(FsContext&, sink, filename) lives in toolboxes/io; this glue
// unwraps the CallContext and binds the output sink to engine.outputText.

#include <nk/io/text/extras.hpp>

#include <nk/core/engine.hpp>
#include <nk/core/types.hpp>
#include <nk/value/error.hpp>
#include <nk/value/value.hpp>

#include <string>

namespace nk::io::detail {

void type_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("type: requires a filename string",
                     0, 0, "type", "", "nk:type:nargin");
    type(ctx.engine->fsContext(),
         [&ctx](const std::string &s) { ctx.engine->outputText(s); },
         args[0].toString());
    outs[0] = Value();
}

} // namespace nk::io::detail
