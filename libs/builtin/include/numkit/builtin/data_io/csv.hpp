// libs/builtin/include/numkit/builtin/data_io/csv.hpp
#pragma once

#include <numkit/core/span.hpp>
#include <numkit/core/value.hpp>

namespace numkit {
class Engine;
}

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// CSV text I/O — both routes go through the Engine's VirtualFS
// (resolvePath + readFile / writeFile), so Engine& is required.
// Signatures are Span-based because MATLAB's csvread/csvwrite are
// inherently variadic (range arg, offsets).
// ════════════════════════════════════════════════════════════════════════

Value csvread(Engine &engine, Span<const Value> args);
void csvwrite(Engine &engine, Span<const Value> args);

} // namespace numkit::builtin
