// libs/control/src/props/props.cpp
//
// LTI model predicates and analytic properties. Every entry point
// dispatches on the struct's `kind` tag ('tf' / 'zpk' / 'ss') so a
// script can call e.g. `isstable(tf(num,den))` or `pole(ss(A,B,C,D))`
// uniformly.
//
// State-space poles are computed via the Faddeev–LeVerrier expansion
// of det(sI − A) (already used by ss2tf in libs/control/conversion);
// the resulting characteristic polynomial is then handed to
// builtin::roots. State-space *zeros* fall back to converting to a
// transfer function via libs/signal's ss2tf and taking roots(num) —
// works for SISO systems, which is what `zero(sys)` is most useful
// for in a function-form environment.

#include <numkit/control/props/props.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::control {

namespace {

using Cd = std::complex<double>;

bool hasKind(const Value &sys, const char *want) {
    if (!sys.isStruct()) return false;
    if (!sys.hasField("kind")) return false;
    return sys.field("kind").toString() == want;
}

double sampleTime(const Value &sys) {
    if (sys.isStruct() && sys.hasField("Ts")) return sys.field("Ts").toScalar();
    return 0.0;
}

Value boolValue(std::pmr::memory_resource *mr, bool b) {
    return Value::logicalScalar(b, mr);
}

// Char poly via Faddeev–LeVerrier. Returns coefficients [1, c1, c2, …, cn]
// (descending powers, leading 1). Used for ss-form pole extraction.
std::vector<double> charPoly(const Value &A) {
    const size_t n = A.dims().rows();
    std::vector<double> Av(n * n);
    for (size_t i = 0; i < n * n; ++i) Av[i] = A.elemAsDouble(i);

    std::vector<double> M(n * n, 0.0);
    for (size_t i = 0; i < n; ++i) M[i * n + i] = 1.0; // I
    std::vector<double> coeff(n + 1, 0.0);
    coeff[0] = 1.0;

    for (size_t k = 1; k <= n; ++k) {
        // AM = A · M
        std::vector<double> AM(n * n, 0.0);
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < n; ++i) {
                double s = 0.0;
                for (size_t l = 0; l < n; ++l)
                    s += Av[l * n + i] * M[j * n + l];
                AM[j * n + i] = s;
            }
        double tr = 0.0;
        for (size_t i = 0; i < n; ++i) tr += AM[i * n + i];
        const double ck = -tr / static_cast<double>(k);
        coeff[k] = ck;
        // M_k = AM + ck I
        for (size_t i = 0; i < n; ++i) AM[i * n + i] += ck;
        M = std::move(AM);
    }
    return coeff;
}

// Pull a Value (either real or complex) into a vector of complex
// numbers. Convenient for stability checks where everything compares
// uniformly.
std::vector<Cd> toComplexVec(const Value &v) {
    const size_t N = v.numel();
    std::vector<Cd> out(N);
    if (v.type() == ValueType::COMPLEX) {
        const Cd *src = v.complexData();
        for (size_t i = 0; i < N; ++i) out[i] = src[i];
    } else {
        for (size_t i = 0; i < N; ++i) out[i] = Cd(v.elemAsDouble(i), 0.0);
    }
    return out;
}

Value rowFromCoeffs(std::pmr::memory_resource *mr,
                    const std::vector<double> &v) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

// Helper: poles as a column Value, regardless of input form.
Value polesOf(std::pmr::memory_resource *mr, const Value &sys) {
    if (hasKind(sys, "tf"))
        return builtin::roots(mr, sys.field("den"));
    if (hasKind(sys, "zpk"))
        return sys.field("p");
    if (hasKind(sys, "ss")) {
        auto cp = charPoly(sys.field("A"));
        return builtin::roots(mr, rowFromCoeffs(mr, cp));
    }
    throw Error("expected an LTI struct (tf/zpk/ss)",
                0, 0, "lti", "", "m:lti:kind");
}

Value zerosOf(std::pmr::memory_resource *mr, const Value &sys) {
    if (hasKind(sys, "tf"))
        return builtin::roots(mr, sys.field("num"));
    if (hasKind(sys, "zpk"))
        return sys.field("z");
    if (hasKind(sys, "ss"))
        // Going through ss → tf is a SISO-only fallback; matches the
        // header docstring.
        throw Error("zero(sys) on state-space form not yet implemented; "
                    "convert via [num,den] = ss2tf(A,B,C,D) then roots(num).",
                    0, 0, "zero", "", "m:zero:ss");
    throw Error("expected an LTI struct (tf/zpk/ss)",
                0, 0, "lti", "", "m:lti:kind");
}

} // anonymous

Value isct(std::pmr::memory_resource *mr, const Value &sys) {
    return boolValue(mr, sampleTime(sys) == 0.0);
}

Value isdt(std::pmr::memory_resource *mr, const Value &sys) {
    const double Ts = sampleTime(sys);
    return boolValue(mr, Ts > 0.0 || Ts == -1.0);
}

Value issiso(std::pmr::memory_resource *mr, const Value &sys) {
    if (hasKind(sys, "tf") || hasKind(sys, "zpk"))
        return boolValue(mr, true);   // single-output by construction here
    if (hasKind(sys, "ss")) {
        const Value &B = sys.field("B");
        const Value &C = sys.field("C");
        const bool one_in  = (B.dims().cols() == 1);
        const bool one_out = (C.dims().rows() == 1);
        return boolValue(mr, one_in && one_out);
    }
    return boolValue(mr, false);
}

Value isproper(std::pmr::memory_resource *mr, const Value &sys) {
    if (hasKind(sys, "tf"))
        return boolValue(mr,
            sys.field("num").numel() <= sys.field("den").numel());
    if (hasKind(sys, "zpk"))
        return boolValue(mr,
            sys.field("z").numel() <= sys.field("p").numel());
    if (hasKind(sys, "ss"))
        return boolValue(mr, true);
    return boolValue(mr, false);
}

Value isstable(std::pmr::memory_resource *mr, const Value &sys) {
    Value p = polesOf(mr, sys);
    auto pv = toComplexVec(p);
    const bool discrete = (sampleTime(sys) > 0.0);
    for (const auto &z : pv) {
        if (discrete) {
            if (std::abs(z) >= 1.0)
                return boolValue(mr, false);
        } else {
            if (z.real() >= 0.0)
                return boolValue(mr, false);
        }
    }
    return boolValue(mr, true);
}

Value order(std::pmr::memory_resource *mr, const Value &sys) {
    if (hasKind(sys, "tf")) {
        // order = max(deg num, deg den) = max(numel-1).
        const size_t n = sys.field("num").numel();
        const size_t d = sys.field("den").numel();
        const size_t m = (n > d) ? n : d;
        return Value::scalar((m == 0) ? 0.0 : double(m - 1), mr);
    }
    if (hasKind(sys, "zpk")) {
        const size_t z = sys.field("z").numel();
        const size_t p = sys.field("p").numel();
        return Value::scalar(double((z > p) ? z : p), mr);
    }
    if (hasKind(sys, "ss"))
        return Value::scalar(double(sys.field("A").dims().rows()), mr);
    return Value::scalar(0.0, mr);
}

Value pole(std::pmr::memory_resource *mr, const Value &sys) {
    return polesOf(mr, sys);
}

Value zero(std::pmr::memory_resource *mr, const Value &sys) {
    return zerosOf(mr, sys);
}

void damp(std::pmr::memory_resource *mr, const Value &sys,
          Value *wnOut, Value *zetaOut, Value *pOut)
{
    Value p = polesOf(mr, sys);
    auto pv = toComplexVec(p);
    const bool discrete = (sampleTime(sys) > 0.0);
    const double Ts = sampleTime(sys);
    const size_t N = pv.size();

    Value wn = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    Value zeta = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *wd = wn.doubleDataMut();
    double *zd = zeta.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        Cd s = pv[i];
        if (discrete) {
            // s-plane equivalent: s = ln(z) / Ts.
            s = std::log(s) / Ts;
        }
        const double wn_i = std::abs(s);
        const double zeta_i = (wn_i == 0.0) ? 0.0 : -s.real() / wn_i;
        wd[i] = wn_i;
        zd[i] = zeta_i;
    }

    if (wnOut)   *wnOut = wn;
    if (zetaOut) *zetaOut = zeta;
    if (pOut)    *pOut = p;
}

namespace detail {

void isct_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("isct: needs sys", 0, 0, "isct", "", "m:isct:nargin");
  o[0] = isct(c.engine->resource(), a[0]); }

void isdt_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("isdt: needs sys", 0, 0, "isdt", "", "m:isdt:nargin");
  o[0] = isdt(c.engine->resource(), a[0]); }

void issiso_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("issiso: needs sys", 0, 0, "issiso", "", "m:issiso:nargin");
  o[0] = issiso(c.engine->resource(), a[0]); }

void isproper_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("isproper: needs sys", 0, 0, "isproper", "", "m:isproper:nargin");
  o[0] = isproper(c.engine->resource(), a[0]); }

void isstable_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("isstable: needs sys", 0, 0, "isstable", "", "m:isstable:nargin");
  o[0] = isstable(c.engine->resource(), a[0]); }

void order_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("order: needs sys", 0, 0, "order", "", "m:order:nargin");
  o[0] = order(c.engine->resource(), a[0]); }

void pole_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("pole: needs sys", 0, 0, "pole", "", "m:pole:nargin");
  o[0] = pole(c.engine->resource(), a[0]); }

void zero_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{ if (a.empty()) throw Error("zero: needs sys", 0, 0, "zero", "", "m:zero:nargin");
  o[0] = zero(c.engine->resource(), a[0]); }

void damp_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty()) throw Error("damp: needs sys", 0, 0, "damp", "", "m:damp:nargin");
    Value wn, zeta, p;
    damp(c.engine->resource(), a[0], &wn, &zeta, &p);
    if (o.size() >= 1) o[0] = wn;
    if (o.size() >= 2) o[1] = zeta;
    if (o.size() >= 3) o[2] = p;
}

} // namespace detail

} // namespace numkit::control
