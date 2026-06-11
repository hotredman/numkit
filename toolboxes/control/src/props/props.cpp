// toolboxes/control/src/props/props.cpp
//
// LTI model predicates and analytic properties. Every entry point
// dispatches on the struct's `kind` tag ('tf' / 'zpk' / 'ss') so a
// script can call e.g. `isstable(tf(num,den))` or `pole(ss(A,B,C,D))`
// uniformly.
//
// State-space poles are computed via the Faddeev–LeVerrier expansion
// of det(sI − A) (already used by ss2tf in toolboxes/control/conversion);
// the resulting characteristic polynomial is then handed to
// numkit::math::roots. State-space *zeros* fall back to converting to a
// transfer function via toolboxes/signal's ss2tf and taking roots(num) —
// works for SISO systems, which is what `zero(sys)` is most useful
// for in a function-form environment.

#include <numkit/control/props/props.hpp>
#include <numkit/control/internal/numerics.hpp>
#include <numkit/control/conversion/conversion.hpp>

#include <numkit/math/poly/polynomials.hpp>

// Compute-only TU: Value substrate + Error, no engine. The is*/order/pole/
// zero/damp/pzmap/tzero builtins (CallContext wrappers) live in props_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

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

Value boolValue(bool b, std::pmr::memory_resource *mr) {
    return Value::logicalScalar(b, mr);
}

// Char poly thin wrapper around the shared Faddeev–LeVerrier kernel.
// Pulls A out of the LTI struct as a plain column-major double buffer
// and forwards.
std::vector<double> charPoly(const Value &A) {
    const size_t n = A.dims().rows();
    std::vector<double> Av(n * n);
    for (size_t i = 0; i < n * n; ++i) Av[i] = A.elemAsDouble(i);
    return internal::charPoly(Av, n);
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

Value rowFromCoeffs(const std::vector<double> &v, std::pmr::memory_resource *mr) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

// Helper: poles as a column Value, regardless of input form.
Value polesOf(const Value &sys, std::pmr::memory_resource *mr) {
    if (hasKind(sys, "tf"))
        return numkit::math::roots(sys.field("den"), mr);
    if (hasKind(sys, "zpk"))
        return sys.field("p");
    if (hasKind(sys, "ss")) {
        auto cp = charPoly(sys.field("A"));
        return numkit::math::roots(rowFromCoeffs(cp, mr), mr);
    }
    throw Error("expected an LTI struct (tf/zpk/ss)",
                0, 0, "lti", "", "numkit:lti:kind");
}

Value zerosOf(const Value &sys, std::pmr::memory_resource *mr) {
    if (hasKind(sys, "tf"))
        return numkit::math::roots(sys.field("num"), mr);
    if (hasKind(sys, "zpk"))
        return sys.field("z");
    if (hasKind(sys, "ss")) {
        // SISO state-space: convert to (num, den) via ss2tf and root
        // the numerator. Matches MATLAB's transmission-zero behaviour
        // for SISO systems (without needing the QZ generalised eigen-
        // value solver that handles MIMO directly).
        const Value &A = sys.field("A");
        const Value &B = sys.field("B");
        const Value &C = sys.field("C");
        const Value &D = sys.field("D");
        const bool one_in  = (B.dims().cols() == 1);
        const bool one_out = (C.dims().rows() == 1);
        if (!one_in || !one_out) {
            throw Error("zero(sys): MIMO state-space transmission zeros "
                        "require a generalised eigenvalue (QZ) solver; "
                        "not yet implemented in this build.",
                        0, 0, "zero", "", "numkit:zero:miso");
        }
        auto [num, den] = ss2tf(A, B, C, D, /*iu=*/1, mr);
        return numkit::math::roots(num, mr);
    }
    throw Error("expected an LTI struct (tf/zpk/ss)",
                0, 0, "lti", "", "numkit:lti:kind");
}

} // anonymous

Value isct(const Value &sys, std::pmr::memory_resource *mr) {
    return boolValue(sampleTime(sys) == 0.0, mr);
}

Value isdt(const Value &sys, std::pmr::memory_resource *mr) {
    const double Ts = sampleTime(sys);
    return boolValue(Ts > 0.0 || Ts == -1.0, mr);
}

Value issiso(const Value &sys, std::pmr::memory_resource *mr) {
    if (hasKind(sys, "tf") || hasKind(sys, "zpk"))
        return boolValue(true, mr);   // single-output by construction here
    if (hasKind(sys, "ss")) {
        const Value &B = sys.field("B");
        const Value &C = sys.field("C");
        const bool one_in  = (B.dims().cols() == 1);
        const bool one_out = (C.dims().rows() == 1);
        return boolValue(one_in && one_out, mr);
    }
    return boolValue(false, mr);
}

Value isproper(const Value &sys, std::pmr::memory_resource *mr) {
    if (hasKind(sys, "tf"))
        return boolValue(sys.field("num").numel() <= sys.field("den").numel(), mr);
    if (hasKind(sys, "zpk"))
        return boolValue(sys.field("z").numel() <= sys.field("p").numel(), mr);
    if (hasKind(sys, "ss"))
        return boolValue(true, mr);
    return boolValue(false, mr);
}

Value isstable(const Value &sys, std::pmr::memory_resource *mr) {
    Value p = polesOf(sys, mr);
    auto pv = toComplexVec(p);
    const bool discrete = (sampleTime(sys) > 0.0);
    for (const auto &z : pv) {
        if (discrete) {
            if (std::abs(z) >= 1.0)
                return boolValue(false, mr);
        } else {
            if (z.real() >= 0.0)
                return boolValue(false, mr);
        }
    }
    return boolValue(true, mr);
}

Value order(const Value &sys, std::pmr::memory_resource *mr) {
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

Value pole(const Value &sys, std::pmr::memory_resource *mr) {
    return polesOf(sys, mr);
}

Value zero(const Value &sys, std::pmr::memory_resource *mr) {
    return zerosOf(sys, mr);
}

DampResult damp(const Value &sys, std::pmr::memory_resource *mr)
{
    Value p = polesOf(sys, mr);
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

    return {std::move(wn), std::move(zeta), std::move(p)};
}

std::pair<Value, Value>
pzmap(const Value &sys, std::pmr::memory_resource *mr)
{
    return {polesOf(sys, mr), zerosOf(sys, mr)};
}

Value isstatic(const Value &sys, std::pmr::memory_resource *mr)
{
    Value ord = order(sys, mr);
    return Value::logicalScalar(ord.toScalar() == 0.0, mr);
}

Value tzero(const Value &sys, std::pmr::memory_resource *mr)
{
    // SISO build: transmission zeros == ordinary zeros.
    Value siso = issiso(sys, mr);
    if (siso.toScalar() == 0.0)
        throw Error("tzero: only SISO systems supported in this build "
                    "(MIMO transmission zeros need a generalized eigenproblem)",
                    0, 0, "tzero", "", "numkit:tzero:miso");
    return zerosOf(sys, mr);
}

} // namespace numkit::control
