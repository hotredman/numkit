// toolboxes/signal/src/digital_filtering/poly_utils.cpp
// Signal Processing toolbox polynomial utilities — polyscale + polystab.
// Clean-room implementation written from public references
// and the public references it cites:
//   * A. V. Oppenheim & R. W. Schafer, Discrete-Time Signal Processing,
//     3rd ed., Pearson, 2010 — z-transform scaling property (§3.2),
//     minimum-phase systems / conjugate-reciprocal root reflection (§5.6);
//   * J. D. Markel & A. H. Gray, Linear Prediction of Speech, Springer,
//     1976 — radial root scaling as LPC bandwidth expansion;
//   * M. H. Hayes, Statistical Digital Signal Processing and Modeling,
//     Wiley, 1996 — spectral factorisation / stabilisation.
// polyscale scales every root of a polynomial by a factor (coefficient
// k is multiplied by alpha^k). polystab returns the minimum-phase
// polynomial with the same magnitude response, reflecting any root with
// |root| > 1 to its conjugate reciprocal 1/conj(root).

#include <numkit/signal/digital_filtering/poly_utils.hpp>

#include <numkit/math/poly/polynomials.hpp>  // numkit::math::roots / numkit::math::poly

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

#include <complex>

namespace numkit::signal {

namespace {

// ── Element access ────────────────────────────────────────────────────
// Read element k of a Value uniformly as a Complex. Column-major linear
// index; works for DOUBLE and COMPLEX inputs (the only numeric types
// these two functions need to handle).
inline Complex elemAsComplex(const Value &v, size_t k)
{
    if (v.type() == ValueType::COMPLEX)
        return v.complexData()[k];
    return Complex(v.elemAsDouble(k), 0.0);
}

inline bool isComplex(const Value &v)
{
    return v.type() == ValueType::COMPLEX;
}

// A Value counts as a vector if it is empty, or one of rows/cols is 1
// (a scalar is the 1×1 degenerate case), with no higher dimensions.
inline bool isVectorShape(const Value &v)
{
    const Dims &d = v.dims();
    if (d.ndim() > 2)
        return false;
    return v.isEmpty() || d.rows() == 1 || d.cols() == 1;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────
// polyscale — scale every root of a polynomial by a factor.
// Spec §1.  Scaling the roots of A(z) by α is the z-transform scaling
// property A(z) ↦ A(z/α): the coefficient of z^(N-1-k) is multiplied by
// α^k, so for a length-N coefficient row in descending-power order
//     b[k] = a[k] · p(k),   k = 0 .. N-1
// where p(k) = α^k for scalar α, or p(k) = alpha[k]^k for a row-vector
// alpha (per-coefficient factor). A matrix `a` is treated as one
// polynomial per row, each scaled with the same power factors.
// ─────────────────────────────────────────────────────────────────────
Value polyscale(const Value &p, const Value &scale,
                std::pmr::memory_resource *mr)
{
    // Edge case: empty polynomial → empty result (0×0).
    if (p.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    // Determine the per-row layout of `a`.
    //   * vector input  → one polynomial of length n (output is 1×n row)
    //   * matrix m×n    → m polynomials, each length n (output m×n)
    const Dims &pd = p.dims();
    size_t m, n;
    if (isVectorShape(p)) {
        m = 1;
        n = p.numel();
    } else {
        m = pd.rows();
        n = pd.cols();
    }

    // Resolve the scale argument: either a scalar or a length-n vector.
    const size_t scaleN = scale.numel();
    const bool scaleScalar = (scaleN == 1);
    if (!scaleScalar && scaleN != n) {
        throw numkit::Error(
            "polyscale: scale must be a scalar or a vector matching the "
            "polynomial length",
            0, 0, "polyscale", "", "numkit:polyscale:BadScale");
    }

    // Result is complex iff either input is complex.
    const bool wantComplex = isComplex(p) || isComplex(scale);

    // ── Build the per-coefficient power factors p(k), k = 0 .. n-1 ────
    // p(0) = α^0 = 1 always; p(k) = base(k)^k where base(k) is the
    // scalar α or alpha[k].  Computed once, reused for every row.
    ScratchArena arena(mr);
    ScratchVec<Complex> factor(n, &arena);
    if (n > 0)
        factor[0] = Complex(1.0, 0.0);
    if (scaleScalar) {
        const Complex a = elemAsComplex(scale, 0);
        // Successive multiplication: factor[k] = factor[k-1] · α.
        for (size_t k = 1; k < n; ++k)
            factor[k] = factor[k - 1] * a;
    } else {
        // Per-coefficient base: factor[k] = alpha[k]^k via repeated mul.
        for (size_t k = 1; k < n; ++k) {
            const Complex base = elemAsComplex(scale, k);
            Complex acc(1.0, 0.0);
            for (size_t e = 0; e < k; ++e)
                acc *= base;
            factor[k] = acc;
        }
    }

    // ── Apply: b(i,k) = a(i,k) · factor(k) ───────────────────────────
    Value out = Value::matrix(m, n,
                              wantComplex ? ValueType::COMPLEX
                                          : ValueType::DOUBLE,
                              mr);
    if (wantComplex) {
        Complex *dst = out.complexDataMut();
        for (size_t i = 0; i < m; ++i) {
            for (size_t k = 0; k < n; ++k) {
                // Column-major linear index for an m×n result.
                const size_t lin = k * m + i;
                dst[lin] = elemAsComplex(p, lin) * factor[k];
            }
        }
    } else {
        double *dst = out.doubleDataMut();
        for (size_t i = 0; i < m; ++i) {
            for (size_t k = 0; k < n; ++k) {
                const size_t lin = k * m + i;
                // factor[k] is purely real here (both inputs real).
                dst[lin] = p.elemAsDouble(lin) * factor[k].real();
            }
        }
    }
    return out;
}

// ─────────────────────────────────────────────────────────────────────
// polystab — stabilise a polynomial (minimum-phase version).
// Spec §2.  Returns a polynomial with the same magnitude response as the
// input but with every root inside or on the unit circle. A root r with
// |r| > 1 is reflected to its conjugate reciprocal 1/conj(r) — magnitude
// 1/|r|, same angle — which preserves |A(e^jω)| and changes only the
// phase (Oppenheim & Schafer §5.6, minimum-phase systems).
// Procedure:
//   1. roots r_k of a            (numkit::math::roots)
//   2. r_k ← 1/conj(r_k)  when |r_k| > 1, else keep
//   3. rebuild monic poly p      (numkit::math::poly)
//   4. p ← p · (first non-zero coefficient of a)   — restore gain
//   5. a real → take real part to drop round-off
// ─────────────────────────────────────────────────────────────────────
Value polystab(const Value &a, std::pmr::memory_resource *mr)
{
    // Matrix input is not allowed.
    if (!isVectorShape(a)) {
        throw numkit::Error("Input must be a vector.",
                            0, 0, "polystab", "", "numkit:polystab:notVector");
    }

    // Empty input → empty result (0×0).
    if (a.isEmpty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    const bool aComplex = isComplex(a);
    const size_t n = a.numel();

    // Scalar input: no roots to move — return unchanged, as a 1×1 row.
    if (n == 1) {
        Value out = Value::matrix(1, 1,
                                  aComplex ? ValueType::COMPLEX
                                           : ValueType::DOUBLE,
                                  mr);
        if (aComplex)
            out.complexDataMut()[0] = elemAsComplex(a, 0);
        else
            out.doubleDataMut()[0] = a.elemAsDouble(0);
        return out;
    }

    // ── Step 4 (gain): first non-zero coefficient of a ───────────────
    // roots() ignores leading zeros, so the gain is taken from the
    // first coefficient that is actually non-zero. If every coefficient
    // is zero, a is the zero polynomial — return it unchanged.
    Complex gain(0.0, 0.0);
    bool gainFound = false;
    for (size_t k = 0; k < n; ++k) {
        const Complex c = elemAsComplex(a, k);
        if (c != Complex(0.0, 0.0)) {
            gain = c;
            gainFound = true;
            break;
        }
    }
    if (!gainFound) {
        // All-zero polynomial: nothing to stabilise. Return a copy as a
        // 1×n row.
        Value out = Value::matrix(1, n,
                                  aComplex ? ValueType::COMPLEX
                                           : ValueType::DOUBLE,
                                  mr);
        if (aComplex) {
            Complex *dst = out.complexDataMut();
            for (size_t k = 0; k < n; ++k)
                dst[k] = elemAsComplex(a, k);
        } else {
            double *dst = out.doubleDataMut();
            for (size_t k = 0; k < n; ++k)
                dst[k] = a.elemAsDouble(k);
        }
        return out;
    }

    // ── Step 1: roots of a ───────────────────────────────────────────
    ScratchArena arena(mr);
    Value rv = numkit::math::roots(a, &arena);
    const size_t nr = rv.numel();

    // No finite roots (e.g. a degree-0 leftover after stripping) — the
    // stabilised polynomial is just the gain.
    if (nr == 0) {
        Value out = Value::matrix(1, 1,
                                  aComplex ? ValueType::COMPLEX
                                           : ValueType::DOUBLE,
                                  mr);
        if (aComplex)
            out.complexDataMut()[0] = gain;
        else
            out.doubleDataMut()[0] = gain.real();
        return out;
    }

    // ── Step 2: reflect roots with |r| > 1 to 1/conj(r) ──────────────
    ScratchVec<Complex> stabRoots(nr, &arena);
    bool rootsComplex = false;
    for (size_t k = 0; k < nr; ++k) {
        Complex r = elemAsComplex(rv, k);
        const double mag = std::abs(r);
        if (mag > 1.0) {
            // Conjugate reciprocal: magnitude 1/|r|, unchanged angle.
            // 1/conj(r) = r / |r|^2.
            r = r / (mag * mag);
        }
        // A root on the unit circle satisfies 1/conj(r) == r, so the
        // boundary is a no-op; a zero root stays zero. Both handled by
        // the |r| > 1 guard above.
        stabRoots[k] = r;
        if (r.imag() != 0.0)
            rootsComplex = true;
    }

    // ── Step 3: rebuild the monic polynomial from the stabilised roots.
    // numkit::math::poly needs a COMPLEX vector to yield a COMPLEX result;
    // if all stabilised roots are real, a DOUBLE root vector suffices.
    Value rootVec;
    if (rootsComplex) {
        rootVec = Value::matrix(nr, 1, ValueType::COMPLEX, &arena);
        Complex *rd = rootVec.complexDataMut();
        for (size_t k = 0; k < nr; ++k)
            rd[k] = stabRoots[k];
    } else {
        rootVec = Value::matrix(nr, 1, ValueType::DOUBLE, &arena);
        double *rd = rootVec.doubleDataMut();
        for (size_t k = 0; k < nr; ++k)
            rd[k] = stabRoots[k].real();
    }
    Value monic = numkit::math::poly(rootVec, &arena);
    const size_t mlen = monic.numel();

    // ── Steps 4 & 5: apply gain, drop round-off for real input ───────
    // Result is complex only if the input a is complex; for a real a,
    // reflection maps conjugate pairs to conjugate pairs so the
    // rebuilt polynomial is real up to round-off — take the real part.
    Value out = Value::matrix(1, mlen,
                              aComplex ? ValueType::COMPLEX
                                       : ValueType::DOUBLE,
                              mr);
    if (aComplex) {
        Complex *dst = out.complexDataMut();
        for (size_t k = 0; k < mlen; ++k)
            dst[k] = elemAsComplex(monic, k) * gain;
    } else {
        double *dst = out.doubleDataMut();
        const double g = gain.real();
        for (size_t k = 0; k < mlen; ++k)
            dst[k] = elemAsComplex(monic, k).real() * g;
    }
    return out;
}

} // namespace numkit::signal
