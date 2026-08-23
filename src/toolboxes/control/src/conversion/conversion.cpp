// toolboxes/control/src/conversion/conversion.cpp
//
// Inter-form conversions tf ↔ zpk ↔ ss. Built on the existing
// numkit::builtin::roots / numkit::builtin::poly polynomial primitives, plus a local
// Faddeev–LeVerrier expansion for ss2tf.

#include <numkit/control/conversion/conversion.hpp>

#include <numkit/builtin/polyfun.hpp>

// Compute-only TU: Value substrate + Error, no engine. conversion.cpp has no
// CallContext builtins of its own (the tf2ss/ss2tf/etc. register wrappers live
// in their caller toolboxes); it is pure inter-form numeric conversion.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

namespace numkit::control {

namespace {

using Cd = std::complex<double>;

// Read a row/column polynomial coefficient vector into a vector of
// doubles. Complex coefficients are reduced to their real parts (the
// caller verifies the inputs are real for the SS path).
std::vector<double> coeffsReal(const Value &v) {
    std::vector<double> out(v.numel());
    if (v.type() == ValueType::COMPLEX) {
        const Cd *c = v.complexData();
        for (size_t i = 0; i < v.numel(); ++i) out[i] = c[i].real();
    } else {
        for (size_t i = 0; i < v.numel(); ++i) out[i] = v.elemAsDouble(i);
    }
    return out;
}

// Strip leading zeros (preserve at least 1 entry).
std::vector<double> stripLeading(const std::vector<double> &v) {
    size_t i = 0;
    while (i + 1 < v.size() && v[i] == 0.0) ++i;
    return std::vector<double>(v.begin() + i, v.end());
}

Value rowOfDoubles(const std::vector<double> &v, std::pmr::memory_resource *mr) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

Value matrixFromVec(size_t rows, size_t cols, const std::vector<double> &v, std::pmr::memory_resource *mr) {
    Value m = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), m.doubleDataMut());
    return m;
}

} // anonymous

Tf2ZpResult tf2zp(const Value &num, const Value &den,
                  std::pmr::memory_resource *mr)
{
    auto numV = stripLeading(coeffsReal(num));
    auto denV = stripLeading(coeffsReal(den));
    if (denV.empty() || denV[0] == 0.0)
        throw Error("tf2zp: denominator must have a nonzero leading coefficient",
                    0, 0, "tf2zp", "", "numkit:tf2zp:den");

    // Gain is num(1)/den(1) (after stripping leading zeros).
    const double k = numV.empty() ? 0.0 : numV[0] / denV[0];

    // Roots of num and den. The existing numkit::builtin::roots already
    // handles both real-and-complex output and trailing-zero roots.
    Value zRoots = numkit::builtin::roots(num, mr);
    Value pRoots = numkit::builtin::roots(den, mr);

    return {std::move(zRoots), std::move(pRoots),
            Value::scalar(k, mr)};
}

std::pair<Value, Value>
zp2tf(const Value &z, const Value &p, const Value &k,
      std::pmr::memory_resource *mr)
{
    // num = k * poly(z), den = poly(p). numkit::builtin::poly takes a column
    // vector of roots and returns the row of polynomial coefficients
    // (leading 1, descending powers).
    Value den = numkit::builtin::poly(p, mr);
    Value num = numkit::builtin::poly(z, mr);

    // poly([]) of no zeros is the constant polynomial 1 (MATLAB: poly([])==1).
    // numkit::builtin::poly returns an empty row for empty input, which would
    // collapse num to [] and silently drop the gain (bugs/control/zpk-empty-zeros).
    // Normalize the no-zero case to [1] so num scales to [k].
    if (num.numel() == 0) {
        num = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        num.doubleDataMut()[0] = 1.0;
    }

    // Multiply num by the gain (real scalar in our zpk; we handle the
    // complex-product case by promoting num if k or num is complex).
    const double kVal = k.toScalar();
    if (num.type() == ValueType::COMPLEX) {
        const size_t N = num.numel();
        Value scaled = Value::matrix(1, N, ValueType::COMPLEX, mr);
        const Cd *src = num.complexData();
        Cd *dst = scaled.complexDataMut();
        for (size_t i = 0; i < N; ++i) dst[i] = src[i] * kVal;
        num = scaled;
    } else {
        const size_t N = num.numel();
        Value scaled = Value::matrix(1, N, ValueType::DOUBLE, mr);
        double *dst = scaled.doubleDataMut();
        for (size_t i = 0; i < N; ++i) dst[i] = num.elemAsDouble(i) * kVal;
        num = scaled;
    }

    return {std::move(num), std::move(den)};
}

StateSpace tf2ss(const Value &num, const Value &den,
                 std::pmr::memory_resource *mr)
{
    auto denV = stripLeading(coeffsReal(den));
    if (denV.empty() || denV[0] == 0.0)
        throw Error("tf2ss: invalid denominator",
                    0, 0, "tf2ss", "", "numkit:tf2ss:den");
    const double a0 = denV[0];
    // Normalise so leading coefficient of den is 1.
    std::vector<double> a(denV.size());
    for (size_t i = 0; i < denV.size(); ++i) a[i] = denV[i] / a0;

    auto numV = coeffsReal(num);
    if (numV.size() > a.size())
        throw Error("tf2ss: numerator longer than denominator (improper system)",
                    0, 0, "tf2ss", "", "numkit:tf2ss:improper");
    // Left-pad numerator with zeros so it has the same length as `a`.
    std::vector<double> b(a.size(), 0.0);
    const size_t pad = a.size() - numV.size();
    for (size_t i = 0; i < numV.size(); ++i) b[pad + i] = numV[i] / a0;

    const size_t n = a.size() - 1;  // state-space order
    if (n == 0) {
        // Pure feedthrough: A=0×0, B=0×1, C=1×0, D=b[0].
        return {Value::matrix(0, 0, ValueType::DOUBLE, mr),
                Value::matrix(0, 1, ValueType::DOUBLE, mr),
                Value::matrix(1, 0, ValueType::DOUBLE, mr),
                Value::scalar(b.empty() ? 0.0 : b[0], mr)};
    }

    // Controllable canonical form (column-major storage).
    // A = [0 1 0 ... 0;
    //      0 0 1 ... 0;
    //      ...
    //      0 0 0 ... 1;
    //      -a_n -a_{n-1} ... -a_1]
    std::vector<double> Avec(n * n, 0.0);
    for (size_t i = 0; i + 1 < n; ++i) {
        // column j = i+1, row i.
        Avec[(i + 1) * n + i] = 1.0;
    }
    // Last row: -a_n .. -a_1 (a[1..n] are the non-leading coeffs;
    // we want them reversed because A(n-1, j) = -a[n-j]).
    for (size_t j = 0; j < n; ++j) {
        Avec[j * n + (n - 1)] = -a[n - j];
    }

    // B = [0; ...; 0; 1] (column).
    std::vector<double> Bvec(n, 0.0);
    Bvec[n - 1] = 1.0;

    // C row of length n. With proper rational form b[0] is the D term.
    // For G(s) = b(s)/a(s) with deg(b) <= deg(a), after dividing,
    // G(s) = b[0] + (b[1] - b[0]·a[1]) s^{n-1}/a(s) + ... + (b[n] - b[0]·a[n])/a(s).
    // C[k] = b[n - k] - b[0] · a[n - k]   for k = 0..n-1
    std::vector<double> Cvec(n, 0.0);
    for (size_t k = 0; k < n; ++k) {
        Cvec[k] = b[n - k] - b[0] * a[n - k];
    }

    return {matrixFromVec(n, n, Avec, mr),
            matrixFromVec(n, 1, Bvec, mr),
            matrixFromVec(1, n, Cvec, mr),
            Value::scalar(b[0], mr)};
}

std::pair<Value, Value>
ss2tf(const Value &A, const Value &B,
      const Value &C, const Value &D, int iu,
      std::pmr::memory_resource *mr)
{
    const size_t n = A.dims().rows();
    if (A.dims().cols() != n)
        throw Error("ss2tf: A must be square",
                    0, 0, "ss2tf", "", "numkit:ss2tf:A");

    // Columns of B, C and D referring to the iu-th input (1-based).
    const size_t nin = (B.dims().cols() == 0) ? 1 : B.dims().cols();
    if (iu < 1 || static_cast<size_t>(iu) > nin)
        throw Error("ss2tf: iu out of range",
                    0, 0, "ss2tf", "", "numkit:ss2tf:iu");
    const size_t inIdx = static_cast<size_t>(iu - 1);
    const size_t ny = C.dims().rows();
    if (ny != 1)
        throw Error("ss2tf: only single-output systems supported "
                    "(got C with rows != 1)",
                    0, 0, "ss2tf", "", "numkit:ss2tf:miso");

    // Read matrices into flat column-major double buffers.
    auto readMat = [&](const Value &v, size_t r, size_t c) {
        std::vector<double> M(r * c, 0.0);
        for (size_t i = 0; i < r * c; ++i) M[i] = v.elemAsDouble(i);
        return M;
    };
    auto Av = readMat(A, n, n);
    // B column iu.
    std::vector<double> Bv(n, 0.0);
    for (size_t i = 0; i < n; ++i) Bv[i] = B.elemAsDouble(inIdx * n + i);
    // C row 0 (single-output).
    std::vector<double> Cv(n, 0.0);
    for (size_t i = 0; i < n; ++i) Cv[i] = C.elemAsDouble(i * 1 + 0);
    // D scalar at (0, iu-1).
    double Dscalar = 0.0;
    if (D.numel() > 0) {
        // D is ny × nin; we read element (0, inIdx).
        const size_t Drows = (D.dims().rows() == 0) ? 1 : D.dims().rows();
        Dscalar = D.elemAsDouble(inIdx * Drows + 0);
    }

    // Faddeev–LeVerrier.  M_0 = I,  c_k = -(1/k) trace(A·M_{k-1}),
    // M_k = A·M_{k-1} + c_k I.  char poly = s^n + c_1 s^{n-1} + ... + c_n.
    std::vector<double> M(n * n, 0.0); // current M_{k-1}
    for (size_t i = 0; i < n; ++i) M[i * n + i] = 1.0; // I
    std::vector<double> coeff(n + 1, 0.0);
    coeff[0] = 1.0;

    // num accumulates from C·M_k·B (length n) + D·char(s).
    std::vector<double> numAcc(n, 0.0); // s^{n-1} ... s^0

    auto matMul = [&](const std::vector<double> &X,
                      const std::vector<double> &Y) {
        std::vector<double> Z(n * n, 0.0);
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < n; ++i) {
                double s = 0.0;
                for (size_t l = 0; l < n; ++l)
                    s += X[l * n + i] * Y[j * n + l];
                Z[j * n + i] = s;
            }
        return Z;
    };
    auto matVec = [&](const std::vector<double> &X,
                      const std::vector<double> &y) {
        std::vector<double> z(n, 0.0);
        for (size_t i = 0; i < n; ++i) {
            double s = 0.0;
            for (size_t j = 0; j < n; ++j) s += X[j * n + i] * y[j];
            z[i] = s;
        }
        return z;
    };

    // numAcc[k] gets C·M_k·B for k = 0..n-1.
    {
        // k = 0: M_0 = I → C·I·B = C·B.
        double cb = 0.0;
        for (size_t i = 0; i < n; ++i) cb += Cv[i] * Bv[i];
        numAcc[0] = cb;
    }

    for (size_t k = 1; k <= n; ++k) {
        // A·M_{k-1}
        auto AM = matMul(Av, M);
        // trace(A·M_{k-1})
        double tr = 0.0;
        for (size_t i = 0; i < n; ++i) tr += AM[i * n + i];
        const double ck = -tr / static_cast<double>(k);
        coeff[k] = ck;
        // M_k = A·M_{k-1} + ck · I
        std::vector<double> Mk(n * n, 0.0);
        for (size_t i = 0; i < n * n; ++i) Mk[i] = AM[i];
        for (size_t i = 0; i < n; ++i) Mk[i * n + i] += ck;
        M = std::move(Mk);

        if (k < n) {
            auto MB = matVec(M, Bv);
            double cmb = 0.0;
            for (size_t i = 0; i < n; ++i) cmb += Cv[i] * MB[i];
            numAcc[k] = cmb;
        }
    }

    // Final num = numAcc + D·char_poly.
    // num is degree n (length n+1) when D != 0; otherwise degree n-1.
    std::vector<double> numOutVec;
    if (Dscalar != 0.0) {
        numOutVec.assign(n + 1, 0.0);
        for (size_t i = 0; i <= n; ++i) numOutVec[i] = Dscalar * coeff[i];
        // Add C·M_k·B contributions (degree n-1 .. 0) into positions 1..n.
        for (size_t k = 0; k < n; ++k) numOutVec[k + 1] += numAcc[k];
    } else {
        numOutVec = numAcc; // length n
    }
    // den is the char poly: length n+1, leading 1.
    std::vector<double> denOutVec = coeff;

    return {rowOfDoubles(numOutVec, mr),
            rowOfDoubles(denOutVec, mr)};
}

// --- minreal: pole/zero cancellation -----------------------------------

namespace {

bool isKind(const Value &sys, const char *want) {
    return sys.isStruct() && sys.hasField("kind") &&
           sys.field("kind").toString() == want;
}

double sampleTimeOf(const Value &sys) {
    return (sys.isStruct() && sys.hasField("Ts")) ? sys.field("Ts").toScalar() : 0.0;
}

std::vector<std::complex<double>>
rootsComplex(const Value &polyV, std::pmr::memory_resource *mr) {
    Value r = numkit::builtin::roots(polyV, mr);
    const size_t n = r.numel();
    std::vector<std::complex<double>> out(n);
    if (r.type() == ValueType::COMPLEX) {
        const std::complex<double> *s = r.complexData();
        for (size_t i = 0; i < n; ++i) out[i] = s[i];
    } else {
        for (size_t i = 0; i < n; ++i) out[i] = {r.elemAsDouble(i), 0.0};
    }
    return out;
}

// Monic polynomial whose roots are `rts`, expanded as ∏(s − r_i) in complex
// arithmetic; the real part is returned (conjugate pairs cancel the imaginary
// part exactly, up to round-off noise).
std::vector<double> polyFromRoots(const std::vector<std::complex<double>> &rts) {
    std::vector<std::complex<double>> c{{1.0, 0.0}};
    for (const auto &r : rts) {
        std::vector<std::complex<double>> nc(c.size() + 1, {0.0, 0.0});
        for (size_t i = 0; i < c.size(); ++i) { nc[i] += c[i]; nc[i + 1] -= c[i] * r; }
        c.swap(nc);
    }
    std::vector<double> out(c.size());
    for (size_t i = 0; i < c.size(); ++i) out[i] = c[i].real();
    return out;
}

Value taggedStruct(const char *kind, double Ts, std::pmr::memory_resource *mr) {
    Value s = Value::structure(mr);
    s.field("kind") = Value::fromString(kind, mr);
    s.field("Ts") = Value::scalar(Ts, mr);
    return s;
}

// Cancel common roots of (num, den) within relative tolerance; returns the
// reduced (num, den) as real coefficient rows. Greedy: each pole claims the
// nearest surviving zero within tol — symmetric, so conjugate pairs cancel
// together and the rebuilt polynomials stay real.
std::pair<Value, Value>
cancelTf(const Value &numV, const Value &denV, double tol,
         std::pmr::memory_resource *mr) {
    auto numS = stripLeading(coeffsReal(numV));
    auto denS = stripLeading(coeffsReal(denV));
    if (denS.empty())
        throw Error("minreal: denominator is identically zero",
                    0, 0, "minreal", "", "numkit:minreal:den");
    const double numLead = numS.empty() ? 0.0 : numS.front();
    const double denLead = denS.front();

    auto z = rootsComplex(numV, mr);
    auto p = rootsComplex(denV, mr);

    std::vector<char> zused(z.size(), 0);
    std::vector<std::complex<double>> remP, remZ;
    for (const auto &pj : p) {
        int best = -1;
        double bd = std::numeric_limits<double>::max();
        for (size_t i = 0; i < z.size(); ++i)
            if (!zused[i]) {
                const double d = std::abs(z[i] - pj);
                if (d < bd) { bd = d; best = static_cast<int>(i); }
            }
        if (best >= 0 && bd <= tol * std::max(1.0, std::abs(pj))) zused[best] = 1;
        else remP.push_back(pj);
    }
    for (size_t i = 0; i < z.size(); ++i) if (!zused[i]) remZ.push_back(z[i]);

    auto nn = polyFromRoots(remZ);
    for (auto &c : nn) c *= numLead;
    auto dd = polyFromRoots(remP);
    for (auto &c : dd) c *= denLead;
    return {rowOfDoubles(nn, mr), rowOfDoubles(dd, mr)};
}

} // anonymous

Value minreal(const Value &sys, double tol, std::pmr::memory_resource *mr) {
    if (tol <= 0.0) tol = std::sqrt(std::numeric_limits<double>::epsilon());

    if (isKind(sys, "tf")) {
        auto [n2, d2] = cancelTf(sys.field("num"), sys.field("den"), tol, mr);
        Value s = taggedStruct("tf", sampleTimeOf(sys), mr);
        s.field("num") = std::move(n2);
        s.field("den") = std::move(d2);
        return s;
    }
    if (isKind(sys, "zpk")) {
        auto [num, den] = zp2tf(sys.field("z"), sys.field("p"), sys.field("k"), mr);
        auto [n2, d2] = cancelTf(num, den, tol, mr);
        Value s = taggedStruct("tf", sampleTimeOf(sys), mr);   // returns tf form
        s.field("num") = std::move(n2);
        s.field("den") = std::move(d2);
        return s;
    }
    if (isKind(sys, "ss")) {
        const Value &B = sys.field("B");
        const Value &C = sys.field("C");
        if (C.dims().rows() != 1 || B.dims().cols() != 1)
            throw Error("minreal: only SISO state-space is supported",
                        0, 0, "minreal", "", "numkit:minreal:mimo");
        auto [num, den] = ss2tf(sys.field("A"), B, C, sys.field("D"), 1, mr);
        auto [n2, d2] = cancelTf(num, den, tol, mr);
        StateSpace r = tf2ss(n2, d2, mr);
        Value s = taggedStruct("ss", sampleTimeOf(sys), mr);
        s.field("A") = std::move(r.A);
        s.field("B") = std::move(r.B);
        s.field("C") = std::move(r.C);
        s.field("D") = std::move(r.D);
        return s;
    }
    throw Error("minreal: expected an LTI struct (tf / zpk / ss)",
                0, 0, "minreal", "", "numkit:minreal:kind");
}

// No `*_reg` adapters are provided here — every conversion entry
// point (tf2zp, zp2tf in toolboxes/builtin; tf2ss, ss2tf in toolboxes/signal)
// is already registered as a builtin. The C++ APIs above are kept
// public so upcoming control cycles (e.g. step / lsim / feedback)
// can compose them internally without paying the runtime dispatch
// tax on the builtin lookup. (minreal's CallContext wrapper lives in
// lti_reg.cpp alongside the other model-level builtins.)

} // namespace numkit::control
