// src/runtime/include/numkit/runtime/language/commands/env.hpp
#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

namespace numkit {
class Engine;
}

namespace numkit::runtime {

void registerEnvRuntime(Engine &engine);

/// @file
/// @ingroup group_iofun
/// @brief Process-environment builtins.
///
/// Thin wrappers over `_putenv_s` / `::setenv` and the cross-platform
/// `envGet` from `numkit/fs/branding.hpp`. No Engine state required;
/// `getenv` just needs a memory resource for its Value result.

/// @brief Set / unset an environment variable (`setenv(name, value)`).
///
/// Two-argument form sets `name = value`. Empty `value` removes the
/// variable from the process environment.
///
/// @param args  `(name, value)` — both CHAR / STRING.
/// @throws Error  Bad argument types or count
///                (`m:setenv:nargin` / `m:setenv:badArg`).
void setenv(Span<const Value> args);

/// @brief Read an environment variable (`val = getenv(name)`).
///
/// @param args  `(name)` — CHAR / STRING.
/// @param mr    Memory resource (nullptr -> process default).
/// @return      Value of the variable as a CHAR row, or empty `''` if
///              the variable is unset.
/// @throws Error  Bad argument type or count
///                (`m:getenv:nargin` / `m:getenv:badArg`).
Value getenv(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::runtime
