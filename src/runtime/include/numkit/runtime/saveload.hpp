// runtime/include/numkit/runtime/saveload.hpp
#pragma once

#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

namespace numkit {
class Engine;
class Environment;
}

namespace numkit::runtime {

/// @file
/// @brief Workspace persistence — `save` / `load` (ASCII + matio v5 .mat).
///
/// Companion to the session-state workspace runtime builtins (`clear` /
/// `who` / `whos` / `clearvars`) registered alongside these via
/// `runtime::registerWorkspaceRuntime`; those live in-process, these
/// read/write files. Both touch the VM's variable environment, so the
/// public C++ API takes `Engine&` (for VFS) and `Environment&` (for var
/// lookup / assignment).
///
/// `load`'s no-LHS branch writes into `Environment` using a
/// stem-derived var name; the `(nargout, outs)` pair lets the same
/// entry point serve both forms.

/// @brief Save workspace variables to an ASCII file
/// (`save filename var1 var2 ...`).
///
/// Looks each requested variable up in `env`, formats it as ASCII,
/// and writes the file through `engine`'s VFS. With no variables
/// specified, saves the entire workspace.
///
/// @param engine  Engine context (VFS).
/// @param env     Variable environment to read from.
/// @param args    `(filename [, var1, var2, …])` — strings or symbol
///                names.
/// @throws Error  On VFS failure or unknown variable name.
/// @see load
void save(Engine &engine, Environment &env, Span<const Value> args);

/// @brief Load workspace variables from an ASCII file
/// (`load filename` / `S = load(filename)`).
///
/// Reads the file through `engine`'s VFS, parses ASCII matrices, and
/// either:
/// - (no-LHS form) writes each parsed variable into `env` using a
///   stem-derived var name, or
/// - (LHS form) returns a struct with one field per variable.
///
/// @param engine   Engine context (VFS).
/// @param env      Variable environment to assign into.
/// @param args     `(filename [, var1, var2, …])`.
/// @param nargout  Number of requested outputs (0 = assign into
///                 `env`, 1 = return struct).
/// @param outs     Output slot for the struct form.
/// @throws Error   On VFS failure, parse failure, or unknown variable.
/// @see save
void load(Engine &engine, Environment &env, Span<const Value> args,
          size_t nargout, Span<Value> outs);

} // namespace numkit::runtime
