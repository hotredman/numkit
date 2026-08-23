// toolboxes/signal/src/filter_design/analog_filters_reg.cpp
//
// CallContext register half of filter_design/analog_filters.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/polyfun.hpp>
#include <numkit/signal/filter_design/analog_filters.hpp>
#include <numkit/signal/filter_implementation/conversions_extras.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

#define NK_PROTO0_REG(name)                                                     \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires N",                                    \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int N = static_cast<int>(args[0].toScalar());                     \
        auto [z, p, k] = name(N, ctx.engine->resource());                       \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

#define NK_PROTO1_REG(name)                                                     \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#name ": requires (N, ripple_or_atten_dB)",             \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int N = static_cast<int>(args[0].toScalar());                     \
        const double r = args[1].toScalar();                                    \
        auto [z, p, k] = name(N, r, ctx.engine->resource());                    \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_PROTO0_REG(buttap)
NK_PROTO0_REG(besselap)
NK_PROTO1_REG(cheb1ap)
NK_PROTO1_REG(cheb2ap)

// ellipap takes 3 args (N, Rp, Rs).
void ellipap_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ellipap: requires (N, Rp, Rs)",
                     0, 0, "ellipap", "", "numkit:ellipap:nargin");
    const int N     = static_cast<int>(args[0].toScalar());
    const double Rp = args[1].toScalar();
    const double Rs = args[2].toScalar();
    auto [z, p, k] = ellipap(N, Rp, Rs, ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(k);
}

#undef NK_PROTO0_REG
#undef NK_PROTO1_REG

// MATLAB's lp2* TF form returns the numerator at its true polynomial degree
// (length = #zeros + 1), NOT zero-padded to the denominator length the way
// zp2tf does. e.g. lp2lp of a Butterworth prototype (no zeros) returns
// bt = [Wo^N] (length 1), while zp2tf would give [0 … 0 Wo^N]. Strip the
// leading exact-zeros that zp2tf inserted so the TF form matches MATLAB.
// (No-op for lp2hp/lp2bs, whose numerators are already full length.)
static Value lp2StripLeadingZeros(const Value &b, std::pmr::memory_resource *mr)
{
    const size_t n = b.numel();
    size_t s = 0;
    while (s + 1 < n && b.elemAsDouble(s) == 0.0) ++s;
    if (s == 0) return b;
    auto r = Value::matrix(1, n - s, ValueType::DOUBLE, mr);
    double *d = r.doubleDataMut();
    for (size_t i = s; i < n; ++i) d[i - s] = b.elemAsDouble(i);
    return r;
}

// MATLAB lp2lp/lp2hp accept BOTH:
//   [zt, pt, kt] = lp2lp(z, p, k, Wo)   -- ZPK form (4 args)
//   [bt, at]     = lp2lp(b, a, Wo)      -- TF form  (3 args)
// Same for lp2hp. The TF form is identified by the 3-arg call and
// dispatches via tf2zpk -> ZPK lp2lp -> zp2tf.
#define NK_LP2X1_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        auto *mr = ctx.engine->resource();                                       \
        if (args.size() == 3) {                                                  \
            /* TF form: (b, a, Wo) -> (bt, at). */                               \
            const double Wo = args[2].toScalar();                                \
            auto [z0, p0, k0] = tf2zpk(args[0], args[1], mr);                    \
            auto [zt, pt, kt] = fn(z0, p0, k0, Wo, mr);                          \
            auto [bt, at] = numkit::builtin::zp2tf(zt, pt, kt.toScalar(), mr);           \
            bt = lp2StripLeadingZeros(bt, mr);                                   \
            outs[0] = std::move(bt);                                             \
            if (nargout > 1) outs[1] = std::move(at);                            \
            return;                                                              \
        }                                                                        \
        if (args.size() < 4)                                                     \
            throw Error(#name ": requires (z, p, k, Wo) or (b, a, Wo)",         \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const double Wo = args[3].toScalar();                                   \
        auto [z, p, k] = fn(args[0], args[1], args[2].toScalar(), Wo, mr);      \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_LP2X1_REG(lp2lp, lp2lp)
NK_LP2X1_REG(lp2hp, lp2hp)

#undef NK_LP2X1_REG

// MATLAB lp2bp/lp2bs accept BOTH:
//   [zt, pt, kt] = lp2bp(z, p, k, Wo, Bw)   -- ZPK form (5 args)
//   [bt, at]     = lp2bp(b, a, Wo, Bw)      -- TF form  (4 args)
#define NK_LP2X2_REG(name, fn)                                                  \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        auto *mr = ctx.engine->resource();                                       \
        if (args.size() == 4) {                                                  \
            /* TF form: (b, a, Wo, Bw) -> (bt, at). */                           \
            const double Wo = args[2].toScalar();                                \
            const double Bw = args[3].toScalar();                                \
            auto [z0, p0, k0] = tf2zpk(args[0], args[1], mr);                    \
            auto [zt, pt, kt] = fn(z0, p0, k0, Wo, Bw, mr);                      \
            auto [bt, at] = numkit::builtin::zp2tf(zt, pt, kt.toScalar(), mr);           \
            bt = lp2StripLeadingZeros(bt, mr);                                   \
            outs[0] = std::move(bt);                                             \
            if (nargout > 1) outs[1] = std::move(at);                            \
            return;                                                              \
        }                                                                        \
        if (args.size() < 5)                                                     \
            throw Error(#name ": requires (z, p, k, Wo, Bw) or (b, a, Wo, Bw)", \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const double Wo = args[3].toScalar();                                   \
        const double Bw = args[4].toScalar();                                   \
        auto [z, p, k] = fn(args[0], args[1], args[2].toScalar(), Wo, Bw, mr);  \
        outs[0] = std::move(z);                                                  \
        if (nargout > 1) outs[1] = std::move(p);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_LP2X2_REG(lp2bp, lp2bp)
NK_LP2X2_REG(lp2bs, lp2bs)

#undef NK_LP2X2_REG

void bilinear_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bilinear: requires (b, a, fs[, fp])",
                     0, 0, "bilinear", "", "numkit:bilinear:nargin");
    const double fs = args[2].toScalar();
    const double fp = (args.size() >= 4 && !args[3].isEmpty()) ? args[3].toScalar() : 0.0;
    auto [bd, ad] = bilinear(args[0], args[1], fs, fp, ctx.engine->resource());
    outs[0] = std::move(bd);
    if (nargout > 1) outs[1] = std::move(ad);
}

void impinvar_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("impinvar: requires (b, a, fs[, tol])",
                     0, 0, "impinvar", "", "numkit:impinvar:nargin");
    const double fs  = args[2].toScalar();
    const double tol = (args.size() >= 4 && !args[3].isEmpty()) ? args[3].toScalar() : 1e-3;
    auto [bd, ad] = impinvar(args[0], args[1], fs, tol, ctx.engine->resource());
    outs[0] = std::move(bd);
    if (nargout > 1) outs[1] = std::move(ad);
}

void freqs_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("freqs: requires (b, a, w)",
                     0, 0, "freqs", "", "numkit:freqs:nargin");
    outs[0] = freqs(args[0], args[1], args[2], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
