// libs/signal/src/filter_implementation/conversions_extras.cpp
//
// sos2tf / sos2zp / tf2zpk + tf↔ss + sos↔ss + zpk↔ss adapters.

#include <numkit/signal/filter_implementation/conversions_extras.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>      // tf2zp / zp2tf / roots
#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/signal/filter_implementation/conversions.hpp>  // tf2sos

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::signal {

namespace {

// Trim trailing zeros from a polynomial coefficient vector. Returns
// the kept length (1 = constant, 0 = empty input).
size_t trimTrailingZeros(std::vector<double> &v)
{
    while (v.size() > 1 && std::abs(v.back()) < 1e-15) v.pop_back();
    return v.size();
}

// Convolve two real polynomials (coefficient-wise multiplication).
std::vector<double> polyConv(const std::vector<double> &a, const std::vector<double> &b)
{
    if (a.empty() || b.empty()) return {};
    std::vector<double> out(a.size() + b.size() - 1, 0.0);
    for (size_t i = 0; i < a.size(); ++i)
        for (size_t j = 0; j < b.size(); ++j)
            out[i + j] += a[i] * b[j];
    return out;
}

Value rowFromVec(const std::vector<double> &v, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < v.size(); ++i) dst[i] = v[i];
    return out;
}

// Append row's roots (as Complex) to the running list.
void appendRoots(const double *coeffs, size_t n, std::vector<Complex> &out, std::pmr::memory_resource *mr)
{
    // Build a 1×n DOUBLE Value to feed roots(). Trim trailing zeros so
    // roots() doesn't add spurious roots at 0.
    std::vector<double> trimmed(coeffs, coeffs + n);
    trimTrailingZeros(trimmed);
    if (trimmed.size() < 2) return;
    auto p = Value::matrix(1, trimmed.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < trimmed.size(); ++i)
        p.doubleDataMut()[i] = trimmed[i];
    auto r = builtin::roots(p, mr);
    if (r.isComplex()) {
        const Complex *src = r.complexData();
        for (size_t i = 0; i < r.numel(); ++i) out.push_back(src[i]);
    } else {
        const double *src = r.doubleData();
        for (size_t i = 0; i < r.numel(); ++i) out.emplace_back(src[i], 0.0);
    }
}

Value complexColFromVec(const std::vector<Complex> &v, std::pmr::memory_resource *mr)
{
    bool anyImag = false;
    for (const auto &c : v)
        if (std::abs(c.imag()) > 1e-12) { anyImag = true; break; }
    if (anyImag) {
        auto out = Value::complexMatrix(v.size(), 1, mr);
        Complex *dst = out.complexDataMut();
        for (size_t i = 0; i < v.size(); ++i) dst[i] = v[i];
        return out;
    }
    auto out = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < v.size(); ++i) dst[i] = v[i].real();
    return out;
}

void requireSosShape(const Value &sos, const char *fn)
{
    if (sos.dims().cols() != 6 || sos.dims().rows() == 0)
        throw Error(std::string(fn) + ": sos must be an L×6 matrix",
                     0, 0, fn, "", std::string("m:") + fn + ":badShape");
}

} // namespace

// ── sos2tf ────────────────────────────────────────────────────────────
std::tuple<Value, Value>
sos2tf(const Value &sos, double g, std::pmr::memory_resource *mr)
{
    requireSosShape(sos, "sos2tf");
    const size_t L = sos.dims().rows();
    std::vector<double> b{1.0}, a{1.0};
    for (size_t i = 0; i < L; ++i) {
        std::vector<double> bs = {sos(i, 0), sos(i, 1), sos(i, 2)};
        std::vector<double> as = {sos(i, 3), sos(i, 4), sos(i, 5)};
        b = polyConv(b, bs);
        a = polyConv(a, as);
    }
    if (g != 1.0)
        for (auto &v : b) v *= g;
    return std::make_tuple(rowFromVec(b, mr), rowFromVec(a, mr));
}

// ── sos2zp ────────────────────────────────────────────────────────────
std::tuple<Value, Value, double>
sos2zp(const Value &sos, double g, std::pmr::memory_resource *mr)
{
    requireSosShape(sos, "sos2zp");
    const size_t L = sos.dims().rows();
    std::vector<Complex> zeros, poles;
    double gain = g;
    for (size_t i = 0; i < L; ++i) {
        const double bArr[3] = {sos(i, 0), sos(i, 1), sos(i, 2)};
        const double aArr[3] = {sos(i, 3), sos(i, 4), sos(i, 5)};
        appendRoots(bArr, 3, zeros, mr);
        appendRoots(aArr, 3, poles, mr);
        // Per-section gain factor: b0 / a0 for that section.
        if (std::abs(aArr[0]) > 1e-15)
            gain *= bArr[0] / aArr[0];
    }
    return std::make_tuple(complexColFromVec(zeros, mr),
                            complexColFromVec(poles, mr),
                            gain);
}

// ── tf2zpk ────────────────────────────────────────────────────────────
std::tuple<Value, Value, double>
tf2zpk(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    auto [z, p, k] = builtin::tf2zp(b, a, mr);
    return std::make_tuple(std::move(z), std::move(p),
                            (k.numel() == 0) ? 0.0 : k.toScalar());
}

// ── tf2ss (controllable canonical form) ───────────────────────────────
std::tuple<Value, Value, Value, Value>
tf2ss(const Value &b, const Value &a, std::pmr::memory_resource *mr)
{
    if (a.numel() == 0)
        throw Error("tf2ss: denominator a must be non-empty",
                     0, 0, "tf2ss", "", "m:tf2ss:badArg");
    const double a0 = a.elemAsDouble(0);
    if (std::abs(a0) < 1e-15)
        throw Error("tf2ss: a(1) must be non-zero",
                     0, 0, "tf2ss", "", "m:tf2ss:badArg");

    // Normalise so a(1) = 1.
    const size_t na = a.numel();
    std::vector<double> ah(na);
    for (size_t i = 0; i < na; ++i) ah[i] = a.elemAsDouble(i) / a0;

    // Pad b to the same length as a (left-pad with zeros — matches
    // MATLAB convention for proper rationals).
    const size_t nb = b.numel();
    std::vector<double> bh(na, 0.0);
    if (nb >= na) {
        for (size_t i = 0; i < na; ++i)
            bh[i] = b.elemAsDouble(nb - na + i) / a0;
    } else {
        for (size_t i = 0; i < nb; ++i)
            bh[na - nb + i] = b.elemAsDouble(i) / a0;
    }

    // Denominator order N. State dimension = N.
    if (na < 2)
        throw Error("tf2ss: order must be >= 1 (denominator length >= 2)",
                     0, 0, "tf2ss", "", "m:tf2ss:badArg");
    const size_t N = na - 1;

    // Direct feed-through D = bh[0]   (since b is normalised + padded
    // to length N+1, this is the leading coeff).
    const double D = bh[0];

    // MATLAB tf2ss returns the controller canonical form:
    //   A = [-a2 -a3 ... -a(N+1);
    //         1   0  ...  0;
    //         0   1  ...  0;
    //         ...
    //         0   0  ...  0]    (companion in TOP row)
    //   B = [1; 0; 0; ...; 0]
    //   C = [b2 - a2*b1, b3 - a3*b1, ..., b(N+1) - a(N+1)*b1]
    //   D = b1
    //
    // Here ah[0]=1 (post-normalisation) and ah[1..N] are -a2..-a(N+1)
    // when negated.
    auto AVal = Value::matrix(N, N, ValueType::DOUBLE, mr);
    double *Ap = AVal.doubleDataMut();
    std::fill(Ap, Ap + N * N, 0.0);
    // Top row (column-major: Ap[r + c*N]).
    for (size_t j = 0; j < N; ++j)
        Ap[0 + j * N] = -ah[j + 1];
    // Identity sub-diagonal: A(i+1, i) = 1 for i = 0..N-2.
    for (size_t i = 0; i + 1 < N; ++i)
        Ap[(i + 1) + i * N] = 1.0;

    // B: column vector with 1 in FIRST entry.
    auto BVal = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *Bp = BVal.doubleDataMut();
    std::fill(Bp, Bp + N, 0.0);
    Bp[0] = 1.0;

    // C: 1xN row, C(j) = bh[j+1] - ah[j+1]*D for j = 0..N-1.
    auto CVal = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *Cp = CVal.doubleDataMut();
    for (size_t j = 0; j < N; ++j)
        Cp[j] = bh[j + 1] - ah[j + 1] * D;

    auto DVal = Value::scalar(D, mr);
    return std::make_tuple(std::move(AVal), std::move(BVal),
                            std::move(CVal), std::move(DVal));
}

// ── ss2tf ─────────────────────────────────────────────────────────────
// SISO. Returns (b, a) where:
//   a = char poly of A   (Faddeev–LeVerrier or characteristic polynomial)
//   b = numerator built from the resolvent (a*D + C * adj(zI - A) * B)
// This is heavy linear algebra; use Faddeev's algorithm.
std::tuple<Value, Value>
ss2tf(const Value &A, const Value &B, const Value &C, double D,
      std::pmr::memory_resource *mr)
{
    if (A.dims().rows() != A.dims().cols())
        throw Error("ss2tf: A must be square",
                     0, 0, "ss2tf", "", "m:ss2tf:badShape");
    const size_t N = A.dims().rows();
    if (B.dims().rows() != N || B.dims().cols() != 1)
        throw Error("ss2tf: B must be N×1",
                     0, 0, "ss2tf", "", "m:ss2tf:badShape");
    if (C.dims().rows() != 1 || C.dims().cols() != N)
        throw Error("ss2tf: C must be 1×N",
                     0, 0, "ss2tf", "", "m:ss2tf:badShape");

    const double Dscalar = D;

    // Faddeev–LeVerrier:
    //   M_0 = I,   c_n = 1
    //   M_k = A * M_{k-1} + c_{n-k+1} * I
    //   c_{n-k} = -trace(A * M_k) / k
    // The polynomial coefficients c_0..c_n satisfy
    //   p(λ) = c_0 + c_1 λ + ... + c_n λ^n   (with c_n = 1).
    // We also accumulate b(λ) = D * a(λ) + Σ C * M_k * B * λ^k.

    auto matMul = [N](const std::vector<double> &X,
                      const std::vector<double> &Y,
                      std::vector<double> &out) {
        out.assign(N * N, 0.0);
        for (size_t i = 0; i < N; ++i)
            for (size_t j = 0; j < N; ++j) {
                double s = 0.0;
                for (size_t k = 0; k < N; ++k)
                    s += X[k * N + i] * Y[j * N + k];   // column-major
                out[j * N + i] = s;
            }
    };
    auto matVec = [N](const std::vector<double> &X,
                      const double *y,
                      std::vector<double> &out) {
        out.assign(N, 0.0);
        for (size_t i = 0; i < N; ++i)
            for (size_t k = 0; k < N; ++k)
                out[i] += X[k * N + i] * y[k];
    };

    std::vector<double> A_(N * N);
    for (size_t i = 0; i < N; ++i)
        for (size_t j = 0; j < N; ++j)
            A_[j * N + i] = A(i, j);
    const double *Bp = B.doubleData();
    const double *Cp = C.doubleData();

    // Faddeev–LeVerrier driving the characteristic polynomial AND
    // adj(sI - A) = sum_{k=0}^{N-1} M_k s^{N-1-k}.
    //
    // a_coeffs are stored from s^0 (index 0) up to s^N (index N);
    // adjoint coefficients C·M_k·B are accumulated into CMB[k] for
    // k=0..N-1 and then placed into b_coeffs[N-1-k]:
    //   b_coeffs[N]       = D
    //   b_coeffs[N-1-k]   = D · a_coeffs[N-1-k] + (C · M_k · B)
    std::vector<double> M(N * N, 0.0), AM(N * N);
    for (size_t i = 0; i < N; ++i) M[i * N + i] = 1.0;   // M_0 = I

    std::vector<double> a_coeffs(N + 1, 0.0);
    a_coeffs[N] = 1.0;
    std::vector<double> CMB(N, 0.0);

    std::vector<double> tmp(N);
    auto dotCMB = [&](const std::vector<double> &Mat) {
        matVec(Mat, Bp, tmp);
        double s = 0.0;
        for (size_t i = 0; i < N; ++i) s += Cp[i] * tmp[i];
        return s;
    };
    CMB[0] = dotCMB(M);   // C * M_0 * B = C * B

    for (size_t k = 1; k <= N; ++k) {
        matMul(A_, M, AM);
        double trace = 0.0;
        for (size_t i = 0; i < N; ++i) trace += AM[i * N + i];
        const double ck = -trace / static_cast<double>(k);
        a_coeffs[N - k] = ck;
        // M_k = AM + ck * I
        for (size_t i = 0; i < N * N; ++i) M[i] = AM[i];
        for (size_t i = 0; i < N; ++i) M[i * N + i] += ck;
        // Capture C·M_k·B for k=1..N-1 (M_N is unused for adjoint).
        if (k < N) CMB[k] = dotCMB(M);
    }

    std::vector<double> b_coeffs(N + 1, 0.0);
    b_coeffs[N] = Dscalar;
    for (size_t k = 0; k < N; ++k)
        b_coeffs[N - 1 - k] = Dscalar * a_coeffs[N - 1 - k] + CMB[k];

    // Reverse to MATLAB ordering (highest-power first).
    std::reverse(a_coeffs.begin(), a_coeffs.end());
    std::reverse(b_coeffs.begin(), b_coeffs.end());

    return std::make_tuple(rowFromVec(b_coeffs, mr), rowFromVec(a_coeffs, mr));
}

// ── ss2zp ─────────────────────────────────────────────────────────────
std::tuple<Value, Value, double>
ss2zp(const Value &A, const Value &B, const Value &C, double D,
      std::pmr::memory_resource *mr)
{
    auto [b, a] = ss2tf(A, B, C, D, mr);
    return tf2zpk(b, a, mr);
}

// ── zp2ss ─────────────────────────────────────────────────────────────
std::tuple<Value, Value, Value, Value>
zp2ss(const Value &z, const Value &p, double k, std::pmr::memory_resource *mr)
{
    auto [b, a] = builtin::zp2tf(z, p, k, mr);
    return tf2ss(b, a, mr);
}

// ── sos2ss ────────────────────────────────────────────────────────────
std::tuple<Value, Value, Value, Value>
sos2ss(const Value &sos, double g, std::pmr::memory_resource *mr)
{
    auto [b, a] = sos2tf(sos, g, mr);
    return tf2ss(b, a, mr);
}

// ── ss2sos ────────────────────────────────────────────────────────────
Value ss2sos(const Value &A, const Value &B, const Value &C, double D,
             std::pmr::memory_resource *mr)
{
    auto [b, a] = ss2tf(A, B, C, D, mr);
    return tf2sos(b, a, mr);
}

// ── ctf2zp (Phase 4.11) ───────────────────────────────────────────────
// Cascaded transfer function (NUM, DEN, SV) → zero/pole/gain.
// Loops over each section: tf2zpk(NUM(i,:), DEN(i,:)) → accumulate.
// Final gain = prod(SV) * prod(per-section gains).
std::tuple<Value, Value, double>
ctf2zp(const Value &NUM, const Value &DEN, const Value &SV, std::pmr::memory_resource *mr)
{
    // Vector inputs → reshape to single-row "section".
    auto rowsOf = [](const Value &v) -> std::size_t {
        return (v.dims().rows() == 1 && v.dims().cols() > 1) ? 1 : v.dims().rows();
    };
    auto colsOf = [](const Value &v) -> std::size_t {
        return (v.dims().rows() == 1 && v.dims().cols() > 1) ? v.dims().cols() : v.dims().cols();
    };
    const std::size_t Knum = rowsOf(NUM);
    const std::size_t Kden = rowsOf(DEN);
    const std::size_t K = std::max(Knum, Kden);
    if (K == 0)
        throw Error("ctf2zp: NUM/DEN must be non-empty",
                    0, 0, "ctf2zp", "", "m:ctf2zp:Empty");

    // SV defaults to 1 if not provided.
    std::vector<double> sv;
    if (SV.isEmpty() || SV.isEmpty()) {
        sv.assign(K + 1, 1.0);
    } else if (SV.numel() == 1) {
        sv.assign(K + 1, 1.0);
        sv[0] = SV.toScalar();  // overall gain in first slot — others stay 1.
    } else if (SV.numel() == K + 1) {
        sv.resize(K + 1);
        for (std::size_t i = 0; i <= K; ++i) sv[i] = SV.elemAsDouble(i);
    } else {
        throw Error("ctf2zp: SV must be scalar or K+1 vector",
                    0, 0, "ctf2zp", "", "m:ctf2zp:invalidSVDims");
    }

    // Helper to extract row i of a matrix (or replicate if matrix is just
    // a scalar/vector representing the "all sections same" case).
    auto extractRow = [&](const Value &M, std::size_t i, std::size_t Krows) {
        const std::size_t cols = colsOf(M);
        Value row = Value::matrix(1, cols, ValueType::DOUBLE, mr);
        double *rd = row.doubleDataMut();
        if (M.dims().rows() == 1 && M.dims().cols() > 1) {
            // Vector input — used for all sections.
            for (std::size_t k = 0; k < cols; ++k) rd[k] = M.elemAsDouble(k);
        } else if (Krows == 1 && M.numel() == 1) {
            rd[0] = M.toScalar();
        } else {
            // Matrix: column-major, element at (i, k) = i + k * rows.
            const std::size_t rows = M.dims().rows();
            for (std::size_t k = 0; k < cols; ++k) rd[k] = M.elemAsDouble(i + k * rows);
        }
        return row;
    };

    std::vector<Complex> zeros, poles;
    double gainAccum = 1.0;
    for (std::size_t i = 0; i < K; ++i) {
        Value brow = extractRow(NUM, std::min(i, Knum - 1), Knum);
        Value arow = extractRow(DEN, std::min(i, Kden - 1), Kden);
        auto [zsec, psec, gsec] = tf2zpk(brow, arow, mr);
        const std::size_t nz = zsec.numel();
        const std::size_t np = psec.numel();
        // Append zeros/poles
        if (zsec.type() == ValueType::COMPLEX) {
            const Complex *zd = zsec.complexData();
            for (std::size_t k = 0; k < nz; ++k) zeros.push_back(zd[k]);
        } else {
            for (std::size_t k = 0; k < nz; ++k)
                zeros.push_back(Complex(zsec.elemAsDouble(k), 0.0));
        }
        if (psec.type() == ValueType::COMPLEX) {
            const Complex *pd = psec.complexData();
            for (std::size_t k = 0; k < np; ++k) poles.push_back(pd[k]);
        } else {
            for (std::size_t k = 0; k < np; ++k)
                poles.push_back(Complex(psec.elemAsDouble(k), 0.0));
        }
        gainAccum *= gsec;
    }
    // Multiply overall gain by prod(SV).
    double svProd = 1.0;
    for (double v : sv) svProd *= v;
    gainAccum *= svProd;

    // Build output Z, P as complex column vectors.
    Value Z = Value::matrix(zeros.size(), zeros.empty() ? 0 : 1,
                             ValueType::COMPLEX, mr);
    Value P = Value::matrix(poles.size(), poles.empty() ? 0 : 1,
                             ValueType::COMPLEX, mr);
    if (!zeros.empty())
        std::copy(zeros.begin(), zeros.end(), Z.complexDataMut());
    if (!poles.empty())
        std::copy(poles.begin(), poles.end(), P.complexDataMut());
    return {Z, P, gainAccum};
}

// ── scaleFilterSections (Phase 4.11) ──────────────────────────────────
// Distribute scale values across cascade-section numerators per
// MATLAB R2025b scaleFilterSections.m + scalectfnum.m.
//
// scalar SV:
//   numsv = |sv|^(1/K) * num
//   numsv[K-1, :] *= sign(sv)
// vector SV (length K+1):
//   numsv[k, :] = |sv[K]|^(1/K) * sv[k] * num[k, :]
//   numsv[K-1, :] *= sign(sv[K])
Value scaleFilterSections(const Value &CTFNum, const Value &SV, std::pmr::memory_resource *mr)
{
    const std::size_t K = (CTFNum.dims().rows() == 1 && CTFNum.dims().cols() > 1)
                           ? 1 : CTFNum.dims().rows();
    const std::size_t P = (CTFNum.dims().rows() == 1 && CTFNum.dims().cols() > 1)
                           ? CTFNum.dims().cols() : CTFNum.dims().cols();
    const std::size_t Nsv = SV.numel();
    if (Nsv != 1 && Nsv != K + 1)
        throw Error("scaleFilterSections: SV length must be 1 or K+1",
                    0, 0, "scaleFilterSections", "",
                    "m:scaleFilterSections:invalidNumberOfScaleValues");

    // Early out: all-ones SV → return CTFNum unchanged (fresh copy).
    bool allOnes = true;
    for (std::size_t i = 0; i < Nsv; ++i)
        if (SV.elemAsDouble(i) != 1.0) { allOnes = false; break; }
    if (allOnes) {
        Value out = Value::matrix(K, P, ValueType::DOUBLE, mr);
        std::copy(CTFNum.doubleData(), CTFNum.doubleData() + K * P,
                  out.doubleDataMut());
        return out;
    }

    Value out = Value::matrix(K, P, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *Nd = CTFNum.doubleData();
    const double Kd = static_cast<double>(K);

    if (Nsv == 1) {
        const double s = SV.toScalar();
        const double absRoot = std::pow(std::abs(s), 1.0 / Kd);
        const double sgn = (s > 0.0) ? 1.0 : (s < 0.0 ? -1.0 : 0.0);
        for (std::size_t k = 0; k < K; ++k) {
            const double rowMul = (k == K - 1) ? sgn * absRoot : absRoot;
            for (std::size_t j = 0; j < P; ++j)
                od[k + j * K] = Nd[k + j * K] * rowMul;
        }
    } else {
        const double svLast = SV.elemAsDouble(K);
        const double absRoot = std::pow(std::abs(svLast), 1.0 / Kd);
        const double sgnLast = (svLast > 0.0) ? 1.0 : (svLast < 0.0 ? -1.0 : 0.0);
        for (std::size_t k = 0; k < K; ++k) {
            const double svk = SV.elemAsDouble(k);
            double mul = absRoot * svk;
            if (k == K - 1) mul *= sgnLast;
            for (std::size_t j = 0; j < P; ++j)
                od[k + j * K] = Nd[k + j * K] * mul;
        }
    }
    return out;
}

namespace detail {

void sos2tf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sos2tf: requires at least 1 argument (sos)",
                     0, 0, "sos2tf", "", "m:sos2tf:nargin");
    const double g = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    auto [b, a] = sos2tf(args[0], g, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void sos2zp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sos2zp: requires at least 1 argument (sos)",
                     0, 0, "sos2zp", "", "m:sos2zp:nargin");
    const double g = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    auto [z, p, gain] = sos2zp(args[0], g, ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = Value::scalar(gain, ctx.engine->resource());
}

void tf2zpk_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf2zpk: requires (b, a)",
                     0, 0, "tf2zpk", "", "m:tf2zpk:nargin");
    auto [z, p, gain] = tf2zpk(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = Value::scalar(gain, ctx.engine->resource());
}

void tf2ss_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf2ss: requires (b, a)",
                     0, 0, "tf2ss", "", "m:tf2ss:nargin");
    auto [A, B, C, D] = tf2ss(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(A);
    if (nargout > 1) outs[1] = std::move(B);
    if (nargout > 2) outs[2] = std::move(C);
    if (nargout > 3) outs[3] = std::move(D);
}

void ss2tf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss2tf: requires (A, B, C, D)",
                     0, 0, "ss2tf", "", "m:ss2tf:nargin");
    const double D = (args[3].numel() == 0) ? 0.0 : args[3].elemAsDouble(0);
    auto [b, a] = ss2tf(args[0], args[1], args[2], D, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void ss2zp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss2zp: requires (A, B, C, D)",
                     0, 0, "ss2zp", "", "m:ss2zp:nargin");
    const double D = (args[3].numel() == 0) ? 0.0 : args[3].elemAsDouble(0);
    auto [z, p, gain] = ss2zp(args[0], args[1], args[2], D, ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = Value::scalar(gain, ctx.engine->resource());
}

void zp2ss_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("zp2ss: requires (z, p, k)",
                     0, 0, "zp2ss", "", "m:zp2ss:nargin");
    auto [A, B, C, D] = zp2ss(args[0], args[1], args[2].toScalar(), ctx.engine->resource());
    outs[0] = std::move(A);
    if (nargout > 1) outs[1] = std::move(B);
    if (nargout > 2) outs[2] = std::move(C);
    if (nargout > 3) outs[3] = std::move(D);
}

void sos2ss_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sos2ss: requires at least 1 argument (sos)",
                     0, 0, "sos2ss", "", "m:sos2ss:nargin");
    const double g = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    auto [A, B, C, D] = sos2ss(args[0], g, ctx.engine->resource());
    outs[0] = std::move(A);
    if (nargout > 1) outs[1] = std::move(B);
    if (nargout > 2) outs[2] = std::move(C);
    if (nargout > 3) outs[3] = std::move(D);
}

void ss2sos_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss2sos: requires (A, B, C, D)",
                     0, 0, "ss2sos", "", "m:ss2sos:nargin");
    const double D = (args[3].numel() == 0) ? 0.0 : args[3].elemAsDouble(0);
    outs[0] = ss2sos(args[0], args[1], args[2], D, ctx.engine->resource());
}

void ctf2zp_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ctf2zp: requires (NUM [, DEN [, SV]])",
                    0, 0, "ctf2zp", "", "m:ctf2zp:nargin");
    auto *mr = ctx.engine->resource();
    Value DEN = (args.size() >= 2) ? args[1] : Value::scalar(1.0, mr);
    const Value &SV = (args.size() >= 3) ? args[2] : Value::Empty;
    auto [Z, P, k] = ctf2zp(args[0], DEN, SV, mr);
    outs[0] = std::move(Z);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(P);
    if (nargout >= 3 && outs.size() >= 3) outs[2] = Value::scalar(k, mr);
}

void scaleFilterSections_reg(Span<const Value> args, size_t /*nargout*/,
                              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("scaleFilterSections: requires (CTFNum, SV)",
                    0, 0, "scaleFilterSections", "",
                    "m:scaleFilterSections:nargin");
    outs[0] = scaleFilterSections(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
