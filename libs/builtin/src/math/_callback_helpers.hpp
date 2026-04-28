// libs/builtin/src/math/_callback_helpers.hpp
//
// Shared helper for builtins that take a function-handle argument and
// invoke it once per scalar input (fzero, integral, ...). Lifted out of
// the original MStdCalculus.cpp so each split TU can include it without
// linker collisions. Inline to keep the header self-contained.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin::detail::callback {

using ::numkit::m::Engine;

inline double evalCallback(Engine *engine, const MValue &fn, double x)
{
    Allocator alloc = Allocator::defaultAllocator();
    MValue arg = MValue::scalar(x, &alloc);
    Span<const MValue> args(&arg, 1);
    MValue r = engine->callFunctionHandle(fn, args);
    if (!r.isScalar() && r.numel() != 1)
        throw MError("callback: handle must return a scalar value",
                     0, 0, "callback", "", "m:callback:nonScalar");
    return r.elemAsDouble(0);
}

} // namespace numkit::m::builtin::detail::callback
