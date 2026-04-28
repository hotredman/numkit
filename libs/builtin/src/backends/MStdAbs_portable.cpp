// libs/builtin/src/backends/MStdAbs_portable.cpp
//
// Reference scalar implementation of abs(). Compiled when
// NUMKIT_WITH_SIMD=OFF (see libs/builtin/src/CMakeLists.txt). The
// Highway-dispatched variant lives in MStdAbs_simd.cpp — both share
// exactly this behaviour for small / complex / scalar inputs; the
// SIMD backend only diverges on the real-vector fast path.

#include <numkit/m/builtin/math/elementary/rounding.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "../MStdHelpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::m::builtin {

MValue abs(Allocator &alloc, const MValue &x, MValue *hint)
{
    if (x.isComplex()) {
        if (x.isScalar())
            return MValue::scalar(std::abs(x.toComplex()), &alloc);
        auto r = createLike(x, MType::DOUBLE, &alloc);
        for (size_t i = 0; i < x.numel(); ++i)
            r.doubleDataMut()[i] = std::abs(x.complexData()[i]);
        return r;
    }
    if (x.isScalar())
        return MValue::scalar(std::fabs(x.toScalar()), &alloc);
    // Output-reuse fast path: caller-provided hint is a heap double
    // of matching shape with unique ownership — write straight into
    // its buffer instead of allocating a fresh one.
    if (hint && hint->isHeapDouble() && hint->heapRefCount() == 1
        && hint->dims() == x.dims()) {
        MValue r = std::move(*hint);
        const double *in  = x.doubleData();
        double       *out = r.doubleDataMut();
        for (size_t i = 0; i < x.numel(); ++i)
            out[i] = std::fabs(in[i]);
        return r;
    }
    return unaryDouble(x, [](double v) { return std::abs(v); }, &alloc);
}

} // namespace numkit::m::builtin
