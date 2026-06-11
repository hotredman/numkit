// bundle/src/register/io/paths_reg.cpp
// CallContext adapters for io.paths filesep/fullfile/fileparts/tempdir/tempname.
// Compute is Engine-free in toolboxes/io/src/paths/paths.cpp; these bridge via
// engine.fsContext()/resource(). IoLibrary::install registers them by name.
#include <numkit/io/paths/paths.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

#include <string>
#include <vector>

namespace numkit::io::detail {

void filesep_reg(Span<const Value> /*args*/, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = filesep(ctx.engine->resource());
}

void fullfile_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) {
        outs[0] = Value::matrix(1, 0, ValueType::CHAR, ctx.engine->resource());
        return;
    }
    std::vector<std::string> parts;
    parts.reserve(args.size());
    for (const auto &a : args) {
        if (!a.isChar() && !a.isString())
            throw Error("fullfile: all arguments must be strings",
                         0, 0, "fullfile", "", "numkit:fullfile:badArg");
        parts.push_back(a.toString());
    }
    outs[0] = fullfile(Span<const std::string>(parts.data(), parts.size()), ctx.engine->resource());
}

void fileparts_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("fileparts: requires a string path",
                     0, 0, "fileparts", "", "numkit:fileparts:nargin");
    auto [folder, name, ext] = fileparts(args[0].toString(), ctx.engine->resource());
    outs[0] = std::move(folder);
    if (nargout > 1) outs[1] = std::move(name);
    if (nargout > 2) outs[2] = std::move(ext);
}

void tempdir_reg(Span<const Value> /*args*/, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = tempdir(ctx.engine->fsContext(), ctx.engine->resource());
}

void tempname_reg(Span<const Value> /*args*/, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    outs[0] = tempname(ctx.engine->fsContext(), ctx.engine->resource());
}

} // namespace numkit::io::detail
