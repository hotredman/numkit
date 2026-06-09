#pragma once
// env.hpp moved to the runtime layer (numkit/runtime/language/commands/env.hpp)
// as Phase 3-A step C3 — env's compute pulls core/engine, so it is core-coupled
// and belongs in runtime, not the lang library. Forwarding stub; ns still
// numkit::builtin (rename is C4).
#include <numkit/runtime/language/commands/env.hpp>
