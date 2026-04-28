// libs/builtin/src/MStdPoly.cpp

#include <numkit/m/builtin/MStdPoly.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"
#include "MStdPolyHelpers.hpp"

#include <cmath>
#include <vector>

namespace numkit::m::builtin {

MValue roots(Allocator &alloc, const MValue &p)
{
    if (p.type() == MType::COMPLEX)
        throw MError("roots: complex coefficient input is not supported",
                     0, 0, "roots", "", "m:roots:complex");
    if (!p.dims().isVector() && !p.isScalar() && !p.isEmpty())
        throw MError("roots: argument must be a vector",
                     0, 0, "roots", "", "m:roots:notVector");

    // Read coefficients as DOUBLE (promote integer/single/logical).
    const std::size_t n = p.numel();
    std::vector<double> coeffs(n);
    for (std::size_t i = 0; i < n; ++i)
        coeffs[i] = p.elemAsDouble(i);

    auto rs = detail::polyRootsDurandKerner(coeffs);
    const std::size_t k = rs.size();

    // If every root is real, return a real column. Otherwise return COMPLEX.
    bool anyComplex = false;
    for (const auto &r : rs)
        if (std::abs(r.imag()) > 1e-12 * (std::abs(r.real()) + 1.0)) {
            anyComplex = true;
            break;
        }

    if (!anyComplex) {
        auto out = MValue::matrix(k, 1, MType::DOUBLE, &alloc);
        for (std::size_t i = 0; i < k; ++i)
            out.doubleDataMut()[i] = rs[i].real();
        return out;
    }
    auto out = MValue::complexMatrix(k, 1, &alloc);
    for (std::size_t i = 0; i < k; ++i)
        out.complexDataMut()[i] = rs[i];
    return out;
}

namespace detail {

void roots_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("roots: requires 1 argument",
                     0, 0, "roots", "", "m:roots:nargin");
    outs[0] = roots(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::m::builtin
