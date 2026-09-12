// bundle/src/register/io/csv_reg.cpp
// CallContext adapters for io.text csvread/csvwrite. Compute is Engine-free
// in toolboxes/io/src/text/csv.cpp (FsContext& + mr); these bridge via
// engine.fsContext()/resource(). IoLibrary::install registers them by name.
#include <nk/io/text/csv.hpp>
#include <nk/core/engine.hpp>
#include <nk/core/types.hpp>
#include <nk/value/value.hpp>

namespace nk::io::detail {

void csvread_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
    outs[0] = csvread(ctx.engine->fsContext(), args, ctx.engine->resource());
}

void csvwrite_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
    (void)outs;
    csvwrite(ctx.engine->fsContext(), args);
}


} // namespace nk::io::detail
