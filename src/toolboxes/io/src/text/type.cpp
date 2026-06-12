// toolboxes/io/src/text/type.cpp
//
// type(filename) — read a file via the VFS and stream it to a caller-provided
// output sink. Core-free: the Engine coupling (its output sink) is injected by
// the caller, so this compute TU includes no <numkit/core/...>. The bundle
// `type_reg` adapter binds the sink to `engine.outputText`. The VFS read goes
// through the shared slurpFile (extras.hpp).
#include <numkit/io/text/extras.hpp>

#include <functional>
#include <string>

namespace numkit::io {

// ── type ──────────────────────────────────────────────────────────────
void type(FsContext &fs,
          const std::function<void(const std::string &)> &out,
          const std::string &filename)
{
    auto content = slurpFile(fs, filename, "type");
    out(content);
    if (content.empty() || content.back() != '\n')
        out("\n");
}

} // namespace numkit::io
