// toolboxes/comm/src/channel/channel.cpp

#include <numkit/comm/channel/channel.hpp>

#include <numkit/builtin/math/random/rng.hpp>
#include <numkit/builtin/math/special/special.hpp>     // betaincinv

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <mutex>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::comm {

namespace {

using Cd = std::complex<double>;

// Compute average power of a complex / real array.
double avg_power(const Value &x) {
    const size_t N = x.numel();
    if (N == 0) return 0.0;
    double s = 0.0;
    if (x.type() == ValueType::COMPLEX) {
        const Cd *cd = x.complexData();
        for (size_t i = 0; i < N; ++i) s += std::norm(cd[i]);
    } else {
        for (size_t i = 0; i < N; ++i) {
            const double v = x.elemAsDouble(i);
            s += v * v;
        }
    }
    return s / double(N);
}

template <typename Op>
Value elementwise(std::pmr::memory_resource *mr, const Value &x, Op op) {
    const size_t N = x.numel();
    if (x.isScalar()) return Value::scalar(op(x.toScalar()), mr);
    const auto &d = x.dims();
    Value out;
    if (d.is3D()) out = Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    else          out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) od[i] = op(x.elemAsDouble(i));
    return out;
}

} // anonymous

Value awgn(const Value &x, double snr_db, double sigpower_db,
           std::pmr::memory_resource *mr)
{
    const bool is_complex = (x.type() == ValueType::COMPLEX);
    double sig_pow = (sigpower_db < -1e9)  // -inf sentinel = "measured"
                     ? avg_power(x)
                     : std::pow(10.0, sigpower_db / 10.0);
    if (sig_pow <= 0.0) sig_pow = 1.0;
    const double snr_lin = std::pow(10.0, snr_db / 10.0);
    const double noise_pow = sig_pow / snr_lin;
    const double sigma = std::sqrt(is_complex ? noise_pow / 2.0 : noise_pow);

    const size_t N = x.numel();
    Value out;
    const auto &d = x.dims();
    const ValueType ty = is_complex ? ValueType::COMPLEX : ValueType::DOUBLE;
    if (x.isScalar()) out = Value::matrix(1, 1, ty, mr);
    else if (d.is3D())out = Value::matrix3d(d.rows(), d.cols(), d.pages(), ty, mr);
    else              out = Value::matrix(d.rows(), d.cols(), ty, mr);

    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::normal_distribution<double> nd(0.0, sigma);
    std::lock_guard<std::mutex> lk(mtx);
    if (is_complex) {
        Cd *od = out.complexDataMut();
        const Cd *xd = x.complexData();
        for (size_t i = 0; i < N; ++i)
            od[i] = xd[i] + Cd(nd(gen), nd(gen));
    } else {
        double *od = out.doubleDataMut();
        for (size_t i = 0; i < N; ++i)
            od[i] = x.elemAsDouble(i) + nd(gen);
    }
    return out;
}

Value wgn(int m, int n, double p, const std::string &type,
          bool complex_out, std::pmr::memory_resource *mr)
{
    double power_lin = 0.0;
    if (type == "linear") power_lin = p;
    else if (type == "dBm") power_lin = std::pow(10.0, (p - 30.0) / 10.0);
    else /* dBW */         power_lin = std::pow(10.0, p / 10.0);
    if (power_lin < 0.0) power_lin = 0.0;
    const double sigma = std::sqrt(complex_out ? power_lin / 2.0 : power_lin);

    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::normal_distribution<double> nd(0.0, sigma);
    std::lock_guard<std::mutex> lk(mtx);

    if (complex_out) {
        Value out = Value::matrix(m, n, ValueType::COMPLEX, mr);
        Cd *od = out.complexDataMut();
        const size_t N = (size_t)m * (size_t)n;
        for (size_t i = 0; i < N; ++i) od[i] = Cd(nd(gen), nd(gen));
        return out;
    }
    Value out = Value::matrix(m, n, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const size_t N = (size_t)m * (size_t)n;
    for (size_t i = 0; i < N; ++i) od[i] = nd(gen);
    return out;
}

Value bsc(const Value &x, double p, std::pmr::memory_resource *mr) {
    if (p < 0.0 || p > 1.0)
        throw Error("bsc: p must be in [0, 1]", 0, 0, "bsc", "",
                    "numkit:bsc:badp");
    auto &gen = ::numkit::builtin::sharedEngine();
    auto &mtx = ::numkit::builtin::rngMutex();
    std::uniform_real_distribution<double> ud(0.0, 1.0);

    const size_t N = x.numel();
    Value out;
    const auto &d = x.dims();
    if (x.isScalar()) out = Value::matrix(1, 1, ValueType::DOUBLE, mr);
    else if (d.is3D())out = Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    else              out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    std::lock_guard<std::mutex> lk(mtx);
    for (size_t i = 0; i < N; ++i) {
        const int b = (int)x.elemAsDouble(i) & 1;
        od[i] = double(ud(gen) < p ? (b ^ 1) : b);
    }
    return out;
}

Value qfunc(const Value &x, std::pmr::memory_resource *mr) {
    return elementwise(mr, x, [](double v) {
        return 0.5 * std::erfc(v / std::sqrt(2.0));
    });
}

namespace {
// Inverse complementary error function via Newton refinement on Acklam approx.
double erfcinv_approx(double x) {
    // erfcinv(x) = -erfinv(x - 1). Use Acklam-style series for the inv normal CDF
    // and convert: norminv(p) = √2·erfinv(2p-1), so erfinv(y) = norminv((y+1)/2)/√2.
    const double p = 1.0 - 0.5 * x;  // erfcinv(x) where x = 2(1-p)
    static const double a[] = { -3.969683028665376e+01,  2.209460984245205e+02,
                                 -2.759285104469687e+02,  1.383577518672690e+02,
                                 -3.066479806614716e+01,  2.506628277459239e+00 };
    static const double b[] = { -5.447609879822406e+01,  1.615858368580409e+02,
                                 -1.556989798598866e+02,  6.680131188771972e+01,
                                 -1.328068155288572e+01 };
    static const double c[] = { -7.784894002430293e-03, -3.223964580411365e-01,
                                 -2.400758277161838e+00, -2.549732539343734e+00,
                                  4.374664141464968e+00,  2.938163982698783e+00 };
    static const double d[] = {  7.784695709041462e-03,  3.224671290700398e-01,
                                  2.445134137142996e+00,  3.754408661907416e+00 };
    const double pl = 0.02425, ph = 1.0 - pl;
    double q, r, z;
    if (p < pl) {
        q = std::sqrt(-2.0 * std::log(p));
        z = (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
            ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    } else if (p <= ph) {
        q = p - 0.5; r = q*q;
        z = (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5]) * q /
            (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1.0);
    } else {
        q = std::sqrt(-2.0 * std::log(1.0 - p));
        z = -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) /
             ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1.0);
    }
    return z / std::sqrt(2.0);
}
} // anonymous

Value qfuncinv(const Value &p, std::pmr::memory_resource *mr) {
    return elementwise(mr, p, [](double v) {
        if (!(v > 0.0 && v < 1.0)) return std::numeric_limits<double>::quiet_NaN();
        // Q(x) = 0.5·erfc(x/√2). Inverse: x = √2·erfcinv(2v).
        return std::sqrt(2.0) * erfcinv_approx(2.0 * v);
    });
}

Value marcumq(const Value &a_v, const Value &b_v, int m,
              std::pmr::memory_resource *mr)
{
    // Power-series approximation: Q_m(a, b) = exp(-(a²+b²)/2) · Σ_{k=0..} (a/b)^(m-1+k) · I_{m-1+k}(ab)
    // For first-cut, only m=1 (standard Marcum Q) is implemented robustly via:
    //   Q1(a, b) = exp(-(a²+b²)/2) · Σ_{k=0..∞} (a/b)^k · I_k(ab)
    // which simplifies to the classical formula. For other m, fall back to
    // numerical integration of the integral form.
    if (m < 1) m = 1;
    const size_t Na = a_v.numel(), Nb = b_v.numel();
    const size_t N = std::max(Na, Nb);
    Value out = Value::matrix((Na > Nb) ? Na : Nb, 1, ValueType::DOUBLE, mr);
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        const double a = a_v.elemAsDouble(Na > 1 ? i : 0);
        const double b = b_v.elemAsDouble(Nb > 1 ? i : 0);
        if (b <= 0.0)         { od[i] = 1.0; continue; }
        if (a == 0.0) {
            // Q_m(0, b) = exp(-b²/2) · Σ_{k=0..m-1} b^(2k)/(2^k · k!)
            // = chi-squared 2m-DOF tail at b².
            const double y = 0.5 * b * b;
            // Integrate gamma(m, b²/2) upper incomplete / Γ(m).
            // For small m we can sum directly: Q_m(0, b) = Σ_{k=0..m-1} y^k e^{-y} / k!
            double acc = 0.0;
            double term = std::exp(-y);
            for (int k = 0; k < m; ++k) {
                acc += term;
                term *= y / double(k + 1);
            }
            od[i] = acc;
            continue;
        }
        // Numerical integration of the integral definition:
        //   Q_m(a, b) = ∫_b^∞ x · (x/a)^(m-1) · exp(-(x²+a²)/2) · I_{m-1}(a·x) dx
        // Adaptive trapezoidal up to a sensible upper limit.
        const double upper = std::max(b + 20.0, a + 20.0);
        const int N_int = 4096;
        const double dx = (upper - b) / double(N_int);
        // Modified Bessel I_{m-1}(z) via series: I_n(z) = Σ_{k=0..} (z/2)^(2k+n)/(k!(k+n)!).
        auto besselI = [&](int n, double z) {
            if (z == 0.0) return n == 0 ? 1.0 : 0.0;
            const double half = z / 2.0;
            double term = 1.0;
            for (int k = 1; k <= n; ++k) term *= half / double(k);
            term *= std::pow(half, n) / std::pow(half, n);
            // Correction: term should be (z/2)^n / n!. The above sequence gives
            // exactly that, but to be safe rebuild explicitly:
            term = 1.0;
            for (int k = 1; k <= n; ++k) term *= half / double(k);
            term *= std::pow(half, n);
            // Hmm — replace with simpler explicit form:
            term = std::pow(half, n);
            double fact = 1.0;
            for (int k = 1; k <= n; ++k) fact *= double(k);
            term /= fact;
            double acc = term;
            for (int k = 1; k < 100; ++k) {
                term *= half * half / (double(k) * double(k + n));
                acc += term;
                if (std::fabs(term) < 1e-18 * std::fabs(acc)) break;
            }
            return acc;
        };
        double sum = 0.0;
        for (int k = 0; k <= N_int; ++k) {
            const double xv = b + double(k) * dx;
            const double w = (k == 0 || k == N_int) ? 0.5 : 1.0;
            const double f = xv * std::pow(xv / a, m - 1)
                           * std::exp(-0.5 * (xv * xv + a * a))
                           * besselI(m - 1, a * xv);
            sum += w * f;
        }
        od[i] = sum * dx;
    }
    return out;
}

Value berawgn(const Value &EbNo_dB, const std::string &mod, int M,
              std::pmr::memory_resource *mr)
{
    return elementwise(mr, EbNo_dB, [&](double dB) {
        const double EbNo = std::pow(10.0, dB / 10.0);
        if (mod == "psk") {
            if (M == 2) return 0.5 * std::erfc(std::sqrt(EbNo));
            if (M == 4) return 0.5 * std::erfc(std::sqrt(EbNo));
            // BER approx for M-PSK (high SNR): (2/k) · Q(√(2k·EbNo) sin(π/M))
            const double k = std::log2(double(M));
            const double arg = std::sqrt(2.0 * k * EbNo) * std::sin(M_PI / double(M));
            return (2.0 / k) * 0.5 * std::erfc(arg / std::sqrt(2.0));
        }
        if (mod == "qam") {
            const double k = std::log2(double(M));
            const double K = std::sqrt(double(M));
            const double base = std::erfc(std::sqrt(3.0 * k * EbNo / (2.0 * (M - 1))));
            const double sym_err = (1.0 - 1.0 / K) * base;
            return sym_err / k;  // approximate Gray-coded BER
        }
        if (mod == "pam") {
            const double k = std::log2(double(M));
            const double base = std::erfc(std::sqrt(3.0 * k * EbNo / (M * M - 1)));
            return (M - 1) / (M * k) * base;
        }
        if (mod == "fsk") {
            // Coherent orthogonal FSK: BER = 0.5·erfc(√(EbNo/2)) for M=2.
            return 0.5 * std::erfc(std::sqrt(EbNo / 2.0));
        }
        if (mod == "dpsk") {
            // BER for DPSK (M=2): 0.5·exp(-EbNo).
            return 0.5 * std::exp(-EbNo);
        }
        return std::numeric_limits<double>::quiet_NaN();
    });
}

Value noisebw(const Value &num, const Value &den, int Nsamp,
              double fs, std::pmr::memory_resource *mr)
{
    // MATLAB convention (noisebw):
    //   NBW = (fs / N) * sum(|H[k]|^2) / max(|H[k]|^2)
    // Discrete grid over [0, pi) -- N points spaced w_k = pi*k/N.
    // (No 1/2 factor; the resulting NBW matches MATLAB's reference.)
    if (Nsamp <= 0) Nsamp = 1024;
    const size_t Mn = num.numel();
    const size_t Md = den.numel();
    std::vector<double> bn(Mn), an(Md);
    for (size_t i = 0; i < Mn; ++i) bn[i] = num.elemAsDouble(i);
    for (size_t i = 0; i < Md; ++i) an[i] = den.elemAsDouble(i);

    double Hmax2 = 0.0;
    double sum_H2 = 0.0;
    for (int k = 0; k < Nsamp; ++k) {
        const double w = M_PI * double(k) / double(Nsamp);
        const Cd z = std::polar(1.0, -w);
        Cd np(0.0, 0.0), dp(0.0, 0.0);
        Cd zk(1.0, 0.0);
        for (size_t i = 0; i < Mn; ++i) { np += bn[i] * zk; zk *= z; }
        zk = Cd(1.0, 0.0);
        for (size_t i = 0; i < Md; ++i) { dp += an[i] * zk; zk *= z; }
        const double H2 = (std::abs(dp) > 0.0) ? std::norm(np / dp) : 0.0;
        if (H2 > Hmax2) Hmax2 = H2;
        sum_H2 += H2;
    }
    const double bw = (Hmax2 > 0.0)
        ? fs * sum_H2 / Hmax2 / double(Nsamp)
        : 0.0;
    return Value::scalar(bw, mr);
}

// Inverse regularized incomplete beta scalar wrapper (uses builtin Newton-iter
// implementation that operates on Value).
static double betaincinv_scalar(std::pmr::memory_resource *mr,
                                double p, double a, double b)
{
    if (a <= 0.0 || b <= 0.0) return std::numeric_limits<double>::quiet_NaN();
    if (p <= 0.0) return 0.0;
    if (p >= 1.0) return 1.0;
    Value pV = Value::scalar(p, mr);
    Value aV = Value::scalar(a, mr);
    Value bV = Value::scalar(b, mr);
    return ::numkit::builtin::betaincinv(pV, aV, bV, mr).toScalar();
}

std::tuple<Value, Value>
berconfint(double numErrs, double numBits, double level,
           std::pmr::memory_resource *mr)
{
    if (!(numBits > 0.0))
        throw Error("berconfint: numBits must be positive",
                    0, 0, "berconfint", "", "numkit:berconfint:nbits");
    if (numErrs < 0.0 || numErrs > numBits)
        throw Error("berconfint: numErrs must satisfy 0 ≤ numErrs ≤ numBits",
                    0, 0, "berconfint", "", "numkit:berconfint:nerrs");
    if (!(level > 0.0 && level < 1.0))
        throw Error("berconfint: level must lie in (0, 1)",
                    0, 0, "berconfint", "", "numkit:berconfint:level");

    const double k = numErrs;
    const double n = numBits;
    const double ber = k / n;
    const double alpha = 1.0 - level;

    // Clopper-Pearson exact binomial CI.
    double lo = (k == 0.0) ? 0.0
                           : betaincinv_scalar(mr, alpha / 2.0, k, n - k + 1.0);
    double hi = (k == n)   ? 1.0
                           : betaincinv_scalar(mr, 1.0 - alpha / 2.0, k + 1.0, n - k);

    Value berV = Value::scalar(ber, mr);
    Value ciV  = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    double *cd = ciV.doubleDataMut();
    cd[0] = lo;
    cd[1] = hi;
    return {std::move(berV), std::move(ciV)};
}

Value convertSNR(const Value &snr_in,
                 const std::string &in_type,
                 const std::string &out_type,
                 int bits_per_symbol,
                 std::pmr::memory_resource *mr)
{
    if (in_type == out_type) return snr_in;
    return elementwise(mr, snr_in, [&](double dB) {
        // Convert via Es/No as the pivot.
        double EsNo;
        if      (in_type == "ebno") EsNo = dB + 10.0 * std::log10(double(bits_per_symbol));
        else if (in_type == "esno") EsNo = dB;
        else /*snr*/                 EsNo = dB;  // assume Es/No = SNR (1 sample/symbol)
        if      (out_type == "ebno") return EsNo - 10.0 * std::log10(double(bits_per_symbol));
        else if (out_type == "esno") return EsNo;
        else                          return EsNo;
    });
}

} // namespace numkit::comm
