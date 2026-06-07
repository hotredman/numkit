// libs/builtin/src/math/elementary/polynomials.cpp

#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include "helpers.hpp"
#include "poly_helpers.hpp"

#include <numkit/builtin/language/arrays/matrix.hpp>  // poly_of_matrix

#include <cmath>
#include <cstring>
#include <memory_resource>
#include <tuple>

#include "polynomials_detail.hpp"

namespace numkit::builtin {

Value roots(const Value &p, std::pmr::memory_resource *mr)
{
    if (p.type() == ValueType::COMPLEX)
        throw Error("roots: complex coefficient input is not supported",
                     0, 0, "roots", "", "numkit:roots:complex");
    if (!p.dims().isVector() && !p.isScalar() && !p.isEmpty())
        throw Error("roots: argument must be a vector",
                     0, 0, "roots", "", "numkit:roots:notVector");

    ScratchArena scratch(mr);

    // Read coefficients as DOUBLE (promote integer/single/logical).
    const std::size_t n = p.numel();
    auto coeffs = ScratchVec<double>(n, &scratch);
    for (std::size_t i = 0; i < n; ++i)
        coeffs[i] = p.elemAsDouble(i);

    auto rs = detail::polyRootsDurandKerner(&scratch, coeffs.data(), coeffs.size());
    const std::size_t k = rs.size();

    // If every root is real, return a real column. Otherwise return COMPLEX.
    bool anyComplex = false;
    for (const auto &r : rs)
        if (std::abs(r.imag()) > 1e-12 * (std::abs(r.real()) + 1.0)) {
            anyComplex = true;
            break;
        }

    if (!anyComplex) {
        auto out = Value::matrix(k, 1, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < k; ++i)
            out.doubleDataMut()[i] = rs[i].real();
        return out;
    }
    auto out = Value::complexMatrix(k, 1, mr);
    for (std::size_t i = 0; i < k; ++i)
        out.complexDataMut()[i] = rs[i];
    return out;
}

// ── polyder / polyint ───────────────────────────────────────────────

Value polyder(const Value &p, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto pv = readPolyAsDouble(p, "polyder", &scratch);
    auto deriv = polyderRaw(pv.data(), pv.size(), &scratch);
    // MATLAB convention: polynomial coefficient vectors are canonical
    // (no leading zeros), so a polyder result like [0, 1, 2] must be
    // trimmed to [1, 2]. See BUGS.md #12.
    trimLeadingZeros(deriv);
    return rowFromVec(deriv.data(), deriv.size(), mr);
}

std::tuple<Value, Value>
polyder(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto bv = readPolyAsDouble(b, "polyder", &scratch);
    auto av = readPolyAsDouble(a, "polyder", &scratch);
    auto bp = polyderRaw(bv.data(), bv.size(), &scratch);
    auto ap = polyderRaw(av.data(), av.size(), &scratch);
    // num = a * b' - b * a'
    auto t1 = polyConv(av.data(), av.size(), bp.data(), bp.size(), &scratch);
    auto t2 = polyConv(bv.data(), bv.size(), ap.data(), ap.size(), &scratch);
    // Align lengths (pad with leading zeros so subtraction is safe).
    if (t1.size() < t2.size()) t1.insert(t1.begin(), t2.size() - t1.size(), 0.0);
    if (t2.size() < t1.size()) t2.insert(t2.begin(), t1.size() - t2.size(), 0.0);
    ScratchVec<double> num(t1.size(), &scratch);
    for (std::size_t i = 0; i < t1.size(); ++i) num[i] = t1[i] - t2[i];
    trimLeadingZeros(num);
    auto den = polyConv(av.data(), av.size(), av.data(), av.size(), &scratch);
    trimLeadingZeros(den);
    return std::make_tuple(rowFromVec(num.data(), num.size(), mr),
                           rowFromVec(den.data(), den.size(), mr));
}

// Read a vector input of (real or COMPLEX) numbers as a Complex vector.

Value polyint(const Value &p, double k, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto pv = readPolyAsDouble(p, "polyint", &scratch);
    if (pv.empty()) {
        // ∫ 0 dx = k.
        auto out = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = k;
        return out;
    }
    const std::size_t n = pv.size();
    auto r = ScratchVec<double>(n + 1, &scratch);
    for (std::size_t i = 0; i < n; ++i) {
        const double exponent = static_cast<double>(n - i);
        r[i] = pv[i] / exponent;
    }
    r[n] = k;
    return rowFromVec(r.data(), r.size(), mr);
}

// ── tf2zp / zp2tf ───────────────────────────────────────────────────
std::tuple<Value, Value, Value>
tf2zp(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto bv = readPolyAsDouble(b, "tf2zp", &scratch);
    auto av = readPolyAsDouble(a, "tf2zp", &scratch);
    if (av.empty() || av[0] == 0.0)
        throw Error("tf2zp: leading denominator coefficient must be non-zero",
                     0, 0, "tf2zp", "", "numkit:tf2zp:badDen");
    // MATLAB strips leading zeros from the numerator before forming the
    // ZPK gain: b = [0 0 0 0 1] is the polynomial "1", so the gain is 1,
    // NOT b(1) = 0. The root finder (polyRootsDurandKerner) already ignores
    // leading zeros for the zeros, so a naive k = bv[0]/av[0] gave a bogus
    // gain of 0 whenever b was zero-padded (e.g. zp2tf output fed into
    // lp2lp/lp2bp via tf2zpk).
    size_t bnz = 0;
    while (bnz < bv.size() && bv[bnz] == 0.0) ++bnz;
    if (bv.empty() || bnz == bv.size()) {
        // Numerator is empty or all zeros → no zeros, gain 0.
        auto z = Value::matrix(0, 1, ValueType::DOUBLE, mr);
        auto pRoots = detail::polyRootsDurandKerner(&scratch, av.data(), av.size());
        auto p = realColIfFlat(pRoots.data(), pRoots.size(), mr);
        auto k = Value::scalar(0.0, mr);
        return std::make_tuple(std::move(z), std::move(p), std::move(k));
    }
    auto zRoots = detail::polyRootsDurandKerner(&scratch, bv.data(), bv.size());
    auto pRoots = detail::polyRootsDurandKerner(&scratch, av.data(), av.size());
    const double k = bv[bnz] / av[0];

    return std::make_tuple(realColIfFlat(zRoots.data(), zRoots.size(), mr),
                           realColIfFlat(pRoots.data(), pRoots.size(), mr),
                           Value::scalar(k, mr));
}

std::tuple<Value, Value>
zp2tf(const Value &z, const Value &p, double k, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    auto zv = readVecAsComplex(z, "zp2tf", &scratch);
    auto pv = readVecAsComplex(p, "zp2tf", &scratch);
    auto bRaw = detail::polyExpandFromRoots(&scratch, zv.data(), zv.size());
    auto aRaw = detail::polyExpandFromRoots(&scratch, pv.data(), pv.size());
    for (auto &v : bRaw) v *= k;
    // MATLAB returns b with the SAME length as a: when there are fewer
    // zeros than poles, the numerator is left-padded with zeros (the
    // surplus high-order coefficients are 0). E.g. one zero, two poles ->
    // b = [0 b0 b1], not [b0 b1].
    if (bRaw.size() < aRaw.size()) {
        const size_t off = aRaw.size() - bRaw.size();
        ScratchVec<double> bPad(aRaw.size(), &scratch);
        for (size_t i = 0; i < aRaw.size(); ++i) bPad[i] = 0.0;
        for (size_t i = 0; i < bRaw.size(); ++i) bPad[off + i] = bRaw[i];
        return std::make_tuple(rowFromVec(bPad.data(), bPad.size(), mr),
                               rowFromVec(aRaw.data(), aRaw.size(), mr));
    }
    return std::make_tuple(rowFromVec(bRaw.data(), bRaw.size(), mr),
                           rowFromVec(aRaw.data(), aRaw.size(), mr));
}

// ── Pack 29: poly / polyvalm / polydiv ───────────────────────────────

Value poly(const Value &r, std::pmr::memory_resource *mr)
{
    // Vector-of-roots → coefficient row. Real-only path uses the
    // existing helper (which drops the imaginary residue, fine when
    // the roots have come from `roots(p)` of a real polynomial).
    if (r.isEmpty()) return rowFromVec(nullptr, 0, mr);
    ScratchArena scratch(mr);
    const size_t n = r.numel();
    auto cv = ScratchVec<Complex>(n, &scratch);
    if (r.isComplex()) {
        const Complex *p = r.complexData();
        for (size_t i = 0; i < n; ++i) cv[i] = p[i];
    } else {
        const double *p = r.doubleData();
        for (size_t i = 0; i < n; ++i) cv[i] = Complex(p[i], 0.0);
    }
    auto coeffs = detail::polyExpandFromRoots(&scratch, cv.data(), n);
    return rowFromVec(coeffs.data(), coeffs.size(), mr);
}

Value polyvalm(const Value &p, const Value &A, std::pmr::memory_resource *mr)
{
    if (A.dims().ndim() > 2 || A.dims().rows() != A.dims().cols())
        throw Error("polyvalm: A must be a square matrix",
                     0, 0, "polyvalm", "", "numkit:polyvalm:notSquare");
    const size_t n = A.dims().rows();
    const size_t k = p.numel();

    // Build `result = p_0 * I` (deg-0 case shortcut included).
    auto I = Value::matrix(n, n, ValueType::DOUBLE, mr);
    {
        double *d = I.doubleDataMut();
        std::fill(d, d + n * n, 0.0);
        for (size_t i = 0; i < n; ++i) d[i * n + i] = 1.0;
    }
    if (k == 0) return I;

    // Horner at the matrix level: result = result * A + p_i * I.
    // p[0] is the leading coefficient.
    auto result = Value::matrix(n, n, ValueType::DOUBLE, mr);
    {
        double *d = result.doubleDataMut();
        const double p0 = p.doubleData()[0];
        for (size_t i = 0; i < n * n; ++i) d[i] = 0.0;
        for (size_t i = 0; i < n; ++i) d[i * n + i] = p0;
    }
    for (size_t step = 1; step < k; ++step) {
        // result = result * A
        auto next = Value::matrix(n, n, ValueType::DOUBLE, mr);
        const double *Rp = result.doubleData();
        const double *Ap = A.doubleData();
        double *Np = next.doubleDataMut();
        // Column-major matmul.
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j) {
                double s = 0.0;
                for (size_t kk = 0; kk < n; ++kk)
                    s += Rp[kk * n + i] * Ap[j * n + kk];
                Np[j * n + i] = s;
            }
        // result = next + p[step] * I
        const double pi = p.doubleData()[step];
        for (size_t i = 0; i < n; ++i) Np[i * n + i] += pi;
        result = std::move(next);
    }
    return result;
}

PolyDiv polydiv(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    if (a.isEmpty() || a.numel() == 0)
        throw Error("polydiv: divisor must be non-empty",
                     0, 0, "polydiv", "", "numkit:polydiv:emptyA");
    const size_t na = a.numel();
    const size_t nb = b.numel();
    // Strip leading zeros from a (matches MATLAB behaviour).
    size_t aOff = 0;
    while (aOff + 1 < na && a.doubleData()[aOff] == 0.0) ++aOff;
    const size_t aEff = na - aOff;
    if (aEff == 0 || a.doubleData()[aOff] == 0.0)
        throw Error("polydiv: divisor is zero", 0, 0, "polydiv", "",
                     "numkit:polydiv:zeroDivisor");

    if (nb < aEff) {
        // Quotient is 0; remainder == b.
        auto q = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        q.doubleDataMut()[0] = 0.0;
        return { std::move(q), b };
    }

    ScratchArena scratch(mr);
    auto bb = ScratchVec<double>(nb, &scratch);
    for (size_t i = 0; i < nb; ++i) bb[i] = b.doubleData()[i];
    const double aLead = a.doubleData()[aOff];
    const size_t qLen = nb - aEff + 1;
    auto qv = ScratchVec<double>(qLen, &scratch);
    for (size_t i = 0; i < qLen; ++i) qv[i] = 0.0;

    for (size_t i = 0; i < qLen; ++i) {
        const double coef = bb[i] / aLead;
        qv[i] = coef;
        for (size_t j = 0; j < aEff; ++j)
            bb[i + j] -= coef * a.doubleData()[aOff + j];
    }
    // Remainder: bb[qLen .. nb-1]; trim leading zeros.
    size_t rOff = qLen;
    while (rOff < nb && bb[rOff] == 0.0) ++rOff;
    Value rOut;
    if (rOff >= nb) {
        rOut = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        rOut.doubleDataMut()[0] = 0.0;
    } else {
        rOut = rowFromVec(bb.data() + rOff, nb - rOff, mr);
    }
    return { rowFromVec(qv.data(), qLen, mr), std::move(rOut) };
}

// ── Pack 36: padecoef ────────────────────────────────────────────────
//
// Coefficients of the (N,N) Padé approximant of e^{-T s}:
//
//     e^{-Ts}  ≈  P(-Ts) / P(Ts)
//     P(x)    = Σ_{k=0..N} a_k x^k
//     a_k     = (2N-k)! N! / ((2N)! k! (N-k)!)
//
// Stable forward recurrence:
//     a_0 = 1
//     a_{k+1} / a_k = (N - k) / ((2N - k) (k + 1))
//
// In MATLAB convention (descending power), the coefficient of s^{N-k}
// is `a_k * (-T)^k` for the numerator and `a_k * T^k` for the
// denominator. After computing both vectors we divide through by
// den[0] so the leading denominator coefficient is 1 (matches
// `padecoef(T,N)` in MATLAB).
PadeCoef padecoef(double T, int N, std::pmr::memory_resource *mr)
{
    if (N < 0)
        throw Error("padecoef: order N must be >= 0",
                     0, 0, "padecoef", "", "numkit:padecoef:badN");

    const size_t n = static_cast<size_t>(N) + 1;

    // a[k] for k = 0..N.
    ScratchArena scratch(mr);
    ScratchVec<double> a(n, &scratch);
    a[0] = 1.0;
    for (int k = 0; k < N; ++k)
        a[k + 1] = a[k] * static_cast<double>(N - k)
                       / (static_cast<double>(2 * N - k) * (k + 1));

    auto numV = Value::matrix(1, n, ValueType::DOUBLE, mr);
    auto denV = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *num = numV.doubleDataMut();
    double *den = denV.doubleDataMut();

    // Descending: index 0 is s^N, index N is s^0. So position N-k stores
    // the k-th-power-of-s coefficient.
    double powNegT = 1.0, powT = 1.0;
    for (int k = 0; k <= N; ++k) {
        num[N - k] = a[k] * powNegT;
        den[N - k] = a[k] * powT;
        powNegT *= -T;
        powT    *=  T;
    }

    // Normalize so leading denominator coefficient is 1.
    const double scale = den[0];
    if (scale != 0.0) {
        for (size_t i = 0; i < n; ++i) {
            num[i] /= scale;
            den[i] /= scale;
        }
    }
    return { std::move(numV), std::move(denV) };
}


// ── residue / residuez — partial fraction expansion ─────────────────
//
// Forward form `[r, p, k] = residue(b, a)` only. The inverse form
// `[b, a] = residue(r, p, k)` is a documented v1 gap.
//
// Algorithm (distinct poles):
//   1. polydiv b/a  → quotient k (direct term), remainder b_rem (deg < deg a)
//   2. roots(a)     → poles p (complex)
//   3. Detect repeated poles within `1e-6 · max(1,|p|)` — throw if any.
//   4. a'(s) = polyder(a)
//   5. For each pole p_i, r_i = b_rem(p_i) / a'(p_i)  (Horner, complex)
//
// residuez is the same machinery in z-domain. We use the substitution
// from MATLAB's `residuez.m`: reverse coefficient order to convert
// between B(z^-1) / A(z^-1) (z-domain convention) and the standard
// polynomial form, then apply the same residue formula. Direct term k
// in z-domain is the polynomial in z^-1 — we return it in the same
// MATLAB convention.

namespace {

// Horner's method in complex domain. Coefficients in descending order.
Complex hornerCx(const double *coeffs, std::size_t n, Complex x)
{
    if (n == 0) return Complex(0, 0);
    Complex acc(coeffs[0], 0.0);
    for (std::size_t i = 1; i < n; ++i)
        acc = acc * x + Complex(coeffs[i], 0.0);
    return acc;
}

// Strip leading zeros and verify result is non-empty.
std::vector<double> readPolyStripped(const Value &p, const char *fn)
{
    if (p.type() == ValueType::COMPLEX)
        throw Error(std::string(fn) + ": complex coefficients not supported in v1",
                     0, 0, fn, "", std::string("numkit:") + fn + ":complex");
    if (!p.isEmpty() && !p.dims().isVector() && !p.isScalar())
        throw Error(std::string(fn) + ": arguments must be vectors",
                     0, 0, fn, "", std::string("numkit:") + fn + ":notVector");
    std::vector<double> v(p.numel());
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = p.elemAsDouble(i);
    std::size_t lo = 0;
    while (lo + 1 < v.size() && v[lo] == 0.0) ++lo;
    return std::vector<double>(v.begin() + lo, v.end());
}

// Returns true if any pair of poles is within tol·max(1,|p|).
bool hasRepeatedPoles(const std::vector<Complex> &p)
{
    for (std::size_t i = 0; i < p.size(); ++i)
        for (std::size_t j = i + 1; j < p.size(); ++j) {
            const double scale = std::max(1.0, std::max(std::abs(p[i]), std::abs(p[j])));
            if (std::abs(p[i] - p[j]) < 1e-6 * scale)
                return true;
        }
    return false;
}

// Pack a complex column. If all entries are real (within tol), return
// a real column instead.
Value packComplexOrReal(const std::vector<Complex> &v, std::pmr::memory_resource *mr)
{
    bool anyComplex = false;
    for (const auto &z : v)
        if (std::abs(z.imag()) > 1e-10 * (std::abs(z.real()) + 1.0)) {
            anyComplex = true; break;
        }
    if (!anyComplex) {
        auto out = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
        for (std::size_t i = 0; i < v.size(); ++i)
            out.doubleDataMut()[i] = v[i].real();
        return out;
    }
    auto out = Value::complexMatrix(v.size(), 1, mr);
    for (std::size_t i = 0; i < v.size(); ++i)
        out.complexDataMut()[i] = v[i];
    return out;
}

Value packDirectTerm(const std::vector<double> &k, std::pmr::memory_resource *mr)
{
    if (k.empty() || (k.size() == 1 && k[0] == 0.0))
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto out = Value::matrix(1, k.size(), ValueType::DOUBLE, mr);
    std::memcpy(out.doubleDataMut(), k.data(), k.size() * sizeof(double));
    return out;
}

// Evaluate B(z) at z = p, where B is in z^-1 ascending coefficient order
// (MATLAB z-domain convention: b[0] + b[1]·z^-1 + ... + b[n]·z^-n).
Complex evalZPolyCx(const double *b, std::size_t n, Complex p)
{
    if (n == 0) return Complex(0, 0);
    Complex acc(0, 0);
    const Complex inv = Complex(1, 0) / p;
    Complex term(1, 0);   // (1/p)^k
    for (std::size_t k = 0; k < n; ++k) {
        acc += Complex(b[k], 0) * term;
        term *= inv;
    }
    return acc;
}

ResidueResult residueS(const Value &b, const Value &a,
                       std::pmr::memory_resource *mr)
{
    const char *fn = "residue";
    auto A = readPolyStripped(a, fn);
    auto B = readPolyStripped(b, fn);
    if (A.empty() || A[0] == 0.0)
        throw Error("residue: denominator must be non-zero",
                     0, 0, fn, "", "numkit:residue:zeroDenom");

    // Polynomial division: B = K·A + R   (deg R < deg A).
    std::vector<double> K;
    std::vector<double> R = B;
    if (B.size() >= A.size()) {
        const std::size_t qLen = B.size() - A.size() + 1;
        K.assign(qLen, 0.0);
        std::vector<double> bb = B;
        const double aLead = A[0];
        for (std::size_t i = 0; i < qLen; ++i) {
            const double coef = bb[i] / aLead;
            K[i] = coef;
            for (std::size_t j = 0; j < A.size(); ++j)
                bb[i + j] -= coef * A[j];
        }
        std::size_t rOff = qLen;
        while (rOff < bb.size() && bb[rOff] == 0.0) ++rOff;
        R.assign(bb.begin() + rOff, bb.end());
    }

    // Roots of A → s-domain poles.
    ScratchArena scratch(mr);
    auto rs = detail::polyRootsDurandKerner(&scratch, A.data(), A.size());
    std::vector<Complex> poles(rs.begin(), rs.end());

    if (hasRepeatedPoles(poles))
        throw Error("residue: repeated-pole case not yet supported "
                    "(v1 distinct-poles only — see KNOWN GAP)",
                     0, 0, fn, "", "numkit:residue:repeatedPole");

    // Derivative coefficients (descending).
    std::vector<double> Aprime;
    if (A.size() > 1) {
        const std::size_t n = A.size() - 1;
        Aprime.assign(n, 0.0);
        for (std::size_t i = 0; i < n; ++i)
            Aprime[i] = A[i] * static_cast<double>(n - i);
    }

    // Residues via standard cover-up: r_i = R(p_i) / A'(p_i).
    std::vector<Complex> residues(poles.size(), Complex(0, 0));
    if (!R.empty()) {
        for (std::size_t i = 0; i < poles.size(); ++i) {
            const Complex num = hornerCx(R.data(), R.size(), poles[i]);
            const Complex den = hornerCx(Aprime.data(), Aprime.size(), poles[i]);
            if (std::abs(den) < 1e-300)
                throw Error("residue: derivative vanishes at a pole — "
                            "likely repeated pole undetected by tolerance",
                             0, 0, fn, "", "numkit:residue:denomZero");
            residues[i] = num / den;
        }
    }

    return {
        packComplexOrReal(residues, mr),
        packComplexOrReal(poles, mr),
        packDirectTerm(K, mr),
    };
}

ResidueResult residueZ(const Value &b, const Value &a,
                       std::pmr::memory_resource *mr)
{
    const char *fn = "residuez";
    // z-domain convention: a[0] + a[1]·z^-1 + ... + a[m]·z^-m. The
    // leading scalar is a[0] (z^0 coefficient), not a[m].
    auto av = readPolyStripped(a, fn);
    auto bv = readPolyStripped(b, fn);
    if (av.empty() || av[0] == 0.0)
        throw Error("residuez: denominator a[0] must be non-zero",
                     0, 0, fn, "", "numkit:residuez:zeroDenom");

    // Normalise: divide A, B by a[0] so a[0] becomes 1. Residues come
    // out in MATLAB's residuez convention without further scaling.
    const double a0 = av[0];
    std::vector<double> A = av, B = bv;
    for (auto &x : A) x /= a0;
    for (auto &x : B) x /= a0;

    // z-domain poles = roots(A). For a polynomial in z^-1 ascending,
    // multiplying through by z^m gives a polynomial in z whose roots
    // ARE the z-domain poles. polyRootsDurandKerner treats its input
    // as MATLAB-descending; for our purpose both readings have the
    // same roots (a + b·z^-1 mult by z → a·z + b, roots match s-form).
    ScratchArena scratch(mr);
    auto rs = detail::polyRootsDurandKerner(&scratch, A.data(), A.size());
    std::vector<Complex> poles(rs.begin(), rs.end());

    if (hasRepeatedPoles(poles))
        throw Error("residuez: repeated-pole case not yet supported "
                    "(v1 distinct-poles only — see KNOWN GAP)",
                     0, 0, fn, "", "numkit:residuez:repeatedPole");

    const std::size_t m = poles.size();

    // Direct term: only the proper case (numel(B) <= numel(A)) is
    // supported in v1 — k is empty. The general polynomial-in-z^-1
    // quotient for improper TFs is a documented gap.
    std::vector<double> K;
    if (B.size() > A.size())
        throw Error("residuez: improper transfer functions "
                    "(numel(b) > numel(a)) not yet supported — direct "
                    "term in z^-1 polynomial form is a v1 KNOWN GAP",
                     0, 0, fn, "", "numkit:residuez:improperTF");

    // Residue formula for distinct z-poles (Oppenheim & Schafer 3e §3.4):
    //
    //   r_i = B(p_i) · p_i^(m-1) / prod_{j ≠ i} (p_i - p_j)
    //
    // where B(p_i) is evaluated treating B as a polynomial in z^-1.
    std::vector<Complex> residues(m, Complex(0, 0));
    for (std::size_t i = 0; i < m; ++i) {
        const Complex Bpi = evalZPolyCx(B.data(), B.size(), poles[i]);
        Complex pPow(1, 0);
        for (std::size_t k = 0; k + 1 < m; ++k) pPow *= poles[i];
        Complex denom(1, 0);
        for (std::size_t j = 0; j < m; ++j) {
            if (j == i) continue;
            denom *= (poles[i] - poles[j]);
        }
        if (std::abs(denom) < 1e-300)
            throw Error("residuez: denominator vanishes at a pole — "
                        "likely repeated pole undetected by tolerance",
                         0, 0, fn, "", "numkit:residuez:denomZero");
        residues[i] = Bpi * pPow / denom;
    }

    return {
        packComplexOrReal(residues, mr),
        packComplexOrReal(poles, mr),
        packDirectTerm(K, mr),
    };
}

} // namespace

ResidueResult residue(const Value &b, const Value &a,
                      std::pmr::memory_resource *mr)
{
    return residueS(b, a, mr);
}

ResidueResult residuez(const Value &b, const Value &a,
                       std::pmr::memory_resource *mr)
{
    return residueZ(b, a, mr);
}


// ════════════════════════════════════════════════════════════════════════
// Curve fitting / evaluation (moved from libs/fit)
// ════════════════════════════════════════════════════════════════════════

Value polyfit(const Value &x, const Value &y, int deg, std::pmr::memory_resource *mr)
{
    const size_t m = x.numel();
    const int np = deg + 1;

    if (static_cast<size_t>(np) > m)
        throw Error("polyfit: not enough data points",
                     0, 0, "polyfit", "", "numkit:polyfit:tooFewPoints");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();

    ScratchArena scratch(mr);

    // Vandermonde matrix A[j, i] = x[i]^(deg - j), stored column-major.
    auto A = ScratchVec<double>(m * np, &scratch);
    for (size_t i = 0; i < m; ++i)
        for (int j = 0; j < np; ++j)
            A[j * m + i] = std::pow(xd[i], deg - j);

    // Normal equations: ATA * p = AT * y.
    auto ATA = ScratchVec<double>(static_cast<std::size_t>(np * np), &scratch);
    for (int r = 0; r < np; ++r)
        for (int c = 0; c < np; ++c)
            for (size_t i = 0; i < m; ++i)
                ATA[c * np + r] += A[r * m + i] * A[c * m + i];

    auto ATy = ScratchVec<double>(static_cast<std::size_t>(np), &scratch);
    for (int r = 0; r < np; ++r)
        for (size_t i = 0; i < m; ++i)
            ATy[r] += A[r * m + i] * yd[i];

    // Gaussian elimination with partial pivoting on [ATA | ATy].
    auto aug = ScratchVec<double>(static_cast<std::size_t>(np * (np + 1)), &scratch);
    for (int r = 0; r < np; ++r) {
        for (int c = 0; c < np; ++c)
            aug[r * (np + 1) + c] = ATA[c * np + r];
        aug[r * (np + 1) + np] = ATy[r];
    }

    for (int k = 0; k < np; ++k) {
        int maxRow = k;
        double maxVal = std::abs(aug[k * (np + 1) + k]);
        for (int r = k + 1; r < np; ++r) {
            const double v = std::abs(aug[r * (np + 1) + k]);
            if (v > maxVal) {
                maxVal = v;
                maxRow = r;
            }
        }
        if (maxRow != k) {
            for (int c = 0; c <= np; ++c)
                std::swap(aug[k * (np + 1) + c], aug[maxRow * (np + 1) + c]);
        }

        const double pivot = aug[k * (np + 1) + k];
        if (std::abs(pivot) < 1e-15)
            throw Error("polyfit: singular matrix",
                         0, 0, "polyfit", "", "numkit:polyfit:singular");

        for (int c = k; c <= np; ++c)
            aug[k * (np + 1) + c] /= pivot;
        for (int r = 0; r < np; ++r) {
            if (r == k)
                continue;
            const double f = aug[r * (np + 1) + k];
            for (int c = k; c <= np; ++c)
                aug[r * (np + 1) + c] -= f * aug[k * (np + 1) + c];
        }
    }

    auto p = Value::matrix(1, np, ValueType::DOUBLE, mr);
    for (int j = 0; j < np; ++j)
        p.doubleDataMut()[j] = aug[j * (np + 1) + np];
    return p;
}

Value polyval(const Value &p, const Value &x, std::pmr::memory_resource *mr)
{
    const double *pd = p.doubleData();
    const size_t np = p.numel();
    const size_t nx = x.numel();
    const double *xd = x.doubleData();

    auto r = createLike(x, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < nx; ++i) {
        double val = pd[0];
        for (size_t j = 1; j < np; ++j)
            val = val * xd[i] + pd[j];
        r.doubleDataMut()[i] = val;
    }
    return r;
}

} // namespace numkit::builtin
