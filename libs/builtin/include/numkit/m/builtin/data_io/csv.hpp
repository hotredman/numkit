// libs/builtin/include/numkit/m/builtin/data_io/csv.hpp
#pragma once

#include <numkit/m/core/MSpan.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m {
class Engine;
}

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// CSV text I/O — both routes go through the Engine's VirtualFS
// (resolvePath + readFile / writeFile), so Engine& is required.
// Signatures are Span-based because MATLAB's csvread/csvwrite are
// inherently variadic (range arg, offsets).
// ════════════════════════════════════════════════════════════════════════

MValue csvread(Engine &engine, Span<const MValue> args);
void csvwrite(Engine &engine, Span<const MValue> args);

} // namespace numkit::m::builtin
