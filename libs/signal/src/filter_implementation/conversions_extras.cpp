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

Value rowFromVec(std::pmr::memory_resource *mr, const std::vector<double> &v)
{
    auto out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    for (size_t i = 0; i < v.size(); ++i) dst[i] = v[i];
    return out;
}

// Append row's roots (as Complex) to the running list.
void appendRoots(std::pmr::memory_resource *mr,
                 const double *coeffs, size_t n,
                 std::vector<Complex> &out)
{
    // Build a 1×n DOUBLE Value to feed roots(). Trim trailing zeros so
    // roots() doesn't add spurious roots at 0.
    std::vector<double> trimmed(coeffs, coeffs + n);
    trimTrailingZeros(trimmed);
    if (trimmed.size() < 2) return;
    auto p = Value::matrix(1, trimmed.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < trimmed.size(); ++i)
        p.doubleDataMut()[i] = trimmed[i];
    auto r = builtin::roots(mr, p);
    if (r.isComplex()) {
        const Complex *src = r.complexData();
        for (size_t i = 0; i < r.numel(); ++i) out.push_back(src[i]);
    } else {
        const double *src = r.doubleData();
        for (size_t i = 0; i < r.numel(); ++i) out.emplace_back(src[i], 0.0);
    }
}

Value complexColFromVec(std::pmr::memory_resource *mr, const std::vector<Complex> &v)
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
sos2tf(std::pmr::memory_resource *mr, const Value &sos, double g)
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
    return std::make_tuple(rowFromVec(mr, b), rowFromVec(mr, a));
}

// ── sos2zp ────────────────────────────────────────────────────────────
std::tuple<Value, Value, double>
sos2zp(std::pmr::memory_resource *mr, const Value &sos, double g)
{
    requireSosShape(sos, "sos2zp");
    const size_t L = sos.dims().rows();
    std::vector<Complex> zeros, poles;
    double gain = g;
    for (size_t i = 0; i < L; ++i) {
        const double bArr[3] = {sos(i, 0), sos(i, 1), sos(i, 2)};
        const double aArr[3] = {sos(i, 3), sos(i, 4), sos(i, 5)};
        appendRoots(mr, bArr, 3, zeros);
        appendRoots(mr, aArr, 3, poles);
        // Per-section gain factor: b0 / a0 for that section.
        if (std::abs(aArr[0]) > 1e-15)
            gain *= bArr[0] / aArr[0];
    }
    return std::make_tuple(complexColFromVec(mr, zeros),
                            complexColFromVec(mr, poles),
                            gain);
}

// ── tf2zpk ────────────────────────────────────────────────────────────
std::tuple<Value, Value, double>
tf2zpk(std::pmr::memory_resource *mr, const Value &b, const Value &a)
{
    auto [z, p, k] = builtin::tf2zp(mr, b, a);
    return std::make_tuple(std::move(z), std::move(p),
                            (k.numel() == 0) ? 0.0 : k.toScalar());
}

// ── tf2ss (controllable canonical form) ───────────────────────────────
std::tuple<Value, Value, Value, Value>
tf2ss(std::pmr::memory_resource *mr, const Value &b, const Value &a)
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
    //                                  to length N+1, this is the leading coeff).
    const double D = bh[0];

    // A: companion matrix.
    //   A(0:N-2, 1:N-1) = identity
    //   A(N-1, :) = -[a(N), a(N-1), ..., a(1)] reversed → -ah[N..1]
    auto AVal = Value::matrix(N, N, ValueType::DOUBLE, mr);
    double *Ap = AVal.doubleDataMut();
    std::fill(Ap, Ap + N * N, 0.0);
    // Identity sub-block (rows 0..N-2, cols 1..N-1).
    for (size_t i = 0; i + 1 < N; ++i)
        Ap[(i + 1) * N + i] = 1.0;     // Ap[r + c*N], column-major
    // Last row.
    for (size_t j = 0; j < N; ++j)
        Ap[(j) * N + (N - 1)] = -ah[N - j];

    // B: column vector with 1 in last entry.
    auto BVal = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *Bp = BVal.doubleDataMut();
    std::fill(Bp, Bp + N, 0.0);
    Bp[N - 1] = 1.0;

    // C: 1×N row, c[j] = b[N-j] - a[N-j] * D     (j = 1..N)
    auto CVal = Value::matrix(1, N, ValueType::DOUBLE, mr);
    double *Cp = CVal.doubleDataMut();
    for (size_t j = 0; j < N; ++j)
        Cp[j] = bh[N - j] - ah[N - j] * D;

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
ss2tf(std::pmr::memory_resource *mr, const Value &A, const Value &B,
       const Value &C, const Value &D)
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

    const double Dscalar = (D.numel() == 0) ? 0.0 : D.elemAsDouble(0);

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

    return std::make_tuple(rowFromVec(mr, b_coeffs), rowFromVec(mr, a_coeffs));
}

// ── ss2zp ─────────────────────────────────────────────────────────────
std::tuple<Value, Value, double>
ss2zp(std::pmr::memory_resource *mr, const Value &A, const Value &B,
       const Value &C, const Value &D)
{
    auto [b, a] = ss2tf(mr, A, B, C, D);
    return tf2zpk(mr, b, a);
}

// ── zp2ss ─────────────────────────────────────────────────────────────
std::tuple<Value, Value, Value, Value>
zp2ss(std::pmr::memory_resource *mr, const Value &z, const Value &p, double k)
{
    auto [b, a] = builtin::zp2tf(mr, z, p, k);
    return tf2ss(mr, b, a);
}

// ── sos2ss ────────────────────────────────────────────────────────────
std::tuple<Value, Value, Value, Value>
sos2ss(std::pmr::memory_resource *mr, const Value &sos, double g)
{
    auto [b, a] = sos2tf(mr, sos, g);
    return tf2ss(mr, b, a);
}

// ── ss2sos ────────────────────────────────────────────────────────────
Value ss2sos(std::pmr::memory_resource *mr, const Value &A, const Value &B,
              const Value &C, const Value &D)
{
    auto [b, a] = ss2tf(mr, A, B, C, D);
    return tf2sos(mr, b, a);
}

namespace detail {

void sos2tf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sos2tf: requires at least 1 argument (sos)",
                     0, 0, "sos2tf", "", "m:sos2tf:nargin");
    const double g = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    auto [b, a] = sos2tf(ctx.engine->resource(), args[0], g);
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void sos2zp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sos2zp: requires at least 1 argument (sos)",
                     0, 0, "sos2zp", "", "m:sos2zp:nargin");
    const double g = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    auto [z, p, gain] = sos2zp(ctx.engine->resource(), args[0], g);
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = Value::scalar(gain, ctx.engine->resource());
}

void tf2zpk_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf2zpk: requires (b, a)",
                     0, 0, "tf2zpk", "", "m:tf2zpk:nargin");
    auto [z, p, gain] = tf2zpk(ctx.engine->resource(), args[0], args[1]);
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = Value::scalar(gain, ctx.engine->resource());
}

void tf2ss_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf2ss: requires (b, a)",
                     0, 0, "tf2ss", "", "m:tf2ss:nargin");
    auto [A, B, C, D] = tf2ss(ctx.engine->resource(), args[0], args[1]);
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
    auto [b, a] = ss2tf(ctx.engine->resource(), args[0], args[1], args[2], args[3]);
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void ss2zp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss2zp: requires (A, B, C, D)",
                     0, 0, "ss2zp", "", "m:ss2zp:nargin");
    auto [z, p, gain] = ss2zp(ctx.engine->resource(), args[0], args[1], args[2], args[3]);
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = Value::scalar(gain, ctx.engine->resource());
}

void zp2ss_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("zp2ss: requires (z, p, k)",
                     0, 0, "zp2ss", "", "m:zp2ss:nargin");
    auto [A, B, C, D] = zp2ss(ctx.engine->resource(), args[0], args[1], args[2].toScalar());
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
    auto [A, B, C, D] = sos2ss(ctx.engine->resource(), args[0], g);
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
    outs[0] = ss2sos(ctx.engine->resource(), args[0], args[1], args[2], args[3]);
}

} // namespace detail

} // namespace numkit::signal
