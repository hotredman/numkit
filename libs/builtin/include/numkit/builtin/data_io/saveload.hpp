// libs/builtin/include/numkit/builtin/data_io/saveload.hpp
#pragma once

#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit {
class Engine;
class Environment;
}

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Workspace persistence — MATLAB's save / load (-ascii only).
//
// Separate from the session-state workspace builtins (clear / who /
// whos / tic / toc) registered via BuiltinLibrary::registerWorkspaceBuiltins;
// those live in-process, these read/write files. Both touch the VM's
// variable environment, so the public C++ API takes Engine& (for Vfs)
// and Environment& (for var lookup / assignment). load's no-LHS branch
// writes into Environment using a stem-derived var name; the
// `(nargout, outs)` pair lets the same entry point serve both forms.
// ════════════════════════════════════════════════════════════════════════

void save(Engine &engine, Environment &env, Span<const Value> args);
void load(Engine &engine, Environment &env, Span<const Value> args,
          size_t nargout, Span<Value> outs);

} // namespace numkit::builtin
