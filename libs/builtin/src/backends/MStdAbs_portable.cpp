// libs/builtin/src/backends/MStdAbs_portable.cpp
//
// Reference scalar implementation of abs(). Compiled when
// NUMKIT_WITH_SIMD=OFF (see libs/builtin/src/CMakeLists.txt). The
// Highway-dispatched variant lives in MStdAbs_simd.cpp — both share
// exactly this behaviour for small / complex / scalar inputs; the
// SIMD backend only diverges on the real-vector fast path.

#include <numkit/m/builtin/MStdMath.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "../MStdHelpers.hpp"

#include <cmath>
#include <complex>

namespace numkit::m::builtin {

MValue abs(Allocator &alloc, const MValue &x)
{
    if (x.isComplex()) {
        if (x.isScalar())
            return MValue::scalar(std::abs(x.toComplex()), &alloc);
        auto r = createLike(x, MType::DOUBLE, &alloc);
        for (size_t i = 0; i < x.numel(); ++i)
            r.doubleDataMut()[i] = std::abs(x.complexData()[i]);
        return r;
    }
    return unaryDouble(x, [](double v) { return std::abs(v); }, &alloc);
}

} // namespace numkit::m::builtin
