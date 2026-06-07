// libs/comm/src/source/random_source_reg.cpp
//
// Register half of the comm random-source builtins: the CallContext
// wrappers randsrc / randerr that apply MATLAB defaults (alphabet [-1 1],
// 1 error per row), parse the optional reproducibility state, and delegate
// to the engine-free compute in random_source.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/comm/source/random_source.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

namespace numkit::comm {
namespace detail {

void randsrc_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("randsrc: requires (m, n [, alphabet [, state]])",
                    0, 0, "randsrc", "", "numkit:randsrc:nargin");
    const size_t m = static_cast<size_t>(args[0].toScalar());
    const size_t n = static_cast<size_t>(args[1].toScalar());
    auto *mr = ctx.engine->resource();

    // Default alphabet = [-1, 1] if not provided.
    Value default_alphabet;
    const Value *alphabet = nullptr;
    if (args.size() >= 3 && !args[2].isEmpty()) {
        alphabet = &args[2];
    } else {
        default_alphabet = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        double *d = default_alphabet.doubleDataMut();
        d[0] = -1.0;
        d[1] =  1.0;
        alphabet = &default_alphabet;
    }

    bool have_state = false;
    uint32_t state = 0;
    if (args.size() >= 4 && !args[3].isEmpty()) {
        have_state = true;
        const double s = args[3].toScalar();
        if (s < 0.0)
            throw Error("randsrc: state must be a non-negative integer",
                        0, 0, "randsrc", "", "numkit:randsrc:InvalidState");
        state = static_cast<uint32_t>(s);
    }

    outs[0] = randsrc(m, n, *alphabet, have_state, state, mr);
}

void randerr_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("randerr: requires (m, n [, errors [, state]])",
                    0, 0, "randerr", "", "numkit:randerr:nargin");
    const size_t m = static_cast<size_t>(args[0].toScalar());
    const size_t n = static_cast<size_t>(args[1].toScalar());
    auto *mr = ctx.engine->resource();

    // Default errspec = 1 error per row.
    Value default_errspec;
    const Value *errspec = nullptr;
    if (args.size() >= 3 && !args[2].isEmpty()) {
        errspec = &args[2];
    } else {
        default_errspec = Value::scalar(1.0, mr);
        errspec = &default_errspec;
    }

    bool have_state = false;
    uint32_t state = 0;
    if (args.size() >= 4 && !args[3].isEmpty()) {
        have_state = true;
        const double s = args[3].toScalar();
        if (s < 0.0)
            throw Error("randerr: state must be a non-negative integer",
                        0, 0, "randerr", "", "numkit:randerr:InvalidState");
        state = static_cast<uint32_t>(s);
    }

    outs[0] = randerr(m, n, *errspec, have_state, state, mr);
}

} // namespace detail

} // namespace numkit::comm
