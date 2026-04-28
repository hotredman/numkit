// libs/builtin/include/numkit/builtin/lang/commands/env.hpp
#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Process-environment builtins — thin wrappers over _putenv_s / ::setenv
// and the cross-platform envGet from numkit/core/branding.hpp. No Engine
// needed; getenv just needs an Allocator for its Value result.
// ════════════════════════════════════════════════════════════════════════

void setenv(Span<const Value> args);
Value getenv(Allocator &alloc, Span<const Value> args);

} // namespace numkit::builtin
