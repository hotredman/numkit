// libs/comm/src/modulation/generic_qam.cpp
//
// Generic constellation modulation/demodulation:
//   y = genqammod(x, constellation)   -- index -> constellation point
//   x = genqamdemod(y, constellation) -- nearest-point demodulation
//
// Default 'integer' input mode only. Bit-input mode
// (`'InputType','bit'`) is documented as deferred -- requires
// bit-grouping by log2(M) which we'll add when called for.

#include <numkit/comm/modulation/generic_qam.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <complex>
#include <limits>

namespace numkit::comm {

using Cd = std::complex<double>;

// Read constellation point k as Cd.
static Cd constAt(const Value &c, size_t k) {
    if (c.isComplex())
        return c.complexData()[k];
    return Cd(c.elemAsDouble(k), 0.0);
}

Value genqammod(const Value &x, const Value &constellation,
                std::pmr::memory_resource *mr)
{
    const size_t M = constellation.numel();
    if (M == 0)
        throw Error("genqammod: constellation must be non-empty",
                    0, 0, "genqammod", "", "m:genqammod:EmptyConst");

    const auto &d = x.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t N = H * W;

    // Output type: complex iff constellation is complex.
    const bool out_complex = constellation.isComplex();
    Value out = Value::matrix(H, W,
                              out_complex ? ValueType::COMPLEX
                                          : ValueType::DOUBLE,
                              mr);

    if (out_complex) {
        Cd *o = out.complexDataMut();
        for (size_t i = 0; i < N; ++i) {
            const double xi = x.elemAsDouble(i);
            if (xi < 0.0 || xi >= static_cast<double>(M))
                throw Error("genqammod: index out of range [0, M-1]",
                            0, 0, "genqammod", "",
                            "m:genqammod:OutOfRange");
            const size_t k = static_cast<size_t>(xi);
            o[i] = constellation.complexData()[k];
        }
    } else {
        double *o = out.doubleDataMut();
        for (size_t i = 0; i < N; ++i) {
            const double xi = x.elemAsDouble(i);
            if (xi < 0.0 || xi >= static_cast<double>(M))
                throw Error("genqammod: index out of range [0, M-1]",
                            0, 0, "genqammod", "",
                            "m:genqammod:OutOfRange");
            const size_t k = static_cast<size_t>(xi);
            o[i] = constellation.elemAsDouble(k);
        }
    }
    return out;
}

Value genqamdemod(const Value &y, const Value &constellation,
                  std::pmr::memory_resource *mr)
{
    const size_t M = constellation.numel();
    if (M == 0)
        throw Error("genqamdemod: constellation must be non-empty",
                    0, 0, "genqamdemod", "", "m:genqamdemod:EmptyConst");

    const auto &d = y.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t N = H * W;

    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    const bool y_complex = y.isComplex();

    for (size_t i = 0; i < N; ++i) {
        const Cd yi = y_complex
                          ? y.complexData()[i]
                          : Cd(y.elemAsDouble(i), 0.0);
        size_t best_k = 0;
        double best_d2 = std::numeric_limits<double>::infinity();
        for (size_t k = 0; k < M; ++k) {
            const Cd ck = constAt(constellation, k);
            const double dr = yi.real() - ck.real();
            const double di = yi.imag() - ck.imag();
            const double d2 = dr * dr + di * di;
            if (d2 < best_d2) {
                best_d2 = d2;
                best_k  = k;
            }
        }
        o[i] = static_cast<double>(best_k);
    }
    return out;
}

namespace detail {

void genqammod_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("genqammod: requires (x, constellation)",
                    0, 0, "genqammod", "", "m:genqammod:nargin");
    // Bit-input mode (name-value 'InputType','bit') -> deferred.
    if (args.size() > 2)
        throw Error("genqammod: name-value options not yet supported "
                    "(bit-input mode deferred)",
                    0, 0, "genqammod", "", "m:genqammod:NotSupported");
    outs[0] = genqammod(args[0], args[1], ctx.engine->resource());
}

void genqamdemod_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("genqamdemod: requires (y, constellation)",
                    0, 0, "genqamdemod", "", "m:genqamdemod:nargin");
    if (args.size() > 2)
        throw Error("genqamdemod: name-value options not yet supported "
                    "(bit-output mode deferred)",
                    0, 0, "genqamdemod", "", "m:genqamdemod:NotSupported");
    outs[0] = genqamdemod(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
