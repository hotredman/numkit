// libs/control/src/conversion/conversion.cpp
//
// Inter-form conversions tf ↔ zpk ↔ ss. Built on the existing
// builtin::roots / builtin::poly polynomial primitives, plus a local
// Faddeev–LeVerrier expansion for ss2tf.

#include <numkit/control/conversion/conversion.hpp>

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
                    0, 0, "tf2zp", "", "m:tf2zp:den");

    // Gain is num(1)/den(1) (after stripping leading zeros).
    const double k = numV.empty() ? 0.0 : numV[0] / denV[0];

    // Roots of num and den. The existing builtin::roots already
    // handles both real-and-complex output and trailing-zero roots.
    Value zRoots = builtin::roots(num, mr);
    Value pRoots = builtin::roots(den, mr);

    return {std::move(zRoots), std::move(pRoots),
            Value::scalar(k, mr)};
}

std::pair<Value, Value>
zp2tf(const Value &z, const Value &p, const Value &k,
      std::pmr::memory_resource *mr)
{
    // num = k * poly(z), den = poly(p). builtin::poly takes a column
    // vector of roots and returns the row of polynomial coefficients
    // (leading 1, descending powers).
    Value den = builtin::poly(p, mr);
    Value num = builtin::poly(z, mr);

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
                    0, 0, "tf2ss", "", "m:tf2ss:den");
    const double a0 = denV[0];
    // Normalise so leading coefficient of den is 1.
    std::vector<double> a(denV.size());
    for (size_t i = 0; i < denV.size(); ++i) a[i] = denV[i] / a0;

    auto numV = coeffsReal(num);
    if (numV.size() > a.size())
        throw Error("tf2ss: numerator longer than denominator (improper system)",
                    0, 0, "tf2ss", "", "m:tf2ss:improper");
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
                    0, 0, "ss2tf", "", "m:ss2tf:A");

    // Columns of B, C and D referring to the iu-th input (1-based).
    const size_t nin = (B.dims().cols() == 0) ? 1 : B.dims().cols();
    if (iu < 1 || static_cast<size_t>(iu) > nin)
        throw Error("ss2tf: iu out of range",
                    0, 0, "ss2tf", "", "m:ss2tf:iu");
    const size_t inIdx = static_cast<size_t>(iu - 1);
    const size_t ny = C.dims().rows();
    if (ny != 1)
        throw Error("ss2tf: only single-output systems supported "
                    "(got C with rows != 1)",
                    0, 0, "ss2tf", "", "m:ss2tf:miso");

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

// No `*_reg` adapters are provided here — every conversion entry
// point (tf2zp, zp2tf in libs/builtin; tf2ss, ss2tf in libs/signal)
// is already registered as a builtin. The C++ APIs above are kept
// public so upcoming control cycles (e.g. step / lsim / feedback)
// can compose them internally without paying the runtime dispatch
// tax on the builtin lookup.

} // namespace numkit::control
