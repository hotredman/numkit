// bundle/src/register/io/fileio_reg.cpp
//
// Engine adapters for the io.file_io builtins. The compute (fopen/fclose/
// fgetl/fgets/feof/ferror/ftell/fseek/frewind/fread/fwrite) is Engine-free in
// toolboxes/io/src/file_io/fileio.cpp — it takes FsContext& (the file-handle
// table, moved out of Engine in B1c) + a memory_resource. These CallContext
// adapters bridge the registration ABI, supplying engine.fsContext() +
// engine.resource(). IoLibrary::install (toolboxes/io/src/library.cpp)
// forward-declares + registers these by name; the definitions resolve at link.
#include <numkit/io/file_io/fileio.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

namespace numkit::io::detail {

#define NK_FILEIO_REG(FN)                                                                          \
    void FN##_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)      \
    {                                                                                              \
        FN(ctx.engine->fsContext(), args, nargout, outs, ctx.engine->resource());                  \
    }

NK_FILEIO_REG(fopen)
NK_FILEIO_REG(fclose)
NK_FILEIO_REG(fgetl)
NK_FILEIO_REG(fgets)
NK_FILEIO_REG(feof)
NK_FILEIO_REG(ferror)
NK_FILEIO_REG(ftell)
NK_FILEIO_REG(fseek)
NK_FILEIO_REG(frewind)
NK_FILEIO_REG(fread)
NK_FILEIO_REG(fwrite)

#undef NK_FILEIO_REG

} // namespace numkit::io::detail
