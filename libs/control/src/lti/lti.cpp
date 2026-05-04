// libs/control/src/lti/lti.cpp
//
// LTI constructors. Each builds a numkit struct value tagged by the
// `kind` field. Discrete-time systems carry Ts > 0; continuous = 0.
// We don't enforce algebraic invariants here (e.g. real coefficients,
// proper rational form) — that's left to the user / converters which
// check on-demand.

#include <numkit/control/lti/lti.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

namespace numkit::control {

namespace {

Value rowVec(std::pmr::memory_resource *mr, const Value &v) {
    // Always represent coefficient lists as a row vector, copying
    // through the user memory resource so the struct fields don't
    // alias caller-side scratch memory.
    const size_t N = v.numel();
    if (v.type() == ValueType::COMPLEX) {
        Value r = Value::matrix(1, N, ValueType::COMPLEX, mr);
        const std::complex<double> *src = v.complexData();
        std::complex<double> *rd = r.complexDataMut();
        for (size_t i = 0; i < N; ++i) rd[i] = src[i];
        return r;
    }
    Value r = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *rd = r.doubleDataMut();
    for (size_t i = 0; i < N; ++i) rd[i] = v.elemAsDouble(i);
    return r;
}

Value tagStruct(std::pmr::memory_resource *mr, const char *kind, double Ts) {
    Value s = Value::structure(mr);
    s.field("kind") = Value::fromString(kind, mr);
    s.field("Ts") = Value::scalar(Ts, mr);
    return s;
}

} // anonymous

Value tf(std::pmr::memory_resource *mr,
         const Value &num, const Value &den, double Ts)
{
    if (den.numel() == 0)
        throw Error("tf: denominator must not be empty",
                    0, 0, "tf", "", "m:tf:den");
    Value s = tagStruct(mr, "tf", Ts);
    s.field("num") = rowVec(mr, num);
    s.field("den") = rowVec(mr, den);
    return s;
}

Value zpk(std::pmr::memory_resource *mr,
          const Value &z, const Value &p, const Value &k, double Ts)
{
    Value s = tagStruct(mr, "zpk", Ts);
    s.field("z") = rowVec(mr, z);
    s.field("p") = rowVec(mr, p);
    // gain is a scalar
    s.field("k") = Value::scalar(k.toScalar(), mr);
    return s;
}

Value ss(std::pmr::memory_resource *mr,
         const Value &A, const Value &B,
         const Value &C, const Value &D, double Ts)
{
    Value s = tagStruct(mr, "ss", Ts);
    // Copy state-space matrices through the user resource. We don't
    // dimensional-check here (zero-state, no inputs, etc. are valid).
    auto copyMat = [&](const Value &m) {
        const size_t r = m.dims().rows();
        const size_t c = m.dims().cols();
        if (m.type() == ValueType::COMPLEX) {
            Value out = Value::matrix(r, c, ValueType::COMPLEX, mr);
            const std::complex<double> *src = m.complexData();
            std::complex<double> *od = out.complexDataMut();
            for (size_t i = 0; i < r * c; ++i) od[i] = src[i];
            return out;
        }
        Value out = Value::matrix(r, c, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t i = 0; i < r * c; ++i) od[i] = m.elemAsDouble(i);
        return out;
    };
    s.field("A") = copyMat(A);
    s.field("B") = copyMat(B);
    s.field("C") = copyMat(C);
    s.field("D") = copyMat(D);
    return s;
}

namespace detail {

static double argTs(Span<const Value> args, size_t pos) {
    if (args.size() <= pos || args[pos].isEmpty()) return 0.0;
    return args[pos].toScalar();
}

void tf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
            CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf: requires (num, den [, Ts])",
                    0, 0, "tf", "", "m:tf:nargin");
    outs[0] = tf(ctx.engine->resource(), args[0], args[1], argTs(args, 2));
}

void zpk_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("zpk: requires (z, p, k [, Ts])",
                    0, 0, "zpk", "", "m:zpk:nargin");
    outs[0] = zpk(ctx.engine->resource(),
                  args[0], args[1], args[2], argTs(args, 3));
}

void ss_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
            CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss: requires (A, B, C, D [, Ts])",
                    0, 0, "ss", "", "m:ss:nargin");
    outs[0] = ss(ctx.engine->resource(),
                 args[0], args[1], args[2], args[3], argTs(args, 4));
}

} // namespace detail

} // namespace numkit::control
