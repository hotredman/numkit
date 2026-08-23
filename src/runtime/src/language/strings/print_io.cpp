// runtime/src/language/strings/print_io.cpp
//
// Engine-coupled string-output builtins — disp / fprintf. Relocated from
// lang/src/strings/print.cpp in C6c-2b: they route through the engine text
// sink (outputText) and the engine fid table (findFile / OpenFile), so they
// belong in the core-aware runtime layer, not in the core-free lang compute
// layer (a dependency previously masked by the builtin/library.hpp umbrella).
// The pure renderer (dispFormat) and the format engine (formatCyclic) stay in
// lang and are called here through their public headers. Namespace stays
// numkit::lang, matching the cell/struct precedent: runtime-located,
// language-namespaced builtins.

#include <numkit/builtin/strfun.hpp>  // formatCyclic
#include <numkit/builtin/strfun.hpp>   // dispFormat + disp/fprintf decls
#include <numkit/core/engine.hpp>          // Engine, findFile, OpenFile

#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

#include <cstddef>
#include <cstring>
#include <string>

namespace numkit::builtin {

void disp(Engine &engine, Span<const Value> args)
{
    for (const auto &a : args)
        engine.outputText(dispFormat(a));
}

std::size_t fprintf(Engine &engine, Span<const Value> args)
{
    if (args.empty())
        return 0;
    std::pmr::memory_resource *mr = engine.resource();

    // First-arg-is-fid disambiguation: MATLAB allows both
    //   fprintf(format, …)  and  fprintf(fid, format, …)
    // We detect the latter via "leading numeric scalar followed by char".
    int fid = 1;
    size_t fmtIdx = 0;
    if (args.size() >= 2 && args[0].isScalar() && args[1].isChar()) {
        fid = static_cast<int>(args[0].toScalar());
        fmtIdx = 1;
    }
    if (!args[fmtIdx].isChar())
        return 0;

    std::string result = formatCyclic(args[fmtIdx].toString(), args, fmtIdx + 1, mr);

    if (fid == 1 || fid == 2) {
        engine.outputText(result);
    } else if (fid >= 3) {
        auto *f = engine.findFile(fid);
        if (!f || !f->forWrite)
            throw Error("fprintf: invalid file identifier");
        // For 'a'/'a+' (appendOnly) snap to end first — MATLAB's
        // contract regardless of prior seek.
        size_t writePos = f->appendOnly ? f->buffer.size() : f->cursor;
        if (writePos + result.size() > f->buffer.size())
            f->buffer.resize(writePos + result.size());
        std::memcpy(f->buffer.data() + writePos, result.data(), result.size());
        f->cursor = writePos + result.size();
    } else {
        throw Error("fprintf: invalid file identifier");
    }
    return result.size();
}

} // namespace numkit::builtin
