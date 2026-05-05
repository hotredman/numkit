// libs/control/src/lti/lti.cpp
//
// LTI constructors. Each builds a numkit struct value tagged by the
// `kind` field. Discrete-time systems carry Ts > 0; continuous = 0.
// We don't enforce algebraic invariants here (e.g. real coefficients,
// proper rational form) — that's left to the user / converters which
// check on-demand.

#include <numkit/control/lti/lti.hpp>
#include <numkit/control/conversion/conversion.hpp>

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

// ──────────────────────────────────────────────────────────────────────
// Extractors: tfdata / zpkdata / ssdata
// ──────────────────────────────────────────────────────────────────────

namespace {

const std::string &kindOf(const Value &sys) {
    static const std::string empty;
    if (!sys.isStruct() || !sys.hasField("kind")) return empty;
    static thread_local std::string s;
    s = sys.field("kind").toString();
    return s;
}

// Pad num with leading zeros to match den length (MATLAB tfdata convention).
Value padToLen(std::pmr::memory_resource *mr, const Value &row, size_t targetLen) {
    const size_t N = row.numel();
    if (N >= targetLen) return rowVec(mr, row);
    Value out = Value::matrix(1, targetLen, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const size_t pad = targetLen - N;
    for (size_t i = 0; i < pad; ++i)     od[i] = 0.0;
    for (size_t i = 0; i < N;   ++i)     od[pad + i] = row.elemAsDouble(i);
    return out;
}

// Wrap a single value in a 1×1 cell.
Value wrapCell(std::pmr::memory_resource *mr, const Value &v) {
    Value c = Value::cell(1, 1, mr);
    c.cellAt(0) = v;
    return c;
}

// Column-vector copy (used for zpkdata's z/p outputs).
Value colVec(std::pmr::memory_resource *mr, const Value &v) {
    const size_t N = v.numel();
    if (v.type() == ValueType::COMPLEX) {
        Value r = Value::matrix(N, 1, ValueType::COMPLEX, mr);
        const std::complex<double> *src = v.complexData();
        std::complex<double> *rd = r.complexDataMut();
        for (size_t i = 0; i < N; ++i) rd[i] = src[i];
        return r;
    }
    Value r = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *rd = r.doubleDataMut();
    for (size_t i = 0; i < N; ++i) rd[i] = v.elemAsDouble(i);
    return r;
}

} // anonymous

std::tuple<Value, Value>
tfdata(std::pmr::memory_resource *mr, const Value &sys, bool asVector)
{
    Value num, den;
    const std::string &k = kindOf(sys);
    if (k == "tf") {
        num = sys.field("num");
        den = sys.field("den");
    } else if (k == "zpk") {
        zp2tf(mr, sys.field("z"), sys.field("p"), sys.field("k"), &num, &den);
    } else if (k == "ss") {
        ss2tf(mr, sys.field("A"), sys.field("B"),
              sys.field("C"), sys.field("D"), 1, &num, &den);
    } else {
        throw Error("tfdata: input must be tf / zpk / ss",
                    0, 0, "tfdata", "", "m:tfdata:kind");
    }
    const size_t L = std::max(num.numel(), den.numel());
    Value np = padToLen(mr, num, L);
    Value dp = padToLen(mr, den, L);
    if (asVector) return {std::move(np), std::move(dp)};
    return {wrapCell(mr, np), wrapCell(mr, dp)};
}

std::tuple<Value, Value, Value>
zpkdata(std::pmr::memory_resource *mr, const Value &sys, bool asVector)
{
    Value z, p, k;
    const std::string &kind = kindOf(sys);
    if (kind == "zpk") {
        z = colVec(mr, sys.field("z"));
        p = colVec(mr, sys.field("p"));
        k = Value::scalar(sys.field("k").toScalar(), mr);
    } else if (kind == "tf") {
        Value zr, pr, kr;
        tf2zp(mr, sys.field("num"), sys.field("den"), &zr, &pr, &kr);
        z = colVec(mr, zr);
        p = colVec(mr, pr);
        k = Value::scalar(kr.toScalar(), mr);
    } else if (kind == "ss") {
        Value n, d, zr, pr, kr;
        ss2tf(mr, sys.field("A"), sys.field("B"),
              sys.field("C"), sys.field("D"), 1, &n, &d);
        tf2zp(mr, n, d, &zr, &pr, &kr);
        z = colVec(mr, zr);
        p = colVec(mr, pr);
        k = Value::scalar(kr.toScalar(), mr);
    } else {
        throw Error("zpkdata: input must be tf / zpk / ss",
                    0, 0, "zpkdata", "", "m:zpkdata:kind");
    }
    if (asVector) return {std::move(z), std::move(p), std::move(k)};
    return {wrapCell(mr, z), wrapCell(mr, p), std::move(k)};
}

std::tuple<Value, Value, Value, Value>
ssdata(std::pmr::memory_resource *mr, const Value &sys)
{
    const std::string &kind = kindOf(sys);
    Value A, B, C, D;
    if (kind == "ss") {
        A = sys.field("A");
        B = sys.field("B");
        C = sys.field("C");
        D = sys.field("D");
    } else if (kind == "tf") {
        tf2ss(mr, sys.field("num"), sys.field("den"), &A, &B, &C, &D);
    } else if (kind == "zpk") {
        Value n, d;
        zp2tf(mr, sys.field("z"), sys.field("p"), sys.field("k"), &n, &d);
        tf2ss(mr, n, d, &A, &B, &C, &D);
    } else {
        throw Error("ssdata: input must be tf / zpk / ss",
                    0, 0, "ssdata", "", "m:ssdata:kind");
    }
    return {std::move(A), std::move(B), std::move(C), std::move(D)};
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

static bool wantVector(Span<const Value> args, size_t pos) {
    if (args.size() <= pos) return false;
    if (!args[pos].isChar() && !args[pos].isString()) return false;
    std::string s = args[pos].toString();
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s == "v" || s == "vector";
}

void tfdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("tfdata: requires (sys [, 'v'])",
                    0, 0, "tfdata", "", "m:tfdata:nargin");
    auto [num, den] = tfdata(ctx.engine->resource(), args[0], wantVector(args, 1));
    outs[0] = std::move(num);
    if (nargout > 1) outs[1] = std::move(den);
}

void zpkdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("zpkdata: requires (sys [, 'v'])",
                    0, 0, "zpkdata", "", "m:zpkdata:nargin");
    auto [z, p, k] = zpkdata(ctx.engine->resource(), args[0], wantVector(args, 1));
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(k);
}

void ssdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("ssdata: requires (sys)",
                    0, 0, "ssdata", "", "m:ssdata:nargin");
    auto [A, B, C, D] = ssdata(ctx.engine->resource(), args[0]);
    outs[0] = std::move(A);
    if (nargout > 1) outs[1] = std::move(B);
    if (nargout > 2) outs[2] = std::move(C);
    if (nargout > 3) outs[3] = std::move(D);
}

} // namespace detail

} // namespace numkit::control
