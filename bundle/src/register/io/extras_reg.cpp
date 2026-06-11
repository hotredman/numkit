// bundle/src/register/io/extras_reg.cpp
// CallContext adapters for io.text fileread/readlines/writelines/readmatrix/
// writematrix. Compute is Engine-free in toolboxes/io/src/text/extras.cpp
// (FsContext&); type lives in its own Engine-coupled TU (type.cpp).
#include <numkit/io/text/extras.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

namespace numkit::io::detail {

void fileread_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("fileread: requires a filename string",
                     0, 0, "fileread", "", "numkit:fileread:nargin");
    outs[0] = fileread(ctx.engine->fsContext(), args[0].toString(), ctx.engine->resource());
}

void readlines_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("readlines: requires a filename string",
                     0, 0, "readlines", "", "numkit:readlines:nargin");
    outs[0] = readlines(ctx.engine->fsContext(), args[0].toString(), ctx.engine->resource());
}

void writelines_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("writelines: requires (lines, filename)",
                     0, 0, "writelines", "", "numkit:writelines:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("writelines: filename must be a string",
                     0, 0, "writelines", "", "numkit:writelines:badFilename");
    writelines(ctx.engine->fsContext(), args[0], args[1].toString());
    outs[0] = Value();
}

void readmatrix_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("readmatrix: requires a filename string",
                     0, 0, "readmatrix", "", "numkit:readmatrix:nargin");
    outs[0] = readmatrix(ctx.engine->fsContext(), args[0].toString(), ctx.engine->resource());
}

void writematrix_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("writematrix: requires (M, filename)",
                     0, 0, "writematrix", "", "numkit:writematrix:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("writematrix: filename must be a string",
                     0, 0, "writematrix", "", "numkit:writematrix:badFilename");
    writematrix(ctx.engine->fsContext(), args[0], args[1].toString());
    outs[0] = Value();
}

} // namespace numkit::io::detail
