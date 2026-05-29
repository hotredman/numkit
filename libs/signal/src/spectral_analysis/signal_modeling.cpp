// libs/signal/src/spectral_analysis/signal_modeling.cpp
//
// Parametric signal modelling: Levinson-Durbin recursion, Yule-Walker
// / Burg AR estimation, linear-prediction coefficients, plus the
// representation-conversion utilities the toolbox documents (poly ↔
// rc ↔ ac ↔ is ↔ lar). All implementations follow the standard real-
// data formulations; complex AR isn't covered here.

#include <numkit/signal/spectral_analysis/signal_modeling.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace numkit::signal {

namespace {

std::vector<double> readVec(const Value &x)
{
    const size_t n = x.numel();
    std::vector<double> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = x.elemAsDouble(i);
    return v;
}

Value rowVec(const std::vector<double> &v, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

Value colVec(const std::vector<double> &v, std::pmr::memory_resource *mr)
{
    auto out = Value::matrix(v.size(), 1, ValueType::DOUBLE, mr);
    if (!v.empty()) std::memcpy(out.doubleDataMut(), v.data(), v.size() * sizeof(double));
    return out;
}

// Biased autocorrelation r[k] = (1/N) * sum_{i=0..N-1-k} x[i] * x[i+k],
// for k in 0..maxLag. Returns vector of length maxLag+1.
std::vector<double> biasedAutocorr(const std::vector<double> &x, int maxLag)
{
    const size_t N = x.size();
    std::vector<double> R(maxLag + 1, 0.0);
    for (int k = 0; k <= maxLag; ++k) {
        double s = 0.0;
        for (size_t i = 0; i + k < N; ++i) s += x[i] * x[i + k];
        R[k] = s / static_cast<double>(N);
    }
    return R;
}

// Core Levinson-Durbin recursion. Given r (length p+1), produces:
//   a (length p+1, a[0] = 1)
//   e (final prediction error variance)
//   k (reflection coefficients, length p)
// Standard Yule-Walker form: a is the coefficient vector of the AR
// poly 1 + a[1]*z^-1 + ... + a[p]*z^-p such that the filter
// 1/A(z) models the input autocorrelation.
struct LevinsonResult { std::vector<double> a; double e; std::vector<double> k; };

LevinsonResult levinsonCore(const std::vector<double> &r, int p)
{
    LevinsonResult res;
    res.a.assign(p + 1, 0.0);
    res.k.assign(p, 0.0);
    res.a[0] = 1.0;
    if (p == 0) { res.e = (r.empty() ? 0.0 : r[0]); return res; }
    double e = r[0];
    if (e <= 0.0) { res.e = 0.0; return res; }
    std::vector<double> a_prev(p + 1, 0.0); a_prev[0] = 1.0;

    for (int i = 1; i <= p; ++i) {
        // Numerator of the i-th reflection coefficient.
        double num = r[i];
        for (int j = 1; j < i; ++j) num += a_prev[j] * r[i - j];
        // MATLAB's levinson runs the full recursion even when the
        // residual energy goes non-positive (a non-PSD autocorrelation,
        // i.e. an unstable model). It does NOT early-exit: e simply
        // climbs back up through the (1 - k^2) factor. Match that — only
        // guard the exact-zero divide. (For valid PSD input e stays > 0,
        // so this changes nothing for the well-posed case.)
        const double k = (e != 0.0) ? -num / e : 0.0;
        res.k[i - 1] = k;

        // Update a using a_prev: a_new[j] = a_prev[j] + k * a_prev[i-j]
        std::vector<double> a_new(p + 1, 0.0);
        a_new[0] = 1.0;
        for (int j = 1; j < i; ++j)
            a_new[j] = a_prev[j] + k * a_prev[i - j];
        a_new[i] = k;
        a_prev = std::move(a_new);
        e *= (1.0 - k * k);
    }
    res.a = a_prev;
    res.e = e;
    return res;
}

} // anonymous

// ── Levinson-Durbin ────────────────────────────────────────────────

std::tuple<Value, Value, Value>
levinson(const Value &r, int n, std::pmr::memory_resource *mr)
{
    auto rv = readVec(r);
    if (n < 0) n = static_cast<int>(rv.size()) - 1;
    if (n < 0) n = 0;
    if (n + 1 > static_cast<int>(rv.size())) rv.resize(n + 1, 0.0);
    auto res = levinsonCore(rv, n);
    return std::make_tuple(rowVec(res.a, mr),
                           Value::scalar(res.e, mr),
                           colVec(res.k, mr));
}

// rlevinson(a, e): given AR poly a and final prediction error e,
// recover the autocorrelation sequence + reflection coefficients via
// step-down. Returns (R, k).
std::tuple<Value, Value>
rlevinson(const Value &a, double efinal, std::pmr::memory_resource *mr)
{
    auto av = readVec(a);
    const int p = static_cast<int>(av.size()) - 1;
    std::vector<double> R(p + 1, 0.0);
    std::vector<double> k(p, 0.0);

    // Step-down to recover reflection coefficients k[0..p-1].
    auto cur = av;
    double e = efinal;
    for (int i = p; i >= 1; --i) {
        const double ki = cur[i];
        k[i - 1] = ki;
        if (std::abs(1.0 - ki * ki) < 1e-300) break;
        std::vector<double> prev(i + 1, 0.0);  // a_{i-1}
        prev[0] = 1.0;
        const double denom = 1.0 - ki * ki;
        for (int j = 1; j < i; ++j)
            prev[j] = (cur[j] - ki * cur[i - j]) / denom;
        cur = prev;
        e /= denom;  // step e back up
    }

    // Forward Levinson on the reflection coeffs, recovering R via
    // r[i] = -(a[1]*r[i-1] + ... + a[i-1]*r[1] + a[i]*r[0]).
    R[0] = e;  // reconstruct r[0] from prediction error climb
    for (int i = 1; i <= p; ++i) {
        double s = 0.0;
        for (int j = 1; j <= i - 1; ++j) s += av[j] * R[i - j];
        R[i] = -av[i] * R[0] - s;
    }
    return std::make_tuple(rowVec(R, mr), colVec(k, mr));
}

// ── AR estimation ─────────────────────────────────────────────────

std::tuple<Value, Value, Value>
aryule(const Value &x, int p, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto R = biasedAutocorr(v, p);
    auto res = levinsonCore(R, p);
    return std::make_tuple(rowVec(res.a, mr),
                           Value::scalar(res.e, mr),
                           colVec(res.k, mr));
}

std::tuple<Value, Value, Value>
arburg(const Value &x, int p, std::pmr::memory_resource *mr)
{
    // Burg's algorithm. Maintains forward / backward prediction
    // errors and updates AR coefficients via reflection coefficients
    // chosen to minimise the sum of forward + backward residual energy.
    auto v = readVec(x);
    const size_t N = v.size();
    std::vector<double> ef = v;
    std::vector<double> eb = v;
    std::vector<double> a(p + 1, 0.0); a[0] = 1.0;
    std::vector<double> k(p, 0.0);

    double E = 0.0;
    for (double y : v) E += y * y;
    E /= static_cast<double>(N);

    for (int i = 0; i < p; ++i) {
        // Reflection coefficient: maximise correlation of forward and
        // backward errors.
        double num = 0.0, den = 0.0;
        for (size_t n = i + 1; n < N; ++n) {
            num += ef[n] * eb[n - 1];
            den += ef[n] * ef[n] + eb[n - 1] * eb[n - 1];
        }
        const double ki = (den > 0) ? -2.0 * num / den : 0.0;
        k[i] = ki;

        // Update prediction errors in-place.
        for (size_t n = N - 1; n > static_cast<size_t>(i); --n) {
            const double f = ef[n];
            const double b = eb[n - 1];
            ef[n] = f + ki * b;
            eb[n] = b + ki * f;
        }

        // Update AR coefficients via the Levinson update.
        std::vector<double> a_new = a;
        for (int j = 1; j <= i; ++j)
            a_new[j] = a[j] + ki * a[i + 1 - j];
        a_new[i + 1] = ki;
        a = std::move(a_new);
        E *= (1.0 - ki * ki);
    }

    return std::make_tuple(rowVec(a, mr),
                           Value::scalar(E, mr),
                           colVec(k, mr));
}

std::tuple<Value, Value>
lpc(const Value &x, int p, std::pmr::memory_resource *mr)
{
    // MATLAB's lpc returns (a, g) where g is the prediction error
    // variance (NOT its sqrt — the docs are explicit). Same numbers
    // as aryule, just without the reflection-coef return.
    auto [a, e, k] = aryule(x, p, mr);
    (void)k;
    return std::make_tuple(std::move(a), std::move(e));
}

// ── Representation conversions ────────────────────────────────────

std::tuple<Value, Value>
ac2poly(const Value &R, std::pmr::memory_resource *mr)
{
    auto rv = readVec(R);
    const int p = static_cast<int>(rv.size()) - 1;
    auto res = levinsonCore(rv, std::max(p, 0));
    return std::make_tuple(rowVec(res.a, mr), Value::scalar(res.e, mr));
}

Value poly2ac(const Value &a, double e, std::pmr::memory_resource *mr)
{
    auto [Rv, kv] = rlevinson(a, e, mr);
    (void)kv;
    return Rv;
}

std::tuple<Value, Value>
ac2rc(const Value &R, std::pmr::memory_resource *mr)
{
    auto rv = readVec(R);
    const int p = static_cast<int>(rv.size()) - 1;
    auto res = levinsonCore(rv, std::max(p, 0));
    return std::make_tuple(colVec(res.k, mr),
                           Value::scalar(rv.empty() ? 0.0 : rv[0], mr));
}

Value schurrc(const Value &R, std::pmr::memory_resource *mr)
{
    // Standalone Schur / Levinson reflection-coeff recursion. Does NOT
    // bail on negative residual energy — schurrc returns the math-valid
    // reflection coefficients even when |k| > 1 (non-PSD R, i.e. an
    // unstable AR model). levinsonCore early-exits on e<=0 to keep the
    // fitted poly stable; schurrc doesn't want that.
    auto rv = readVec(R);
    const int p = static_cast<int>(rv.size()) - 1;
    if (p <= 0) return colVec(std::vector<double>{}, mr);

    std::vector<double> k(p, 0.0);
    std::vector<double> a_prev(p + 1, 0.0); a_prev[0] = 1.0;
    double e = rv[0];

    for (int i = 1; i <= p; ++i) {
        double num = rv[i];
        for (int j = 1; j < i; ++j) num += a_prev[j] * rv[i - j];
        const double ki = (e != 0.0) ? -num / e : 0.0;
        k[i - 1] = ki;

        std::vector<double> a_new(p + 1, 0.0); a_new[0] = 1.0;
        for (int j = 1; j < i; ++j)
            a_new[j] = a_prev[j] + ki * a_prev[i - j];
        a_new[i] = ki;
        a_prev = std::move(a_new);
        e *= (1.0 - ki * ki);
    }
    return colVec(k, mr);
}

Value rc2ac(const Value &k, double r0, std::pmr::memory_resource *mr)
{
    auto kv = readVec(k);
    const int p = static_cast<int>(kv.size());
    // Build R from k via the inverse Levinson: r0 known, then
    // r[i] = -(a_{i-1}[1]*r[i-1] + ... + a_{i-1}[i-1]*r[1]) - k[i-1]*e_{i-1}
    std::vector<double> R(p + 1, 0.0);
    R[0] = r0;
    std::vector<double> a(p + 1, 0.0); a[0] = 1.0;
    double e = r0;
    for (int i = 1; i <= p; ++i) {
        const double ki = kv[i - 1];
        double s = 0.0;
        for (int j = 1; j < i; ++j) s += a[j] * R[i - j];
        R[i] = -ki * e - s;
        // Update a by step-up.
        std::vector<double> a_new(p + 1, 0.0);
        a_new[0] = 1.0;
        for (int j = 1; j < i; ++j) a_new[j] = a[j] + ki * a[i - j];
        a_new[i] = ki;
        a = std::move(a_new);
        e *= (1.0 - ki * ki);
    }
    return rowVec(R, mr);
}

std::tuple<Value, Value>
poly2rc(const Value &a, double efinal, std::pmr::memory_resource *mr)
{
    auto av = readVec(a);
    const int p = static_cast<int>(av.size()) - 1;
    std::vector<double> k(p, 0.0);
    auto cur = av;
    for (int i = p; i >= 1; --i) {
        const double ki = cur[i];
        k[i - 1] = ki;
        const double denom = 1.0 - ki * ki;
        if (std::abs(denom) < 1e-300) break;
        std::vector<double> prev(i + 1, 0.0); prev[0] = 1.0;
        for (int j = 1; j < i; ++j)
            prev[j] = (cur[j] - ki * cur[i - j]) / denom;
        cur = prev;
    }
    // Zero-lag autocorrelation R0 = efinal / prod_i (1 - k_i^2). This is
    // the second output [k, R0] = poly2rc(a, efinal): the energy of the
    // signal whose AR model is `a` with final prediction error `efinal`.
    double prod = 1.0;
    for (double ki : k) prod *= (1.0 - ki * ki);
    const double r0 = (prod != 0.0) ? efinal / prod : 0.0;
    return std::make_tuple(colVec(k, mr), Value::scalar(r0, mr));
}

Value rc2poly(const Value &k, std::pmr::memory_resource *mr)
{
    auto kv = readVec(k);
    const int p = static_cast<int>(kv.size());
    std::vector<double> a(p + 1, 0.0); a[0] = 1.0;
    for (int i = 1; i <= p; ++i) {
        const double ki = kv[i - 1];
        std::vector<double> a_new(p + 1, 0.0);
        a_new[0] = 1.0;
        for (int j = 1; j < i; ++j) a_new[j] = a[j] + ki * a[i - j];
        a_new[i] = ki;
        a = std::move(a_new);
    }
    return rowVec(a, mr);
}

// is2rc / rc2is — inverse-sine parameterisation, k = sin(is).
Value is2rc(const Value &is, std::pmr::memory_resource *mr)
{
    auto v = readVec(is);
    for (auto &y : v) y = std::sin(y);
    return colVec(v, mr);
}
Value rc2is(const Value &k, std::pmr::memory_resource *mr)
{
    auto v = readVec(k);
    for (auto &y : v) {
        // Clamp into [-1, 1] to avoid asin domain errors on numerical
        // overshoots.
        const double c = std::max(-1.0, std::min(1.0, y));
        y = std::asin(c);
    }
    return colVec(v, mr);
}

// lar2rc / rc2lar — log-area-ratio: lar = log((1+k)/(1-k)).
Value lar2rc(const Value &g, std::pmr::memory_resource *mr)
{
    auto v = readVec(g);
    for (auto &y : v) {
        const double e = std::exp(y);
        y = (e - 1.0) / (e + 1.0);
    }
    return colVec(v, mr);
}
Value rc2lar(const Value &k, std::pmr::memory_resource *mr)
{
    auto v = readVec(k);
    for (auto &y : v) {
        const double c = std::max(-0.99999999, std::min(0.99999999, y));
        y = std::log((1.0 + c) / (1.0 - c));
    }
    return colVec(v, mr);
}

// ════════════════════════════════════════════════════════════════════
// Covariance / modified-covariance AR + Prony + corrmtx
// ════════════════════════════════════════════════════════════════════

namespace {

// Solve a small symmetric positive-definite system A·x = b in place
// using Gaussian elimination with partial pivoting. A is row-major
// p×p; returns x. For ill-conditioned inputs (singular matrix)
// returns the partial result.
std::vector<double>
solveSPD(std::vector<double> &A, std::vector<double> &b, int p)
{
    for (int k = 0; k < p; ++k) {
        // Pivot.
        int piv = k;
        double pivVal = std::abs(A[k * p + k]);
        for (int i = k + 1; i < p; ++i)
            if (std::abs(A[i * p + k]) > pivVal) { pivVal = std::abs(A[i * p + k]); piv = i; }
        if (pivVal < 1e-300) continue;
        if (piv != k) {
            for (int j = k; j < p; ++j)
                std::swap(A[k * p + j], A[piv * p + j]);
            std::swap(b[k], b[piv]);
        }
        const double diag = A[k * p + k];
        for (int i = k + 1; i < p; ++i) {
            const double f = A[i * p + k] / diag;
            for (int j = k; j < p; ++j) A[i * p + j] -= f * A[k * p + j];
            b[i] -= f * b[k];
        }
    }
    std::vector<double> x(p, 0.0);
    for (int i = p - 1; i >= 0; --i) {
        double s = b[i];
        for (int j = i + 1; j < p; ++j) s -= A[i * p + j] * x[j];
        const double diag = A[i * p + i];
        x[i] = (std::abs(diag) > 1e-300) ? s / diag : 0.0;
    }
    return x;
}

// Build the covariance correlation R_{ij} = sum_{n=p..N-1} x[n-i] * x[n-j]
// for i, j in 0..p. Used by arcov / armcov.
std::vector<double>
covMatrixForward(const std::vector<double> &x, int p)
{
    const int N = static_cast<int>(x.size());
    std::vector<double> R((p + 1) * (p + 1), 0.0);
    for (int n = p; n < N; ++n) {
        for (int i = 0; i <= p; ++i)
            for (int j = 0; j <= p; ++j)
                R[i * (p + 1) + j] += x[n - i] * x[n - j];
    }
    return R;
}

std::vector<double>
covMatrixForwardBackward(const std::vector<double> &x, int p)
{
    const int N = static_cast<int>(x.size());
    auto R = covMatrixForward(x, p);
    // Backward: R += sum_{n=0..N-1-p} x[n+i] x[n+j]
    for (int n = 0; n + p < N; ++n) {
        for (int i = 0; i <= p; ++i)
            for (int j = 0; j <= p; ++j)
                R[i * (p + 1) + j] += x[n + i] * x[n + j];
    }
    return R;
}

// Given a (p+1)×(p+1) covariance matrix R indexed R[i,j] for i,j in
// 0..p, solve for AR coefficients a[1..p] from R[1..p, 1..p] · a =
// -R[1..p, 0]. Returns full a vector with a[0] = 1 prepended, plus
// the prediction error e = R[0,0] + sum_k a[k] * R[0,k].
struct ArCovResult { std::vector<double> a; double e; };
ArCovResult solveArCov(const std::vector<double> &R, int p)
{
    std::vector<double> A(p * p, 0.0);
    std::vector<double> b(p, 0.0);
    for (int i = 0; i < p; ++i) {
        for (int j = 0; j < p; ++j) A[i * p + j] = R[(i + 1) * (p + 1) + (j + 1)];
        b[i] = -R[(i + 1) * (p + 1) + 0];
    }
    auto a_tail = solveSPD(A, b, p);
    std::vector<double> a(p + 1, 0.0);
    a[0] = 1.0;
    for (int k = 0; k < p; ++k) a[k + 1] = a_tail[k];
    double e = R[0];
    for (int k = 1; k <= p; ++k) e += a[k] * R[k];
    return {a, e};
}

} // anonymous

std::tuple<Value, Value>
arcov(const Value &x, int p, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto R = covMatrixForward(v, p);
    // Normalise by the count of summed terms (N - p) so e has the
    // same units as the residual variance.
    const int N = static_cast<int>(v.size());
    const double norm = static_cast<double>(std::max(N - p, 1));
    for (auto &r : R) r /= norm;
    auto res = solveArCov(R, p);
    return std::make_tuple(rowVec(res.a, mr), Value::scalar(res.e, mr));
}

std::tuple<Value, Value>
armcov(const Value &x, int p, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    auto R = covMatrixForwardBackward(v, p);
    const int N = static_cast<int>(v.size());
    const double norm = static_cast<double>(std::max(2 * (N - p), 1));
    for (auto &r : R) r /= norm;
    auto res = solveArCov(R, p);
    return std::make_tuple(rowVec(res.a, mr), Value::scalar(res.e, mr));
}

// ── prony ─────────────────────────────────────────────────────────
// Identify a numerator b (length nb+1) and denominator a (length na+1)
// such that the IIR filter b(z)/a(z) has impulse response approximately
// equal to h. Standard formulation:
//   1. Form the Hankel-like matrix H of `h` shifted, build the system
//      H_{na rows after first nb+1 samples} · [a[1..na]] = -h[nb+1..]
//   2. Solve by least squares.
//   3. b = (a convolved with h) truncated to nb+1 terms.
std::tuple<Value, Value>
prony(const Value &h, int nb, int na, std::pmr::memory_resource *mr)
{
    auto hv = readVec(h);
    const int N = static_cast<int>(hv.size());
    if (na > 0) {
        // Build linear system M · a_tail = -rhs, where M is (N-1-nb) × na
        // and rhs is length (N-1-nb). M[i, j] = h[nb+1+i-1-j] (padded
        // with zeros on the left), rhs[i] = h[nb+1+i].
        const int rows = std::max(N - 1 - nb, 1);
        std::vector<double> M(rows * na, 0.0);
        std::vector<double> rhs(rows, 0.0);
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < na; ++j) {
                const int idx = nb + i - j;  // h index = nb+1+i - (j+1) = nb+i-j
                M[i * na + j] = (idx >= 0 && idx < N) ? hv[idx] : 0.0;
            }
            const int rhs_idx = nb + 1 + i;
            rhs[i] = (rhs_idx < N) ? -hv[rhs_idx] : 0.0;
        }
        // Normal equations: (M^T M) · a_tail = M^T rhs
        std::vector<double> MtM(na * na, 0.0);
        std::vector<double> Mtb(na, 0.0);
        for (int i = 0; i < na; ++i) {
            for (int j = 0; j < na; ++j) {
                double s = 0.0;
                for (int k = 0; k < rows; ++k) s += M[k * na + i] * M[k * na + j];
                MtM[i * na + j] = s;
            }
            double s = 0.0;
            for (int k = 0; k < rows; ++k) s += M[k * na + i] * rhs[k];
            Mtb[i] = s;
        }
        auto a_tail = solveSPD(MtM, Mtb, na);
        std::vector<double> a(na + 1, 0.0);
        a[0] = 1.0;
        for (int k = 0; k < na; ++k) a[k + 1] = a_tail[k];

        // b = (a * h) truncated to nb+1 terms.
        std::vector<double> b(nb + 1, 0.0);
        for (int i = 0; i <= nb; ++i) {
            double s = 0.0;
            for (int k = 0; k <= std::min(i, na); ++k)
                if (i - k < N) s += a[k] * hv[i - k];
            b[i] = s;
        }
        return std::make_tuple(rowVec(b, mr), rowVec(a, mr));
    }
    // na == 0: pure FIR. b[i] = h[i] for i = 0..nb, a = [1].
    std::vector<double> b(nb + 1, 0.0);
    for (int i = 0; i <= nb && i < N; ++i) b[i] = hv[i];
    return std::make_tuple(rowVec(b, mr), rowVec(std::vector<double>{1.0}, mr));
}

// ── corrmtx ───────────────────────────────────────────────────────
// MATLAB default 'autocorrelation' method: produces the (n+m)×(m+1)
// data matrix X with X(i, j) = x(i-j+1), zero-padded outside the
// input range, such that X' * X is the (m+1)×(m+1) autocorrelation
// matrix.
Value corrmtx(const Value &x, int m, std::pmr::memory_resource *mr)
{
    auto v = readVec(x);
    const int N = static_cast<int>(v.size());
    const int rows = N + m;
    const int cols = m + 1;
    auto X = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    double *d = X.doubleDataMut();
    std::fill(d, d + rows * cols, 0.0);
    for (int j = 0; j < cols; ++j) {
        for (int i = 0; i < rows; ++i) {
            const int idx = i - j;     // x[i-j], zero outside [0, N)
            if (idx >= 0 && idx < N) d[i + j * rows] = v[idx];
        }
    }
    return X;
}

// ── LSF ↔ AR poly ────────────────────────────────────────────────
// LSF is built from P(z) = A(z) + z^{-(N+1)} A_R(z) and Q(z) = A(z)
// - z^{-(N+1)} A_R(z), where A_R is the reverse of A. The roots of
// P and Q lie on the unit circle, alternating; the LSF angles are
// the angles of those roots in (0, π).

namespace {

std::vector<double> reverseVec(const std::vector<double> &v)
{
    return std::vector<double>(v.rbegin(), v.rend());
}

// Pad poly p (descending power) to total length n with leading zeros.
std::vector<double> padLeft(const std::vector<double> &p, int n)
{
    std::vector<double> r(n, 0.0);
    const int off = n - static_cast<int>(p.size());
    for (size_t i = 0; i < p.size(); ++i) r[off + i] = p[i];
    return r;
}

} // anonymous

// AR polynomial A(z) -> line-spectral frequencies.
// Form the symmetric / antisymmetric pair P(z) = A(z) + z^-(N+1)·A(z^-1)
// and Q(z) = A(z) - z^-(N+1)·A(z^-1); their roots lie on the unit
// circle and the LSFs are those roots' angles in (0, π).
// References: F. Itakura, "Line spectrum representation of linear
// predictor coefficients of speech signals", J. Acoust. Soc. Am.
// 57(S1):S35, 1975; P. Kabal & R. P. Ramachandran, "The computation of
// line spectral frequencies using Chebyshev polynomials", IEEE Trans.
// ASSP 34(6):1419-1426, 1986.
Value poly2lsf(const Value &a, std::pmr::memory_resource *mr)
{
    auto av = readVec(a);
    const int N = static_cast<int>(av.size()) - 1;
    if (N <= 0) return colVec(std::vector<double>{}, mr);

    // Build P = A + A_R_shifted, Q = A - A_R_shifted, both length N+2.
    auto ar = reverseVec(av);
    std::vector<double> P(N + 2, 0.0), Q(N + 2, 0.0);
    for (int i = 0; i <= N; ++i) {
        P[i] = av[i];
        Q[i] = av[i];
    }
    for (int i = 1; i <= N + 1; ++i) {
        P[i] += ar[i - 1];
        Q[i] -= ar[i - 1];
    }
    // Find roots and pick angles in (0, π).
    std::vector<double> angles;
    for (auto &poly : {P, Q}) {
        Value polyV = rowVec(poly, mr);
        Value rts = numkit::builtin::roots(polyV, mr);
        const size_t n = rts.numel();
        for (size_t i = 0; i < n; ++i) {
            const Complex c = rts.complexData()[i];
            const double a_ang = std::atan2(c.imag(), c.real());
            if (a_ang > 1e-9 && a_ang < M_PI - 1e-9) angles.push_back(a_ang);
        }
    }
    std::sort(angles.begin(), angles.end());
    // Should yield N angles.
    if (static_cast<int>(angles.size()) > N) angles.resize(N);
    return colVec(angles, mr);
}

Value lsf2poly(const Value &lsf, std::pmr::memory_resource *mr)
{
    auto lv = readVec(lsf);
    const int m = static_cast<int>(lv.size());
    if (m == 0) return rowVec(std::vector<double>{1.0}, mr);

    // LSF -> LPC conversion (Kabal & Ramachandran, IEEE TASSP, 1986).
    // The LSF angles split into two interleaved sets (even / odd
    // index) that build the symmetric / antisymmetric polynomials
    // P and Q; a = ((P + Q) / 2) with the trailing element dropped.
    //
    // Each conjugate-pair root z = e^{±iθ} contributes the real
    // quadratic factor (1 - 2cos(θ) z^-1 + z^-2). The boundary roots
    // z = -1 and z = +1 contribute the linear factors (1 + z^-1) and
    // (1 - z^-1) respectively.
    //
    // The boundary-root distribution differs by parity so that A and
    // B always end up with the same degree (m+1); their trailing
    // coefficients cancel exactly in (A+B)/2 and we drop the result.
    auto quad = [](double w) {
        return std::vector<double>{1.0, -2.0 * std::cos(w), 1.0};
    };
    auto convOne = [](const std::vector<double> &a,
                      const std::vector<double> &b) {
        std::vector<double> r(a.size() + b.size() - 1, 0.0);
        for (size_t i = 0; i < a.size(); ++i)
            for (size_t j = 0; j < b.size(); ++j)
                r[i + j] += a[i] * b[j];
        return r;
    };
    auto convAll = [&](const std::vector<std::vector<double>> &ps,
                       const std::vector<double> &lead) {
        std::vector<double> r = lead;
        for (auto &p : ps) r = convOne(r, p);
        return r;
    };

    std::vector<std::vector<double>> Pquads, Qquads;
    for (int i = 0; i < m; ++i) {
        if (i % 2 == 0) Pquads.push_back(quad(lv[i]));
        else            Qquads.push_back(quad(lv[i]));
    }

    std::vector<double> P, Q;
    if (m % 2 == 1) {
        // m odd: P has no boundary roots, Q gets both (-1 and +1) =
        // (1-z^-2). Both end up degree m+1.
        P = convAll(Pquads, std::vector<double>{1.0});
        Q = convAll(Qquads, std::vector<double>{1.0, 0.0, -1.0});
    } else {
        // m even: P gets z=-1 → (1+z^-1); Q gets z=+1 → (1-z^-1).
        // Both end up degree m+1.
        P = convAll(Pquads, std::vector<double>{1.0,  1.0});
        Q = convAll(Qquads, std::vector<double>{1.0, -1.0});
    }

    // P and Q have identical length now. a = (P + Q) / 2, then drop
    // the trailing element (it cancels to ~0 by construction).
    std::vector<double> a(P.size(), 0.0);
    for (size_t i = 0; i < P.size(); ++i) a[i] = 0.5 * (P[i] + Q[i]);
    if (a.size() > 1) a.pop_back();
    return rowVec(a, mr);
}

// ════════════════════════════════════════════════════════════════════
// invfreqs / invfreqz — Levi equation-error LSQ
// ════════════════════════════════════════════════════════════════════
//
// Given a desired complex frequency response H(jω_k) (or H(e^{jω_k}))
// at K frequency points, fit (b, a) such that B(jω) ≈ H · A(jω).
// With a_0 = 1 fixed, each frequency point contributes one complex
// equation linear in the (nb + 1 + na) real unknowns:
//
//   -b_0 - b_1·xᵏ - … - b_nb·xᵏⁿᵇ + a_1·H_k·xᵏ + … + a_na·H_k·xᵏⁿᵃ = -H_k
//
// where x = jω_k for invfreqs and x = e^{-jω_k} for invfreqz (z⁻¹ form).
// Stack real and imaginary parts → 2K real equations in nv = nb+1+na
// real unknowns; solve via normal equations using solveSPD.

namespace {

using Cd_invfreq = std::complex<double>;

std::vector<Cd_invfreq> readComplexVecLocal(const Value &v) {
    const size_t n = v.numel();
    std::vector<Cd_invfreq> out(n);
    if (v.type() == ValueType::COMPLEX) {
        const Cd_invfreq *cd = v.complexData();
        for (size_t i = 0; i < n; ++i) out[i] = cd[i];
    } else {
        for (size_t i = 0; i < n; ++i) out[i] = Cd_invfreq(v.elemAsDouble(i), 0.0);
    }
    return out;
}

// Common solver: x is the "step" variable per frequency point —
// jω_k for invfreqs, e^{-jω_k} for invfreqz.
std::tuple<std::vector<double>, std::vector<double>>
invfreqLSQ(const std::vector<Cd_invfreq> &H,
           const std::vector<Cd_invfreq> &x,
           int nb, int na)
{
    const int K  = static_cast<int>(H.size());
    const int nv = nb + 1 + na;
    if (K * 2 < nv)
        throw std::runtime_error("invfreq: not enough frequency points to fit (nb, na)");

    // Build A (2K × nv) row-major, plus rhs (length 2K).
    std::vector<double> Ar(2 * K * nv, 0.0);
    std::vector<double> rhs(2 * K, 0.0);
    for (int k = 0; k < K; ++k) {
        // Build x^0..x^max(nb, na) once.
        const int M = std::max(nb, na);
        std::vector<Cd_invfreq> xpow(M + 1);
        xpow[0] = Cd_invfreq(1.0, 0.0);
        for (int i = 1; i <= M; ++i) xpow[i] = xpow[i - 1] * x[k];

        // Row k contains:
        //   -1, -x, -x², …, -x^nb, H·x, H·x², …, H·x^na
        std::vector<Cd_invfreq> row(nv);
        for (int i = 0; i <= nb; ++i) row[i] = -xpow[i];
        for (int j = 1; j <= na; ++j) row[nb + j] = H[k] * xpow[j];

        // Stack real / imag into Ar.
        for (int c = 0; c < nv; ++c) {
            Ar[(2 * k    ) * nv + c] = row[c].real();
            Ar[(2 * k + 1) * nv + c] = row[c].imag();
        }
        rhs[2 * k    ] = -H[k].real();
        rhs[2 * k + 1] = -H[k].imag();
    }

    // Normal equations: (Aᵀ A) θ = Aᵀ rhs.
    std::vector<double> AtA(nv * nv, 0.0);
    std::vector<double> Atb(nv, 0.0);
    for (int i = 0; i < nv; ++i) {
        for (int j = 0; j < nv; ++j) {
            double s = 0.0;
            for (int r = 0; r < 2 * K; ++r) s += Ar[r * nv + i] * Ar[r * nv + j];
            AtA[i * nv + j] = s;
        }
        double s = 0.0;
        for (int r = 0; r < 2 * K; ++r) s += Ar[r * nv + i] * rhs[r];
        Atb[i] = s;
    }
    auto theta = solveSPD(AtA, Atb, nv);

    // theta = [b_0, ..., b_nb, a_1, ..., a_na]
    std::vector<double> b(nb + 1), a(na + 1);
    for (int i = 0; i <= nb; ++i) b[i] = theta[i];
    a[0] = 1.0;
    for (int j = 1; j <= na; ++j) a[j] = theta[nb + j];
    return std::make_tuple(std::move(b), std::move(a));
}

} // anonymous

std::tuple<Value, Value>
invfreqs(const Value &H, const Value &w, int nb, int na, std::pmr::memory_resource *mr)
{
    auto Hv = readComplexVecLocal(H);
    auto wv = readVec(w);
    if (Hv.size() != wv.size())
        throw std::runtime_error("invfreqs: H and w must be the same length");
    std::vector<Cd_invfreq> x(wv.size());
    for (size_t k = 0; k < wv.size(); ++k) x[k] = Cd_invfreq(0.0, wv[k]); // jω
    auto [b, a] = invfreqLSQ(Hv, x, nb, na);
    return std::make_tuple(rowVec(b, mr), rowVec(a, mr));
}

std::tuple<Value, Value>
invfreqz(const Value &H, const Value &w, int nb, int na, std::pmr::memory_resource *mr)
{
    auto Hv = readComplexVecLocal(H);
    auto wv = readVec(w);
    if (Hv.size() != wv.size())
        throw std::runtime_error("invfreqz: H and w must be the same length");
    std::vector<Cd_invfreq> x(wv.size());
    for (size_t k = 0; k < wv.size(); ++k)
        x[k] = std::exp(Cd_invfreq(0.0, -wv[k])); // z⁻¹
    auto [b, a] = invfreqLSQ(Hv, x, nb, na);
    return std::make_tuple(rowVec(b, mr), rowVec(a, mr));
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void levinson_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("levinson: requires at least 1 argument",
                     0, 0, "levinson", "", "numkit:levinson:nargin");
    int n = -1;
    if (args.size() >= 2 && !args[1].isEmpty()) n = static_cast<int>(args[1].toScalar());
    auto [a, e, k] = levinson(args[0], n, ctx.engine->resource());
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(e);
    if (nargout > 2) outs[2] = std::move(k);
}

void rlevinson_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rlevinson: requires (a, e)",
                     0, 0, "rlevinson", "", "numkit:rlevinson:nargin");
    auto [R, k] = rlevinson(args[0], args[1].toScalar(), ctx.engine->resource());
    outs[0] = std::move(R);
    if (nargout > 1) outs[1] = std::move(k);
}

#define NK_AR_REG(name, fn)                                                     \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#name ": requires (x, p)",                              \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int p = static_cast<int>(args[1].toScalar());                     \
        auto [a, e, k] = fn(args[0], p, ctx.engine->resource());                \
        outs[0] = std::move(a);                                                  \
        if (nargout > 1) outs[1] = std::move(e);                                 \
        if (nargout > 2) outs[2] = std::move(k);                                 \
    }

NK_AR_REG(aryule, aryule)
NK_AR_REG(arburg, arburg)

#undef NK_AR_REG

void lpc_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("lpc: requires (x, p)",
                     0, 0, "lpc", "", "numkit:lpc:nargin");
    const int p = static_cast<int>(args[1].toScalar());
    auto [a, g] = lpc(args[0], p, ctx.engine->resource());
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(g);
}

void ac2poly_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("ac2poly: requires 1 argument", 0, 0, "ac2poly", "", "numkit:ac2poly:nargin");
    auto [a, e] = ac2poly(args[0], ctx.engine->resource());
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(e);
}

void poly2ac_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("poly2ac: requires 1 argument", 0, 0, "poly2ac", "", "numkit:poly2ac:nargin");
    const double e = (args.size() >= 2 && !args[1].isEmpty()) ? args[1].toScalar() : 1.0;
    outs[0] = poly2ac(args[0], e, ctx.engine->resource());
}

void ac2rc_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("ac2rc: requires 1 argument", 0, 0, "ac2rc", "", "numkit:ac2rc:nargin");
    auto [k, r0] = ac2rc(args[0], ctx.engine->resource());
    outs[0] = std::move(k);
    if (nargout > 1) outs[1] = std::move(r0);
}

void schurrc_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty()) throw Error("schurrc: requires 1 argument (R)", 0, 0, "schurrc", "", "numkit:schurrc:nargin");
    outs[0] = schurrc(args[0], ctx.engine->resource());
}

void rc2ac_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2) throw Error("rc2ac: requires (k, r0)", 0, 0, "rc2ac", "", "numkit:rc2ac:nargin");
    outs[0] = rc2ac(args[0], args[1].toScalar(), ctx.engine->resource());
}

void poly2rc_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("poly2rc: requires at least 1 argument (a)",
                     0, 0, "poly2rc", "", "numkit:poly2rc:nargin");
    // [k, r0] = poly2rc(a, efinal). efinal (final prediction error) is
    // only needed for the second output; defaults to 0.
    const double efinal = (args.size() >= 2 && !args[1].isEmpty())
                              ? args[1].toScalar() : 0.0;
    auto [k, r0] = poly2rc(args[0], efinal, ctx.engine->resource());
    outs[0] = std::move(k);
    if (nargout > 1) outs[1] = std::move(r0);
}

#define NK_UNARY_CONV_REG(name)                                                 \
    void name##_reg(Span<const Value> args, size_t /*nargout*/,                \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.empty())                                                        \
            throw Error(#name ": requires 1 argument",                          \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        outs[0] = name(args[0], ctx.engine->resource());                         \
    }

NK_UNARY_CONV_REG(rc2poly)
NK_UNARY_CONV_REG(is2rc)
NK_UNARY_CONV_REG(rc2is)
NK_UNARY_CONV_REG(lar2rc)
NK_UNARY_CONV_REG(rc2lar)
NK_UNARY_CONV_REG(poly2lsf)
NK_UNARY_CONV_REG(lsf2poly)

#undef NK_UNARY_CONV_REG

#define NK_AR2_REG(name, fn)                                                    \
    void name##_reg(Span<const Value> args, size_t nargout,                    \
                    Span<Value> outs, CallContext &ctx)                        \
    {                                                                            \
        if (args.size() < 2)                                                     \
            throw Error(#name ": requires (x, p)",                              \
                         0, 0, #name, "", "numkit:" #name ":nargin");                 \
        const int p = static_cast<int>(args[1].toScalar());                     \
        auto [a, e] = fn(args[0], p, ctx.engine->resource());                   \
        outs[0] = std::move(a);                                                  \
        if (nargout > 1) outs[1] = std::move(e);                                 \
    }

NK_AR2_REG(arcov,  arcov)
NK_AR2_REG(armcov, armcov)

#undef NK_AR2_REG

void prony_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("prony: requires (h, nb, na)",
                     0, 0, "prony", "", "numkit:prony:nargin");
    const int nb = static_cast<int>(args[1].toScalar());
    const int na = static_cast<int>(args[2].toScalar());
    auto [b, a] = prony(args[0], nb, na, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void corrmtx_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("corrmtx: requires (x, m)",
                     0, 0, "corrmtx", "", "numkit:corrmtx:nargin");
    const int m = static_cast<int>(args[1].toScalar());
    outs[0] = corrmtx(args[0], m, ctx.engine->resource());
}

void invfreqs_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("invfreqs: requires (H, w, nb, na)",
                     0, 0, "invfreqs", "", "numkit:invfreqs:nargin");
    const int nb = static_cast<int>(args[2].toScalar());
    const int na = static_cast<int>(args[3].toScalar());
    auto [b, a] = invfreqs(args[0], args[1], nb, na, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void invfreqz_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("invfreqz: requires (H, w, nb, na)",
                     0, 0, "invfreqz", "", "numkit:invfreqz:nargin");
    const int nb = static_cast<int>(args[2].toScalar());
    const int na = static_cast<int>(args[3].toScalar());
    auto [b, a] = invfreqz(args[0], args[1], nb, na, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

} // namespace detail
} // namespace numkit::signal
