// toolboxes/control/src/analyze/hinfnorm.cpp
//
// H-infinity norm of a continuous-time LTI system:
//     ‖G‖∞ = sup_ω σ_max(G(jω)),   G(s) = C(sI − A)⁻¹B + D.
//
// Computed by the Bruinsma–Steinbuch Hamiltonian test with bisection on
// γ (no frequency sweep — the test is exact). For a candidate γ form
//     R    = γ²I − DᵀD
//     Ā    = A + B R⁻¹ DᵀC
//     M(γ) = [  Ā            B R⁻¹ Bᵀ
//              −Cᵀ(I + D R⁻¹ Dᵀ)C   −Āᵀ ].
// γ is an UPPER bound on ‖G‖∞ iff M(γ) has NO purely imaginary
// eigenvalue (and R ≻ 0, i.e. γ > σ_max(D)). Bisect γ between a lower
// bracket (any frequency-point gain ≤ ‖G‖∞) and an upper bound found by
// doubling. Everything is real: M's spectrum comes from charPoly →
// math::roots (the same path `pole` uses); the σ_max seeds use a
// Rayleigh power iteration (always ≤ the true σ_max, so a valid lower
// bracket).
//
// Engine-free compute TU. The hinfnorm builtin (CallContext wrapper)
// lives in analyze_reg.cpp.

#include <numkit/control/analyze/analyze.hpp>
#include <numkit/control/conversion/conversion.hpp>
#include <numkit/control/internal/numerics.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/math/poly/polynomials.hpp>   // numkit::math::roots

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>

namespace numkit::control {

namespace {

using Mat = internal::Mat;            // column-major flat std::vector<double>
using Cd  = std::complex<double>;
using internal::solveInPlace;
using internal::charPoly;

bool hasKind(const Value &sys, const char *want) {
    if (!sys.isStruct() || !sys.hasField("kind")) return false;
    return sys.field("kind").toString() == want;
}

double sampleTime(const Value &sys) {
    if (sys.isStruct() && sys.hasField("Ts")) return sys.field("Ts").toScalar();
    return 0.0;
}

// (A, B, C, D) extraction — ss read directly, tf/zpk converted to ss.
StateSpace pullABCD(const Value &sys, std::pmr::memory_resource *mr) {
    StateSpace out;
    if (hasKind(sys, "ss")) {
        out.A = sys.field("A"); out.B = sys.field("B");
        out.C = sys.field("C"); out.D = sys.field("D");
    } else if (hasKind(sys, "tf")) {
        out = tf2ss(sys.field("num"), sys.field("den"), mr);
    } else if (hasKind(sys, "zpk")) {
        auto [num, den] = zp2tf(sys.field("z"), sys.field("p"), sys.field("k"), mr);
        out = tf2ss(num, den, mr);
    } else {
        throw Error("hinfnorm: expected an LTI struct (tf/zpk/ss)",
                    0, 0, "hinfnorm", "", "numkit:hinfnorm:kind");
    }
    return out;
}

Mat readMat(const Value &v, size_t r, size_t c) {
    Mat M(r * c, 0.0);
    for (size_t i = 0; i < r * c; ++i) M[i] = v.elemAsDouble(i);
    return M;
}

// C = A·B, A: ra×ca, B: rb×cb (ca == rb), result ra×cb column-major.
Mat matmul(const Mat &A, size_t ra, size_t ca, const Mat &B, size_t cb) {
    Mat C(ra * cb, 0.0);
    for (size_t j = 0; j < cb; ++j)
        for (size_t k = 0; k < ca; ++k) {
            const double b = B[j * ca + k];
            if (b == 0.0) continue;
            for (size_t i = 0; i < ra; ++i)
                C[j * ra + i] += A[k * ra + i] * b;
        }
    return C;
}

Mat transpose(const Mat &A, size_t r, size_t c) {
    Mat T(r * c, 0.0);
    for (size_t j = 0; j < c; ++j)
        for (size_t i = 0; i < r; ++i)
            T[i * c + j] = A[j * r + i];
    return T;
}

bool inverse(const Mat &A, size_t n, Mat &out) {
    Mat lu = A;
    out.assign(n * n, 0.0);
    for (size_t i = 0; i < n; ++i) out[i * n + i] = 1.0;
    return solveInPlace(lu, out, n, n);
}

// Largest singular value of a real r×c matrix via Rayleigh power
// iteration on the smaller Gram matrix. The Rayleigh quotient never
// exceeds λ_max, so the returned value is ≤ σ_max — a SAFE lower bracket.
double sigmaMax(const Mat &M, size_t r, size_t c) {
    if (r == 0 || c == 0) return 0.0;
    const size_t g = std::min(r, c);
    // Gram = MᵀM (c×c) if c≤r, else MMᵀ (r×r); take the g×g one.
    Mat G(g * g, 0.0);
    if (c <= r) {
        for (size_t a = 0; a < c; ++a)
            for (size_t b = 0; b < c; ++b) {
                double s = 0.0;
                for (size_t k = 0; k < r; ++k) s += M[a * r + k] * M[b * r + k];
                G[b * c + a] = s;   // (MᵀM)[a,b]
            }
    } else {
        for (size_t a = 0; a < r; ++a)
            for (size_t b = 0; b < r; ++b) {
                double s = 0.0;
                for (size_t k = 0; k < c; ++k) s += M[k * r + a] * M[k * r + b];
                G[b * r + a] = s;   // (MMᵀ)[a,b]
            }
    }
    std::vector<double> x(g, 1.0), y(g, 0.0);
    double lambda = 0.0;
    for (int it = 0; it < 200; ++it) {
        for (size_t i = 0; i < g; ++i) {
            double s = 0.0;
            for (size_t k = 0; k < g; ++k) s += G[k * g + i] * x[k];
            y[i] = s;
        }
        double num = 0.0, den = 0.0;
        for (size_t i = 0; i < g; ++i) { num += x[i] * y[i]; den += x[i] * x[i]; }
        if (den <= 0.0) break;
        lambda = num / den;                       // Rayleigh quotient ≤ λ_max
        double nrm = 0.0;
        for (size_t i = 0; i < g; ++i) nrm += y[i] * y[i];
        nrm = std::sqrt(nrm);
        if (nrm <= 0.0) break;
        for (size_t i = 0; i < g; ++i) x[i] = y[i] / nrm;
    }
    return std::sqrt(std::max(lambda, 0.0));
}

std::vector<Cd> eigOf(const Mat &M, size_t n, std::pmr::memory_resource *mr) {
    auto cp = charPoly(M, n);                    // [1, c1, …, cn]
    Value row = Value::matrix(1, cp.size(), ValueType::DOUBLE, mr);
    if (!cp.empty()) std::copy(cp.begin(), cp.end(), row.doubleDataMut());
    Value r = numkit::math::roots(row, mr);
    const size_t N = r.numel();
    std::vector<Cd> out(N);
    if (r.type() == ValueType::COMPLEX) {
        const Cd *src = r.complexData();
        for (size_t i = 0; i < N; ++i) out[i] = src[i];
    } else {
        for (size_t i = 0; i < N; ++i) out[i] = Cd(r.elemAsDouble(i), 0.0);
    }
    return out;
}

// Does the Hamiltonian M(γ) have a purely imaginary eigenvalue?
// (γ ≤ σ_max(D) ⇒ R not positive-definite ⇒ treat as "yes".)
bool hasImagEig(const Mat &A, const Mat &B, const Mat &C, const Mat &D,
                size_t n, size_t m, size_t p, double gamma, double sigD,
                std::pmr::memory_resource *mr)
{
    if (gamma <= sigD * (1.0 + 1e-12)) return true;

    Mat Dt = transpose(D, p, m);                 // m×p
    Mat DtD = matmul(Dt, m, p, D, m);            // m×m
    Mat R(m * m, 0.0);
    for (size_t i = 0; i < m * m; ++i) R[i] = -DtD[i];
    for (size_t i = 0; i < m; ++i) R[i * m + i] += gamma * gamma;
    Mat Ri;
    if (!inverse(R, m, Ri)) return true;         // singular → γ too small

    Mat Bt = transpose(B, n, m);                 // m×n
    Mat Ct = transpose(C, p, n);                 // n×p

    // Ā = A + B·Ri·Dᵀ·C
    Mat RiDt  = matmul(Ri, m, m, Dt, p);         // m×p
    Mat BRiDt = matmul(B, n, m, RiDt, p);        // n×p
    Mat Abar  = matmul(BRiDt, n, p, C, n);       // n×n
    for (size_t i = 0; i < n * n; ++i) Abar[i] += A[i];

    // B·Ri·Bᵀ
    Mat RiBt  = matmul(Ri, m, m, Bt, n);         // m×n
    Mat BRiBt = matmul(B, n, m, RiBt, n);        // n×n

    // Cᵀ·(I + D·Ri·Dᵀ)·C
    Mat DRi   = matmul(D, p, m, Ri, m);          // p×m
    Mat DRiDt = matmul(DRi, p, m, Dt, p);        // p×p
    for (size_t i = 0; i < p; ++i) DRiDt[i * p + i] += 1.0;
    Mat midC  = matmul(DRiDt, p, p, C, n);       // p×n
    Mat Cmid  = matmul(Ct, n, p, midC, n);       // n×n

    // Assemble M(γ) = [Ā, BRiBt; −Cmid, −Āᵀ]  (2n×2n column-major)
    const size_t k = 2 * n;
    Mat M(k * k, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i) {
            M[(j) * k + (i)]         =  Abar[j * n + i];
            M[(n + j) * k + (i)]     =  BRiBt[j * n + i];
            M[(j) * k + (n + i)]     = -Cmid[j * n + i];
            M[(n + j) * k + (n + i)] = -Abar[i * n + j];   // −Āᵀ
        }

    auto ev = eigOf(M, k, mr);
    double scale = 1.0;
    for (const auto &e : ev) scale = std::max(scale, std::abs(e));
    const double tol = 1e-7 * scale;
    for (const auto &e : ev)
        if (std::abs(e.real()) < tol && std::abs(e.imag()) > tol) return true;
    return false;
}

} // anonymous

Value hinfnorm(const Value &sys, std::pmr::memory_resource *mr)
{
    if (sampleTime(sys) != 0.0)
        throw Error("hinfnorm: discrete-time systems are not yet supported "
                    "(continuous-time only)",
                    0, 0, "hinfnorm", "", "numkit:hinfnorm:discrete");

    StateSpace ss = pullABCD(sys, mr);
    const size_t n = ss.A.dims().rows();
    const size_t m = ss.B.dims().cols();
    const size_t p = ss.C.dims().rows();
    if (ss.A.dims().cols() != n)
        throw Error("hinfnorm: A must be square",
                    0, 0, "hinfnorm", "", "numkit:hinfnorm:A");

    const double inf = std::numeric_limits<double>::infinity();
    Value g = Value::scalar(0.0, mr);

    // n == 0 (static gain): ‖G‖∞ = σ_max(D).
    auto A = (n ? readMat(ss.A, n, n) : Mat{});
    auto B = (n ? readMat(ss.B, n, m) : Mat{});
    auto C = (n ? readMat(ss.C, p, n) : Mat{});
    auto D = readMat(ss.D, p, m);
    const double sigD = sigmaMax(D, p, m);

    if (n == 0) { g = Value::scalar(sigD, mr); return g; }

    // Unstable / jω-axis poles → Inf.
    {
        auto ev = eigOf(A, n, mr);
        double maxRe = -inf;
        for (const auto &e : ev) maxRe = std::max(maxRe, e.real());
        if (maxRe >= -1e-9) { g = Value::scalar(inf, mr); return g; }
    }

    // Lower bracket: max frequency-point gain we can read cheaply —
    // the DC gain G(0) = D − C·A⁻¹·B and the ω→∞ gain D. Both ≤ ‖G‖∞.
    double lb = sigD;
    {
        Mat Ai;
        if (inverse(A, n, Ai)) {
            Mat AiB  = matmul(Ai, n, n, B, m);   // n×m
            Mat CAiB = matmul(C, p, n, AiB, m);  // p×m
            Mat G0(p * m, 0.0);
            for (size_t i = 0; i < p * m; ++i) G0[i] = D[i] - CAiB[i];
            lb = std::max(lb, sigmaMax(G0, p, m));
        }
    }

    // Bisect: lo a valid lower bracket (M(lo) has imag eig), hi an upper
    // bound found by doubling.
    double lo = lb;
    double hi = (lb > 0.0 ? lb : 1.0) * (1.0 + 1e-6) + 1e-300;
    int grow = 0;
    while (hasImagEig(A, B, C, D, n, m, p, hi, sigD, mr)) {
        hi *= 2.0;
        if (++grow > 80) { g = Value::scalar(inf, mr); return g; }
    }
    for (int it = 0; it < 200; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (mid <= lo || mid >= hi) break;       // converged to FP resolution
        if (hasImagEig(A, B, C, D, n, m, p, mid, sigD, mr)) lo = mid;
        else hi = mid;
        if (hi - lo <= 1e-12 * (1.0 + hi)) break;
    }
    g = Value::scalar(0.5 * (lo + hi), mr);
    return g;
}

} // namespace numkit::control
