// libs/builtin/src/backends/abs_portable.cpp
//
// Reference scalar implementation of abs(). Compiled when
// NUMKIT_WITH_SIMD=OFF (see libs/builtin/src/CMakeLists.txt). The
// Highway-dispatched variant lives in abs_simd.cpp — both share
// exactly this behaviour for small / complex / scalar inputs; the
// SIMD backend only diverges on the real-vector fast path.

#include <numkit/builtin/math/elementary/rounding.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "../helpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::builtin {

Value abs(Allocator &alloc, const Value &x, Value *hint)
{
    if (x.isComplex()) {
        if (x.isScalar())
            return Value::scalar(std::abs(x.toComplex()), &alloc);
        auto r = createLike(x, ValueType::DOUBLE, &alloc);
        for (size_t i = 0; i < x.numel(); ++i)
            r.doubleDataMut()[i] = std::abs(x.complexData()[i]);
        return r;
    }
    if (x.isScalar())
        return Value::scalar(std::fabs(x.toScalar()), &alloc);
    // Output-reuse fast path: caller-provided hint is a heap double
    // of matching shape with unique ownership — write straight into
    // its buffer instead of allocating a fresh one.
    if (hint && hint->isHeapDouble() && hint->heapRefCount() == 1
        && hint->dims() == x.dims()) {
        Value r = std::move(*hint);
        const double *in  = x.doubleData();
        double       *out = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            out[i] = std::fabs(in[i]);
        return r;
    }
    return unaryDouble(x, [](double v) { return std::abs(v); }, &alloc);
}

} // namespace numkit::builtin
