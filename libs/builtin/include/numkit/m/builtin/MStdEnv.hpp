// libs/builtin/include/numkit/m/builtin/MStdEnv.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Process-environment builtins — thin wrappers over _putenv_s / ::setenv
// and MBranding's cross-platform envGet. No Engine needed; getenv just
// needs an Allocator for its MValue result.
// ════════════════════════════════════════════════════════════════════════

void setenv(Span<const MValue> args);
MValue getenv(Allocator &alloc, Span<const MValue> args);

} // namespace numkit::m::builtin
