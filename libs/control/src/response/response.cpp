// libs/control/src/response/response.cpp
//
// step / impulse / lsim — time-domain responses.
//
// For a continuous SISO model x' = Ax + Bu, y = Cx + Du, we
// discretise under a zero-order hold:
//     A_d = exp(A·dt),
//     B_d = integral_0^dt exp(A·τ) dτ · B
//         = first-block of exp([A B; 0 0]·dt)  (Van Loan).
// The matrix exponential uses 6th-order Padé with scaling and
// squaring — adequate for the well-conditioned A's seen in
// textbook control problems and avoids any external linalg dep.
//
// For a discrete model (Ts > 0) we just iterate the recurrence as-is.
// The simulation grid for continuous mode defaults to ≈ 8 / |Re λ_min|
// when the user passes no time argument; otherwise t is interpreted
// as either tFinal (scalar) or the explicit grid (vector).

#include <numkit/control/response/response.hpp>
#include <numkit/control/lti/lti.hpp>
#include <numkit/control/conversion/conversion.hpp>
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

using Mat = std::vector<double>;     // square, column-major
using Vec = std::vector<double>;

Mat zeros(size_t r, size_t c) { return Mat(r * c, 0.0); }
Mat eye(size_t n) { Mat I(n * n, 0.0); for (size_t i = 0; i < n; ++i) I[i * n + i] = 1.0; return I; }

Mat matmul(const Mat &A, size_t Ar, size_t Ac,
           const Mat &B, size_t Br, size_t Bc)
{
    (void)Br;
    Mat C(Ar * Bc, 0.0);
    for (size_t j = 0; j < Bc; ++j)
        for (size_t i = 0; i < Ar; ++i) {
            double s = 0.0;
            for (size_t k = 0; k < Ac; ++k)
                s += A[k * Ar + i] * B[j * Ac + k];
            C[j * Ar + i] = s;
        }
    return C;
}

double matInfNorm(const Mat &A, size_t n) {
    double m = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double s = 0.0;
        for (size_t j = 0; j < n; ++j) s += std::abs(A[j * n + i]);
        m = std::max(m, s);
    }
    return m;
}

// 4×4 LU + back-substitution; small `n` only (Padé denom solve).
// Solves A·X = B in-place in B (B has nrhs columns), stored col-major.
// Returns false on singular (caller treats as degenerate).
bool solveInPlace(Mat &A, Mat &B, size_t n, size_t nrhs)
{
    std::vector<size_t> piv(n);
    for (size_t i = 0; i < n; ++i) piv[i] = i;
    for (size_t k = 0; k < n; ++k) {
        // Partial pivot.
        size_t pk = k;
        double bestAbs = std::abs(A[k * n + k]);
        for (size_t i = k + 1; i < n; ++i) {
            double v = std::abs(A[k * n + i]);
            if (v > bestAbs) { bestAbs = v; pk = i; }
        }
        if (bestAbs < 1e-14) return false;
        if (pk != k) {
            // swap rows k and pk in A and B.
            for (size_t j = 0; j < n; ++j)
                std::swap(A[j * n + k], A[j * n + pk]);
            for (size_t j = 0; j < nrhs; ++j)
                std::swap(B[j * n + k], B[j * n + pk]);
            std::swap(piv[k], piv[pk]);
        }
        const double diag = A[k * n + k];
        for (size_t i = k + 1; i < n; ++i) {
            const double f = A[k * n + i] / diag;
            A[k * n + i] = f;  // L (below-diag)
            for (size_t j = k + 1; j < n; ++j)
                A[j * n + i] -= f * A[j * n + k];
            for (size_t j = 0; j < nrhs; ++j)
                B[j * n + i] -= f * B[j * n + k];
        }
    }
    // Back-substitute.
    for (size_t j = 0; j < nrhs; ++j) {
        for (size_t i = n; i-- > 0;) {
            double s = B[j * n + i];
            for (size_t k = i + 1; k < n; ++k)
                s -= A[k * n + i] * B[j * n + k];
            B[j * n + i] = s / A[i * n + i];
        }
    }
    return true;
}

// 6th-order Padé approximation of exp(A) with scaling and squaring.
// Algorithm 11.3.1 from Golub & Van Loan, simplified for moderate ‖A‖.
Mat expm(const Mat &Ain, size_t n)
{
    if (n == 0) return Mat{};
    Mat A = Ain;
    const double normA = matInfNorm(A, n);
    int s = 0;
    if (normA > 0.5) {
        const double l2 = std::log2(normA / 0.5);
        s = static_cast<int>(std::ceil(std::max(l2, 0.0)));
    }
    if (s > 0) {
        const double scale = std::pow(0.5, s);
        for (auto &v : A) v *= scale;
    }
    // Padé coefficients for q = 6.
    static const double cN[7] = {
        1.0, 0.5, 12.0/110.0, 2.0/110.0,
        2.0/3960.0, 1.0/166320.0, 1.0/665280.0
    };
    Mat U = zeros(n, n);
    Mat V = zeros(n, n);
    Mat I = eye(n);
    Mat A2 = matmul(A, n, n, A, n, n);
    Mat A4 = matmul(A2, n, n, A2, n, n);
    Mat A6 = matmul(A4, n, n, A2, n, n);
    // p(A) and q(A) for [6/6] Padé:
    //   q = sum_{k=0}^{6}  c_k A^k  with  c_2k from cN
    //   p = sum_{k=0}^{6} (-1)^k c_k A^k
    // We'll just compute with explicit coefficients.
    // Simpler: use the standard m=6 method via direct evaluation.
    // Coefficients b_k for [6/6] (from Higham):
    static const double b[7] = {
        720.0, 360.0, 120.0, 30.0, 6.0, 1.0, 0.0  // not used directly
    };
    (void)b; (void)cN;
    // Direct computation using standard formula:
    //   N(A) = c0 I + c1 A  + c2 A^2  + c3 A^3  + c4 A^4  + c5 A^5  + c6 A^6
    //   D(A) = c0 I − c1 A  + c2 A^2  − c3 A^3  + c4 A^4  − c5 A^5  + c6 A^6
    //   exp(A) ≈ D(A)^{-1} N(A)
    // For the [6/6] Padé, c_k = (12-k)! / k! / (12)! is rounded for
    // small matrices; the canonical normalised set is:
    // Canonical [6/6] Padé coefficients for exp(x):
    //   c_k = (12 − k)! · 6! / (12! · k! · (6 − k)!)
    static const double c[7] = {
        1.0,
        1.0/2.0,
        5.0/44.0,
        1.0/66.0,
        1.0/792.0,
        1.0/15840.0,
        1.0/665280.0
    };
    auto axpyMat = [&](Mat &dst, const Mat &src, double alpha) {
        for (size_t i = 0; i < n * n; ++i) dst[i] += alpha * src[i];
    };
    Mat A3 = matmul(A2, n, n, A, n, n);
    Mat A5 = matmul(A4, n, n, A, n, n);

    // N = c0 I + c1 A + c2 A^2 + c3 A^3 + c4 A^4 + c5 A^5 + c6 A^6
    Mat N = zeros(n, n);
    axpyMat(N, I,  c[0]);
    axpyMat(N, A,  c[1]);
    axpyMat(N, A2, c[2]);
    axpyMat(N, A3, c[3]);
    axpyMat(N, A4, c[4]);
    axpyMat(N, A5, c[5]);
    axpyMat(N, A6, c[6]);
    // D = c0 I − c1 A + c2 A^2 − c3 A^3 + c4 A^4 − c5 A^5 + c6 A^6
    Mat D = zeros(n, n);
    axpyMat(D, I,   c[0]);
    axpyMat(D, A,  -c[1]);
    axpyMat(D, A2,  c[2]);
    axpyMat(D, A3, -c[3]);
    axpyMat(D, A4,  c[4]);
    axpyMat(D, A5, -c[5]);
    axpyMat(D, A6,  c[6]);
    // Solve D · X = N for X.
    Mat Dcopy = D;
    Mat X = N;
    if (!solveInPlace(Dcopy, X, n, n)) {
        // Singular Padé denominator: fall back to truncated series.
        Mat E = I;
        Mat term = I;
        for (int k = 1; k < 30; ++k) {
            term = matmul(term, n, n, A, n, n);
            const double inv = 1.0 / static_cast<double>(k);
            for (auto &v : term) v *= inv;
            for (size_t i = 0; i < n * n; ++i) E[i] += term[i];
        }
        X = E;
    }
    // Squaring step.
    for (int k = 0; k < s; ++k) X = matmul(X, n, n, X, n, n);
    return X;
}

// Bring an LTI sys (any kind) into ss-form coefficient buffers.
struct SS {
    size_t n;          // state dim
    Mat A;             // n×n
    Vec B;             // n
    Vec C;             // n
    double D;          // SISO
    double Ts;
};

SS toSSiso(std::pmr::memory_resource *mr, const Value &sys) {
    Value Av, Bv, Cv, Dv;
    double Ts = 0.0;
    if (sys.isStruct() && sys.hasField("kind")) {
        Ts = sys.field("Ts").toScalar();
        const std::string k = sys.field("kind").toString();
        if (k == "ss") {
            Av = sys.field("A"); Bv = sys.field("B");
            Cv = sys.field("C"); Dv = sys.field("D");
        } else if (k == "tf") {
            tf2ss(mr, sys.field("num"), sys.field("den"),
                  &Av, &Bv, &Cv, &Dv);
        } else if (k == "zpk") {
            Value num, den;
            zp2tf(mr, sys.field("z"), sys.field("p"), sys.field("k"),
                  &num, &den);
            tf2ss(mr, num, den, &Av, &Bv, &Cv, &Dv);
        } else {
            throw Error("control response: unknown LTI kind",
                        0, 0, "response", "", "m:control:kind");
        }
    } else {
        throw Error("control response: expected an LTI struct",
                    0, 0, "response", "", "m:control:kind");
    }

    SS s;
    s.n = Av.dims().rows();
    s.Ts = Ts;
    s.A.resize(s.n * s.n);
    for (size_t i = 0; i < s.n * s.n; ++i) s.A[i] = Av.elemAsDouble(i);
    s.B.resize(s.n);
    for (size_t i = 0; i < s.n; ++i) s.B[i] = Bv.elemAsDouble(i);
    s.C.resize(s.n);
    // C is 1×n — column-major elem 0..n-1 are exactly the row.
    for (size_t i = 0; i < s.n; ++i) s.C[i] = Cv.elemAsDouble(i * 1 + 0);
    s.D = (Dv.numel() == 0) ? 0.0 : Dv.toScalar();
    return s;
}

// Van Loan ZOH: M = [[A,B],[0,0]], expm(M*dt) → top-left A_d, top-right B_d.
void zohDiscretise(const Mat &A, const Vec &B, size_t n, double dt,
                   Mat &Ad, Vec &Bd)
{
    const size_t m = n + 1;
    Mat M = zeros(m, m);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            M[j * m + i] = A[j * n + i] * dt;
    for (size_t i = 0; i < n; ++i)
        M[n * m + i] = B[i] * dt;
    Mat E = expm(M, m);
    Ad.assign(n * n, 0.0);
    Bd.assign(n, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            Ad[j * n + i] = E[j * m + i];
    for (size_t i = 0; i < n; ++i) Bd[i] = E[n * m + i];
}

// Decide a default time grid for continuous models: ~8 time
// constants of the slowest stable mode, ~50 samples per slowest pole.
struct Grid { Vec t; double dt; };

Grid pickGrid(std::pmr::memory_resource *mr, const Value &sys) {
    // Find pole with smallest |real|, use that to set Tfinal; fastest
    // pole sets dt.
    Value pV = pole(mr, sys);
    const size_t np = pV.numel();
    double minAbsReal = 1.0;
    double maxAbsReal = 1.0;
    if (np > 0) {
        std::vector<std::complex<double>> ps(np);
        if (pV.type() == ValueType::COMPLEX) {
            const std::complex<double> *src = pV.complexData();
            for (size_t i = 0; i < np; ++i) ps[i] = src[i];
        } else {
            for (size_t i = 0; i < np; ++i)
                ps[i] = std::complex<double>(pV.elemAsDouble(i), 0.0);
        }
        bool anyStable = false;
        for (auto &p : ps) {
            const double ar = std::abs(p.real());
            const double mag = std::abs(p);
            if (p.real() < 0.0 && mag > 0.0) {
                if (!anyStable || ar < minAbsReal) minAbsReal = ar;
                anyStable = true;
            }
            if (mag > maxAbsReal) maxAbsReal = mag;
        }
        if (!anyStable) minAbsReal = 1.0;
    }
    const double Tfinal = 8.0 / minAbsReal;
    const double dt = std::min(Tfinal / 200.0, 1.0 / (4.0 * maxAbsReal));
    const size_t N = static_cast<size_t>(std::ceil(Tfinal / dt)) + 1;
    Vec t(N);
    for (size_t i = 0; i < N; ++i) t[i] = i * dt;
    return {t, dt};
}

Vec readTimeArg(std::pmr::memory_resource *mr,
                const Value &sys, const Value &tArg)
{
    if (tArg.numel() == 0) {
        return pickGrid(mr, sys).t;
    }
    if (tArg.numel() == 1) {
        const double Tfinal = tArg.toScalar();
        const Grid g = pickGrid(mr, sys);
        // Re-tile at the same dt up to user's Tfinal.
        const double dt = g.dt;
        const size_t N = static_cast<size_t>(std::ceil(Tfinal / dt)) + 1;
        Vec t(N);
        for (size_t i = 0; i < N; ++i) t[i] = i * dt;
        return t;
    }
    Vec t(tArg.numel());
    for (size_t i = 0; i < tArg.numel(); ++i) t[i] = tArg.elemAsDouble(i);
    return t;
}

Value rowFromVec(std::pmr::memory_resource *mr, const Vec &v) {
    Value r = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

// Run the discrete simulation for a SISO system on input u[0..N-1]
// (length N matches t). Returns y[0..N-1].
Vec simulate(const SS &sys, const Vec &t, const Vec &u, const Vec &x0)
{
    const size_t N = t.size();
    const size_t n = sys.n;
    Vec x = x0;
    if (x.size() != n) x.assign(n, 0.0);
    Vec y(N, 0.0);

    if (sys.Ts > 0.0) {
        // Already discrete.
        for (size_t k = 0; k < N; ++k) {
            double yk = sys.D * u[k];
            for (size_t i = 0; i < n; ++i) yk += sys.C[i] * x[i];
            y[k] = yk;
            // x_{k+1} = A·x + B·u
            Vec xn(n, 0.0);
            for (size_t i = 0; i < n; ++i) {
                double s = sys.B[i] * u[k];
                for (size_t j = 0; j < n; ++j)
                    s += sys.A[j * n + i] * x[j];
                xn[i] = s;
            }
            x = std::move(xn);
        }
        return y;
    }

    // Continuous: ZOH-discretise per actual dt. If t is uniform, do
    // one expm; otherwise re-discretise per step (more expensive but
    // robust).
    bool uniform = true;
    double dt = (N >= 2) ? (t[1] - t[0]) : 0.0;
    for (size_t k = 2; k < N && uniform; ++k)
        if (std::abs((t[k] - t[k - 1]) - dt) > 1e-12 * std::max(1.0, std::abs(dt)))
            uniform = false;

    if (uniform && dt > 0.0) {
        Mat Ad; Vec Bd;
        zohDiscretise(sys.A, sys.B, n, dt, Ad, Bd);
        for (size_t k = 0; k < N; ++k) {
            double yk = sys.D * u[k];
            for (size_t i = 0; i < n; ++i) yk += sys.C[i] * x[i];
            y[k] = yk;
            if (k + 1 < N) {
                Vec xn(n, 0.0);
                for (size_t i = 0; i < n; ++i) {
                    double s = Bd[i] * u[k];
                    for (size_t j = 0; j < n; ++j)
                        s += Ad[j * n + i] * x[j];
                    xn[i] = s;
                }
                x = std::move(xn);
            }
        }
    } else {
        // Non-uniform grid: per-step expm.
        for (size_t k = 0; k < N; ++k) {
            double yk = sys.D * u[k];
            for (size_t i = 0; i < n; ++i) yk += sys.C[i] * x[i];
            y[k] = yk;
            if (k + 1 < N) {
                const double dtk = t[k + 1] - t[k];
                Mat Ad; Vec Bd;
                zohDiscretise(sys.A, sys.B, n, dtk, Ad, Bd);
                Vec xn(n, 0.0);
                for (size_t i = 0; i < n; ++i) {
                    double s = Bd[i] * u[k];
                    for (size_t j = 0; j < n; ++j)
                        s += Ad[j * n + i] * x[j];
                    xn[i] = s;
                }
                x = std::move(xn);
            }
        }
    }
    return y;
}

} // anonymous

void step_response(std::pmr::memory_resource *mr,
                   const Value &sys, const Value &tArg,
                   Value *yOut, Value *tOut)
{
    SS s = toSSiso(mr, sys);
    Vec t = readTimeArg(mr, sys, tArg);
    Vec u(t.size(), 1.0);   // unit step
    Vec x0(s.n, 0.0);
    Vec y = simulate(s, t, u, x0);
    if (yOut) *yOut = rowFromVec(mr, y);
    if (tOut) *tOut = rowFromVec(mr, t);
}

void impulse_response(std::pmr::memory_resource *mr,
                      const Value &sys, const Value &tArg,
                      Value *yOut, Value *tOut)
{
    SS s = toSSiso(mr, sys);
    Vec t = readTimeArg(mr, sys, tArg);
    Vec x0(s.n, 0.0);
    Vec u(t.size(), 0.0);

    if (s.Ts > 0.0) {
        // Discrete: u[0] = 1, then 0.
        if (!u.empty()) u[0] = 1.0;
    } else {
        // Continuous: impulse ⇒ x(0+) = B (after differentiating
        // through the delta in u(t)). Simulate with u ≡ 0 and IC = B.
        x0 = s.B;
    }
    Vec y = simulate(s, t, u, x0);
    if (yOut) *yOut = rowFromVec(mr, y);
    if (tOut) *tOut = rowFromVec(mr, t);
}

Value lsim(std::pmr::memory_resource *mr,
           const Value &sys, const Value &uIn, const Value &tIn,
           const Value &x0In)
{
    SS s = toSSiso(mr, sys);
    if (tIn.numel() < 2)
        throw Error("lsim: t must have at least 2 samples",
                    0, 0, "lsim", "", "m:lsim:t");
    Vec t(tIn.numel());
    for (size_t i = 0; i < tIn.numel(); ++i) t[i] = tIn.elemAsDouble(i);
    if (uIn.numel() != t.size())
        throw Error("lsim: u and t must be the same length",
                    0, 0, "lsim", "", "m:lsim:size");
    Vec u(t.size());
    for (size_t i = 0; i < t.size(); ++i) u[i] = uIn.elemAsDouble(i);
    Vec x0(s.n, 0.0);
    if (x0In.numel() == s.n)
        for (size_t i = 0; i < s.n; ++i) x0[i] = x0In.elemAsDouble(i);
    Vec y = simulate(s, t, u, x0);
    return rowFromVec(mr, y);
}

namespace detail {

void step_reg(Span<const Value> a, size_t /*nargout*/, Span<Value> outs,
              CallContext &c)
{
    if (a.empty())
        throw Error("step: requires (sys [, t])",
                    0, 0, "step", "", "m:step:nargin");
    Value y, t;
    Value tArg = (a.size() >= 2) ? a[1] : Value::matrix(1, 0, ValueType::DOUBLE, c.engine->resource());
    step_response(c.engine->resource(), a[0], tArg, &y, &t);
    if (outs.size() >= 1) outs[0] = y;
    if (outs.size() >= 2) outs[1] = t;
}

void impulse_reg(Span<const Value> a, size_t /*nargout*/, Span<Value> outs,
                 CallContext &c)
{
    if (a.empty())
        throw Error("impulse: requires (sys [, t])",
                    0, 0, "impulse", "", "m:impulse:nargin");
    Value y, t;
    Value tArg = (a.size() >= 2) ? a[1] : Value::matrix(1, 0, ValueType::DOUBLE, c.engine->resource());
    impulse_response(c.engine->resource(), a[0], tArg, &y, &t);
    if (outs.size() >= 1) outs[0] = y;
    if (outs.size() >= 2) outs[1] = t;
}

void lsim_reg(Span<const Value> a, size_t /*nargout*/, Span<Value> outs,
              CallContext &c)
{
    if (a.size() < 3)
        throw Error("lsim: requires (sys, u, t [, x0])",
                    0, 0, "lsim", "", "m:lsim:nargin");
    Value x0 = (a.size() >= 4) ? a[3] : Value::matrix(0, 0, ValueType::DOUBLE, c.engine->resource());
    outs[0] = lsim(c.engine->resource(), a[0], a[1], a[2], x0);
}

} // namespace detail

} // namespace numkit::control
