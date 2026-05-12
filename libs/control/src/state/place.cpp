// libs/control/src/state/place.cpp
//
// Ackermann pole placement for SISO (single-input) systems.
//
//   K = [0 … 0 1] · ctrb(A, B)⁻¹ · φ(A),
//
// where φ(s) = ∏ (s − p_i) is the desired closed-loop characteristic
// polynomial. We use builtin::poly to expand p into coefficients in
// MATLAB convention (descending powers, leading 1), then evaluate
// φ(A) by Horner-style matrix multiplications, and finally solve
// the n×n linear system  ctrb(A, B)ᵀ · Kᵀ = (e_nᵀ · φ(A))ᵀ via the
// partial-pivot LU kernel.

#include <numkit/control/state/place.hpp>
#include <numkit/control/state/state.hpp>
#include <numkit/control/internal/numerics.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::control {

namespace {

using Mat = internal::Mat;
using Vec = internal::Vec;
using internal::solveInPlace;
using internal::matmulSq;

Mat readMat(const Value &v, size_t r, size_t c) {
    Mat M(r * c, 0.0);
    for (size_t i = 0; i < r * c; ++i) M[i] = v.elemAsDouble(i);
    return M;
}

Value rowFromVec(const Vec &v, std::pmr::memory_resource *mr) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

// φ(A) for desired char poly with coefficients `c` in MATLAB
// convention: c[0] is leading (highest-degree) coefficient.
//   φ(A) = c[0]·A^n + c[1]·A^(n-1) + … + c[n]·I
// Implemented as Horner:
//   φ(A) = ((( c[0]·I · A + c[1]·I ) · A + c[2]·I ) · A + … ) + c[n]·I
Mat phiOfA(const Mat &A, size_t n, const Vec &c) {
    Mat I(n * n, 0.0);
    for (size_t i = 0; i < n; ++i) I[i * n + i] = 1.0;

    // Initialise H = c[0] · I.
    Mat H(n * n, 0.0);
    for (size_t i = 0; i < n; ++i) H[i * n + i] = c[0];

    for (size_t k = 1; k <= n; ++k) {
        H = matmulSq(H, A, n);
        // H += c[k] · I
        for (size_t i = 0; i < n; ++i) H[i * n + i] += c[k];
    }
    return H;
}

} // anonymous

Value acker(const Value &Av, const Value &Bv, const Value &pv, std::pmr::memory_resource *mr)
{
    const size_t n = Av.dims().rows();
    if (Av.dims().cols() != n)
        throw Error("acker: A must be square",
                    0, 0, "acker", "", "m:acker:A");
    if (Bv.dims().rows() != n || Bv.dims().cols() != 1)
        throw Error("acker: B must be n×1 (SISO only)",
                    0, 0, "acker", "", "m:acker:B");
    if (pv.numel() != n)
        throw Error("acker: pole list must have length n",
                    0, 0, "acker", "", "m:acker:p");

    auto A = readMat(Av, n, n);
    auto B = readMat(Bv, n, 1);

    // Desired characteristic polynomial φ(s) = ∏(s − p_i).
    Value coeffs = builtin::poly(pv, mr);
    Vec c(coeffs.numel());
    for (size_t i = 0; i < coeffs.numel(); ++i) c[i] = coeffs.elemAsDouble(i);
    if (c.size() != n + 1)
        throw Error("acker: poly(p) returned unexpected length",
                    0, 0, "acker", "", "m:acker:poly");

    // φ(A) — n×n.
    Mat phi = phiOfA(A, n, c);

    // Co = ctrb(A, B), n×n for SISO.
    Value CoV = ctrb_AB(Av, Bv, mr);
    Mat Co = readMat(CoV, n, n);

    // Ackermann's formula:  K = e_nᵀ · Co⁻¹ · φ(A).
    // Compute in two steps:
    //   (1) v = e_nᵀ · Co⁻¹     ⟺  Coᵀ · vᵀ = e_n
    //   (2) K = v · φ(A)         (row × matrix → row)
    Mat CoT(n * n, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            CoT[j * n + i] = Co[i * n + j];

    // RHS column = e_n  (last basis vector — entry n-1 is 1).
    Mat eN(n, 0.0);
    eN[n - 1] = 1.0;

    if (!solveInPlace(CoT, eN, n, 1))
        throw Error("acker: controllability matrix is singular "
                    "(system is uncontrollable)",
                    0, 0, "acker", "", "m:acker:singular");
    // eN now holds vᵀ.  v is a length-n row vector.

    // K[k] = sum_j v[j] · phi[j, k].   (column-major phi: phi[j,k] = phi[k*n + j])
    Vec K(n, 0.0);
    for (size_t k = 0; k < n; ++k) {
        double s = 0.0;
        for (size_t j = 0; j < n; ++j)
            s += eN[j] * phi[k * n + j];
        K[k] = s;
    }
    return rowFromVec(K, mr);
}

Value place(const Value &A, const Value &B, const Value &p, std::pmr::memory_resource *mr)
{
    return acker(A, B, p, mr);
}

namespace detail {

void acker_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 3)
        throw Error("acker: requires (A, B, p)",
                    0, 0, "acker", "", "m:acker:nargin");
    o[0] = acker(a[0], a[1], a[2], c.engine->resource());
}

void place_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 3)
        throw Error("place: requires (A, B, p)",
                    0, 0, "place", "", "m:place:nargin");
    o[0] = place(a[0], a[1], a[2], c.engine->resource());
}

} // namespace detail

} // namespace numkit::control
