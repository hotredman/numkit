// libs/linalg/src/schur_convert.cpp
//
// cdf2rdf — complex-diagonal Schur → real-block-diagonal Schur
// rsf2csf — real-block-diagonal Schur → complex-diagonal Schur
//
// Both functions convert between the two equivalent representations
// of the (quasi-)triangular Schur factor for real input matrices.
// MATLAB ships them as utilities for users who need to switch
// between the two forms (e.g. interpreting Schur output from one
// algorithm and feeding it to another that expects the other form).
//
// Practical note: numkit's eig_general_VD currently throws on
// matrices with complex eigenvalues (Francis QR deferred), so the
// complex-input branches here are exercised only when the user
// constructs (V, D) manually or imports them from another source.

#include <numkit/linalg/schur_convert.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/span.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::linalg {

namespace {

constexpr double kPairTol = 1e-10;

inline bool isApproxConj(const Complex &a, const Complex &b, double tol)
{
    return std::abs(a.real() - b.real()) <= tol * (1.0 + std::abs(a.real()))
        && std::abs(a.imag() + b.imag()) <= tol * (1.0 + std::abs(a.imag()));
}

} // anonymous namespace

// ────────────────────────────────────────────────────────────────────────
// cdf2rdf
// ────────────────────────────────────────────────────────────────────────

std::tuple<Value, Value>
cdf2rdf(const Value &V, const Value &D, std::pmr::memory_resource *mr)
{
    if (V.dims().ndim() != 2 || D.dims().ndim() != 2)
        throw Error("cdf2rdf: V and D must be 2-D matrices",
                    0, 0, "cdf2rdf", "", "numkit:cdf2rdf:notMatrix");
    const std::size_t n = static_cast<std::size_t>(D.dims().dim(0));
    if (n != static_cast<std::size_t>(D.dims().dim(1)))
        throw Error("cdf2rdf: D must be square",
                    0, 0, "cdf2rdf", "", "numkit:cdf2rdf:notSquare");
    if (static_cast<std::size_t>(V.dims().dim(0)) != n
        || static_cast<std::size_t>(V.dims().dim(1)) != n)
        throw Error("cdf2rdf: V and D shape mismatch",
                    0, 0, "cdf2rdf", "", "numkit:cdf2rdf:shapeMismatch");
    if (n == 0)
        return std::make_tuple(
            Value::matrix(0, 0, ValueType::DOUBLE, mr),
            Value::matrix(0, 0, ValueType::DOUBLE, mr));

    // Read eigenvalues off D's diagonal. Walk i = 0..n-1; for each
    // real eigenvalue copy column; for each complex eigenvalue expect
    // its conjugate immediately at i+1 and emit a 2×2 real block.
    auto VRout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    auto DRout = Value::matrix(n, n, ValueType::DOUBLE, mr);
    double *vr = VRout.doubleDataMut();
    double *dr = DRout.doubleDataMut();
    std::fill(vr, vr + n * n, 0.0);
    std::fill(dr, dr + n * n, 0.0);

    auto getD = [&](std::size_t i, std::size_t j) -> Complex {
        if (D.isComplex())
            return D.complexElem(i, j);
        return Complex(D.elemAsDouble(i + j * n), 0.0);
    };
    auto getV = [&](std::size_t i, std::size_t j) -> Complex {
        if (V.isComplex())
            return V.complexElem(i, j);
        return Complex(V.elemAsDouble(i + j * n), 0.0);
    };

    std::size_t i = 0;
    while (i < n) {
        Complex lam = getD(i, i);
        // Treat anything within tolerance of real as real.
        if (std::fabs(lam.imag()) <= kPairTol * (1.0 + std::fabs(lam.real()))) {
            // Real eigenvalue.
            dr[i + i * n] = lam.real();
            for (std::size_t r = 0; r < n; ++r)
                vr[r + i * n] = getV(r, i).real();
            ++i;
            continue;
        }
        // Expect a conjugate pair at (i, i+1).
        if (i + 1 >= n)
            throw Error("cdf2rdf: dangling complex eigenvalue at end of D",
                        0, 0, "cdf2rdf", "", "numkit:cdf2rdf:badPair");
        Complex lam2 = getD(i + 1, i + 1);
        if (!isApproxConj(lam, lam2, kPairTol))
            throw Error("cdf2rdf: complex eigenvalues must come in conjugate pairs",
                        0, 0, "cdf2rdf", "", "numkit:cdf2rdf:badPair");
        const double a = lam.real();
        const double b = lam.imag();   // assume b > 0; swap if not
        const double bb = (b >= 0.0) ? b : -b;
        // 2×2 real block. MATLAB convention is [[a b]; [-b a]] — i.e.
        // the positive-imag eigenvalue sits in the upper-right.
        dr[i + i * n]         =  a;
        dr[i + (i + 1) * n]   =  bb;
        dr[(i + 1) + i * n]   = -bb;
        dr[(i + 1) + (i + 1) * n] = a;
        // Real eigvecs paired with the above block convention:
        // VR(:, k+1) = +Im(v) for positive b. Flips for negative b.
        for (std::size_t r = 0; r < n; ++r) {
            Complex v = getV(r, i);
            vr[r + i * n]       = v.real();
            vr[r + (i + 1) * n] = (b >= 0.0) ? v.imag() : -v.imag();
        }
        i += 2;
    }
    return std::make_tuple(std::move(VRout), std::move(DRout));
}

// ────────────────────────────────────────────────────────────────────────
// rsf2csf
// ────────────────────────────────────────────────────────────────────────

std::tuple<Value, Value>
rsf2csf(const Value &UR, const Value &TR, std::pmr::memory_resource *mr)
{
    if (UR.dims().ndim() != 2 || TR.dims().ndim() != 2)
        throw Error("rsf2csf: UR and TR must be 2-D matrices",
                    0, 0, "rsf2csf", "", "numkit:rsf2csf:notMatrix");
    const std::size_t n = static_cast<std::size_t>(TR.dims().dim(0));
    if (n != static_cast<std::size_t>(TR.dims().dim(1)))
        throw Error("rsf2csf: TR must be square",
                    0, 0, "rsf2csf", "", "numkit:rsf2csf:notSquare");
    if (static_cast<std::size_t>(UR.dims().dim(0)) != n
        || static_cast<std::size_t>(UR.dims().dim(1)) != n)
        throw Error("rsf2csf: UR and TR shape mismatch",
                    0, 0, "rsf2csf", "", "numkit:rsf2csf:shapeMismatch");
    if (n == 0)
        return std::make_tuple(
            Value::matrix(0, 0, ValueType::COMPLEX, mr),
            Value::matrix(0, 0, ValueType::COMPLEX, mr));

    // Working copies promoted to complex.
    std::vector<Complex> Td(n * n), Ud(n * n);
    for (std::size_t j = 0; j < n; ++j)
        for (std::size_t i = 0; i < n; ++i) {
            Td[i + j * n] = Complex(TR.elemAsDouble(i + j * n), 0.0);
            Ud[i + j * n] = Complex(UR.elemAsDouble(i + j * n), 0.0);
        }

    // Walk diagonal. For each non-zero sub-diagonal entry TR(i+1, i),
    // the 2×2 block at (i, i+1) has complex eigenvalues. Apply a 2×2
    // unitary similarity (Givens-like with complex c, s) that zeros
    // the sub-diagonal entry and brings the diagonal entries to the
    // eigenvalues of the block.
    std::size_t i = 0;
    while (i + 1 < n) {
        const Complex sub = Td[(i + 1) + i * n];
        if (std::abs(sub) <= kPairTol) {
            ++i;
            continue;
        }
        // 2×2 block: [[a c]; [d a']]. Compute eigenvalues
        //   λ = ((a + a') ± sqrt((a - a')^2 + 4·c·d)) / 2
        const Complex a  = Td[i + i * n];
        const Complex ap = Td[(i + 1) + (i + 1) * n];
        const Complex c  = Td[i + (i + 1) * n];
        const Complex d  = sub;
        const Complex disc = std::sqrt((a - ap) * (a - ap) + Complex(4.0, 0.0) * c * d);
        const Complex lam1 = (a + ap + disc) * Complex(0.5, 0.0);
        const Complex lam2 = (a + ap - disc) * Complex(0.5, 0.0);

        // Build the 2-component eigenvector of the 2×2 block for lam1:
        // (A - lam1·I) · x = 0. Take x = [c; lam1 - a]; normalise.
        Complex x0 = c;
        Complex x1 = lam1 - a;
        // If c is ~0, fall back to alt: x = [lam1 - a'; d].
        if (std::abs(x0) < std::abs(x1) * 1e-300) {
            x0 = lam1 - ap;
            x1 = d;
        }
        const double nrm = std::sqrt(std::norm(x0) + std::norm(x1));
        if (nrm == 0.0) {
            ++i;
            continue;
        }
        x0 /= nrm; x1 /= nrm;
        // Build orthonormal complement y = [-conj(x1); conj(x0)]:
        // then U_block = [x y] is unitary; U_block' * block * U_block
        // is upper triangular with eigenvalues lam1, lam2 on diagonal.
        const Complex y0 = -std::conj(x1);
        const Complex y1 =  std::conj(x0);

        // Apply U_block from the RIGHT to columns i, i+1 of T:
        //   T_new(:, i)   = T(:, i)*x0 + T(:, i+1)*x1
        //   T_new(:, i+1) = T(:, i)*y0 + T(:, i+1)*y1
        for (std::size_t r = 0; r < n; ++r) {
            const Complex t0 = Td[r + i * n];
            const Complex t1 = Td[r + (i + 1) * n];
            Td[r + i * n]       = t0 * x0 + t1 * x1;
            Td[r + (i + 1) * n] = t0 * y0 + t1 * y1;
        }
        // Apply U_block' from the LEFT to rows i, i+1 of T:
        //   T_new(i,   :) = conj(x0)*T(i, :) + conj(x1)*T(i+1, :)
        //   T_new(i+1, :) = conj(y0)*T(i, :) + conj(y1)*T(i+1, :)
        for (std::size_t k = 0; k < n; ++k) {
            const Complex t0 = Td[i + k * n];
            const Complex t1 = Td[(i + 1) + k * n];
            Td[i + k * n]       = std::conj(x0) * t0 + std::conj(x1) * t1;
            Td[(i + 1) + k * n] = std::conj(y0) * t0 + std::conj(y1) * t1;
        }
        // Apply U_block to columns i, i+1 of U.
        for (std::size_t r = 0; r < n; ++r) {
            const Complex u0 = Ud[r + i * n];
            const Complex u1 = Ud[r + (i + 1) * n];
            Ud[r + i * n]       = u0 * x0 + u1 * x1;
            Ud[r + (i + 1) * n] = u0 * y0 + u1 * y1;
        }
        // The sub-diagonal entry should now be ~0 — clear it for cleanliness.
        Td[(i + 1) + i * n] = Complex(0.0, 0.0);

        i += 2;  // skip the freshly-diagonalised pair
    }

    auto Uout = Value::matrix(n, n, ValueType::COMPLEX, mr);
    auto Tout = Value::matrix(n, n, ValueType::COMPLEX, mr);
    Complex *Up = Uout.complexDataMut();
    Complex *Tp = Tout.complexDataMut();
    std::copy(Ud.begin(), Ud.end(), Up);
    std::copy(Td.begin(), Td.end(), Tp);
    return std::make_tuple(std::move(Uout), std::move(Tout));
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void cdf2rdf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("cdf2rdf: requires (V, D)",
                    0, 0, "cdf2rdf", "", "numkit:cdf2rdf:nargin");
    auto [VR, DR] = cdf2rdf(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(VR);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(DR);
}

void rsf2csf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("rsf2csf: requires (U, T)",
                    0, 0, "rsf2csf", "", "numkit:rsf2csf:nargin");
    auto [U, T] = rsf2csf(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(U);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(T);
}

} // namespace detail

} // namespace numkit::linalg
