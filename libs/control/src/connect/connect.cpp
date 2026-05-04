// libs/control/src/connect/connect.cpp
//
// LTI interconnections in transfer-function arithmetic. Each entry
// point first converts the inputs to (num, den) coefficient rows
// via the existing libs/builtin (zp2tf) and libs/control (ss2tf
// helper) primitives, then performs the polynomial arithmetic, and
// finally hands a `tf` struct back to the caller.
//
// Polynomial multiplication uses signal::conv. Polynomial addition
// is done locally (right-aligned, longer length wins).

#include <numkit/control/connect/connect.hpp>
#include <numkit/control/lti/lti.hpp>
#include <numkit/control/conversion/conversion.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>
#include <numkit/signal/convolution/convolution.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::control {

namespace {

bool hasKind(const Value &sys, const char *want) {
    if (!sys.isStruct() || !sys.hasField("kind")) return false;
    return sys.field("kind").toString() == want;
}

double sampleTime(const Value &sys) {
    if (sys.isStruct() && sys.hasField("Ts"))
        return sys.field("Ts").toScalar();
    return 0.0;
}

std::vector<double> coeffsReal(const Value &v) {
    std::vector<double> out(v.numel());
    for (size_t i = 0; i < v.numel(); ++i) out[i] = v.elemAsDouble(i);
    return out;
}

Value rowOf(std::pmr::memory_resource *mr, const std::vector<double> &v) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

// Bring an LTI struct (or a raw 2-arg call: (num, den) split across two
// args via `series(num1, den1, num2, den2)` — not yet supported) into
// a (num, den) coefficient pair.
struct NumDen { std::vector<double> num, den; };

NumDen toNumDen(std::pmr::memory_resource *mr, const Value &sys) {
    if (hasKind(sys, "tf")) {
        return {coeffsReal(sys.field("num")), coeffsReal(sys.field("den"))};
    }
    if (hasKind(sys, "zpk")) {
        Value num, den;
        zp2tf(mr, sys.field("z"), sys.field("p"), sys.field("k"),
              &num, &den);
        return {coeffsReal(num), coeffsReal(den)};
    }
    if (hasKind(sys, "ss")) {
        Value num, den;
        ss2tf(mr, sys.field("A"), sys.field("B"),
              sys.field("C"), sys.field("D"), /*iu=*/1, &num, &den);
        return {coeffsReal(num), coeffsReal(den)};
    }
    throw Error("control connect: expected tf/zpk/ss struct",
                0, 0, "control", "", "m:control:kind");
}

// Polynomial addition right-aligned (highest order first, MATLAB style).
std::vector<double> polyAdd(const std::vector<double> &a,
                            const std::vector<double> &b,
                            double bScale = 1.0)
{
    const size_t na = a.size();
    const size_t nb = b.size();
    const size_t n = std::max(na, nb);
    std::vector<double> y(n, 0.0);
    // Right-align: a[na-1] aligns with y[n-1], same for b.
    for (size_t i = 0; i < na; ++i) y[n - na + i] += a[i];
    for (size_t i = 0; i < nb; ++i) y[n - nb + i] += bScale * b[i];
    return y;
}

// Polynomial multiplication via signal::conv.
std::vector<double> polyMul(std::pmr::memory_resource *mr,
                            const std::vector<double> &a,
                            const std::vector<double> &b)
{
    if (a.empty() || b.empty()) return {};
    Value av = rowOf(mr, a);
    Value bv = rowOf(mr, b);
    Value y = signal::conv(mr, av, bv, "full");
    return coeffsReal(y);
}

double Ts_combine(double Ts1, double Ts2, const char *op) {
    // MATLAB rules: continuous + continuous → continuous; same Ts →
    // same Ts; mismatched Ts → error.
    if (Ts1 == 0.0 && Ts2 == 0.0) return 0.0;
    if (Ts1 == Ts2) return Ts1;
    // Allow mixing only when one side is "unspecified" (Ts == -1).
    if (Ts1 == -1.0) return Ts2;
    if (Ts2 == -1.0) return Ts1;
    throw Error(std::string("control ") + op +
                ": sample times must match (Ts mismatch)",
                0, 0, op, "", "m:control:Ts");
}

} // anonymous

Value series(std::pmr::memory_resource *mr,
             const Value &sys1, const Value &sys2)
{
    auto a = toNumDen(mr, sys1);
    auto b = toNumDen(mr, sys2);
    auto num = polyMul(mr, a.num, b.num);
    auto den = polyMul(mr, a.den, b.den);
    const double Ts = Ts_combine(sampleTime(sys1), sampleTime(sys2),
                                 "series");
    return tf(mr, rowOf(mr, num), rowOf(mr, den), Ts);
}

Value parallel(std::pmr::memory_resource *mr,
               const Value &sys1, const Value &sys2)
{
    auto a = toNumDen(mr, sys1);
    auto b = toNumDen(mr, sys2);
    auto t1 = polyMul(mr, a.num, b.den); // n1·d2
    auto t2 = polyMul(mr, b.num, a.den); // n2·d1
    auto num = polyAdd(t1, t2);
    auto den = polyMul(mr, a.den, b.den);
    const double Ts = Ts_combine(sampleTime(sys1), sampleTime(sys2),
                                 "parallel");
    return tf(mr, rowOf(mr, num), rowOf(mr, den), Ts);
}

Value feedback(std::pmr::memory_resource *mr,
               const Value &G, const Value &H, int sign)
{
    auto g = toNumDen(mr, G);
    auto h = toNumDen(mr, H);
    // T(s) = num_G · den_H / (den_G · den_H − sign · num_G · num_H)
    auto numT = polyMul(mr, g.num, h.den);
    auto loop = polyMul(mr, g.num, h.num);
    auto denBase = polyMul(mr, g.den, h.den);
    // sign convention: MATLAB feedback default is -1 (negative
    // feedback), giving denBase + loop. sign = +1 ⇒ denBase − loop.
    auto denT = polyAdd(denBase, loop, -static_cast<double>(sign));
    const double Ts = Ts_combine(sampleTime(G), sampleTime(H), "feedback");
    return tf(mr, rowOf(mr, numT), rowOf(mr, denT), Ts);
}

namespace detail {

void series_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
    if (a.size() < 2)
        throw Error("series: requires (sys1, sys2)",
                    0, 0, "series", "", "m:series:nargin");
    o[0] = series(c.engine->resource(), a[0], a[1]);
}

void parallel_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
    if (a.size() < 2)
        throw Error("parallel: requires (sys1, sys2)",
                    0, 0, "parallel", "", "m:parallel:nargin");
    o[0] = parallel(c.engine->resource(), a[0], a[1]);
}

void feedback_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c) {
    if (a.size() < 2)
        throw Error("feedback: requires (G, H [, sign])",
                    0, 0, "feedback", "", "m:feedback:nargin");
    int sign = -1;
    if (a.size() >= 3 && !a[2].isEmpty())
        sign = static_cast<int>(a[2].toScalar());
    o[0] = feedback(c.engine->resource(), a[0], a[1], sign);
}

} // namespace detail

} // namespace numkit::control
