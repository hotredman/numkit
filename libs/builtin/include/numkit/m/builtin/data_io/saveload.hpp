// libs/builtin/include/numkit/m/builtin/data_io/saveload.hpp
#pragma once

#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m {
class Engine;
class Environment;
}

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Workspace persistence — MATLAB's save / load (-ascii only).
//
// Separate from the session-state workspace builtins (clear / who /
// whos / tic / toc) registered via StdLibrary::registerWorkspaceBuiltins;
// those live in-process, these read/write files. Both touch the VM's
// variable environment, so the public C++ API takes Engine& (for Vfs)
// and Environment& (for var lookup / assignment). load's no-LHS branch
// writes into Environment using a stem-derived var name; the
// `(nargout, outs)` pair lets the same entry point serve both forms.
// ════════════════════════════════════════════════════════════════════════

void save(Engine &engine, Environment &env, Span<const MValue> args);
void load(Engine &engine, Environment &env, Span<const MValue> args,
          size_t nargout, Span<MValue> outs);

} // namespace numkit::m::builtin
