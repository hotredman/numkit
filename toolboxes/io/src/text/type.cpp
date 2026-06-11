// toolboxes/io/src/text/type.cpp
//
// type(filename) prints a file's content to the engine output sink, so it
// legitimately keeps Engine& (needs the engine for output). Kept in its own
// TU so extras.cpp (the VFS text readers) stays core-free; the VFS read goes
// through the shared slurpFile (extras.hpp).
#include <numkit/io/text/extras.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

namespace numkit::io {

// ── type ──────────────────────────────────────────────────────────────
void type(Engine &engine, const std::string &filename)
{
    auto content = slurpFile(engine.fsContext(), filename, "type");
    engine.outputText(content);
    if (content.empty() || content.back() != '\n')
        engine.outputText("\n");
}

namespace detail {

void type_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("type: requires a filename string",
                     0, 0, "type", "", "numkit:type:nargin");
    type(*ctx.engine, args[0].toString());
    outs[0] = Value();
}

} // namespace detail
} // namespace numkit::io
