// libs/builtin/src/math/_callback_helpers.hpp
//
// Shared helper for builtins that take a function-handle argument and
// invoke it once per scalar input (fzero, integral, ...). Lifted out of
// the original MStdCalculus.cpp so each split TU can include it without
// linker collisions. Inline to keep the header self-contained.

#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

namespace numkit::builtin::detail::callback {

using ::numkit::Engine;

inline double evalCallback(Engine *engine, const Value &fn, double x)
{
    Allocator alloc = Allocator::defaultAllocator();
    Value arg = Value::scalar(x, &alloc);
    Span<const Value> args(&arg, 1);
    Value r = engine->callFunctionHandle(fn, args);
    if (!r.isScalar() && r.numel() != 1)
        throw Error("callback: handle must return a scalar value",
                     0, 0, "callback", "", "m:callback:nonScalar");
    return r.elemAsDouble(0);
}

} // namespace numkit::builtin::detail::callback
