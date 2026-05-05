// libs/optim/src/_callback_helpers.hpp
//
// Local copy of the function-handle eval helper used by fzero / fminbnd /
// fminsearch. Mirrors libs/builtin/src/math/_callback_helpers.hpp under
// a different namespace so libs/optim does not pull libs/builtin/src/.

#pragma once

#include <memory_resource>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

namespace numkit::optim::detail::callback {

using ::numkit::Engine;

inline double evalCallback(Engine *engine, const Value &fn, double x)
{
    std::pmr::memory_resource *mr = std::pmr::get_default_resource();
    Value arg = Value::scalar(x, mr);
    Span<const Value> args(&arg, 1);
    Value r = engine->callFunctionHandle(fn, args);
    if (!r.isScalar() && r.numel() != 1)
        throw Error("callback: handle must return a scalar value",
                     0, 0, "callback", "", "m:callback:nonScalar");
    return r.elemAsDouble(0);
}

} // namespace numkit::optim::detail::callback
