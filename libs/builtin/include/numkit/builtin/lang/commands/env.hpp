// libs/builtin/include/numkit/builtin/lang/commands/env.hpp
#pragma once

#include <memory_resource>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Process-environment builtins — thin wrappers over _putenv_s / ::setenv
// and the cross-platform envGet from numkit/core/branding.hpp. No Engine
// needed; getenv just needs a memory_resource for its Value result.
// ════════════════════════════════════════════════════════════════════════

void setenv(Span<const Value> args);
Value getenv(std::pmr::memory_resource *mr, Span<const Value> args);

} // namespace numkit::builtin
