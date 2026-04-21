// libs/builtin/include/numkit/m/builtin/MStdEnvCsv.hpp
#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m {
class Engine;
}

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Environment + CSV builtins
//
// csvread / csvwrite go through the Engine's VirtualFS (resolvePath +
// readFile / writeFile), so they take Engine&. setenv / getenv are pure
// process-env wrappers; getenv allocates its result, setenv has no
// output. Signatures are Span-based because MATLAB's contract is
// inherently variadic (range arg, offsets, optional value).
// ════════════════════════════════════════════════════════════════════════

MValue csvread(Engine &engine, Span<const MValue> args);
void csvwrite(Engine &engine, Span<const MValue> args);

void setenv(Span<const MValue> args);
MValue getenv(Allocator &alloc, Span<const MValue> args);

} // namespace numkit::m::builtin
