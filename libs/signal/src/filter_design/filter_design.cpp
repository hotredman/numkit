// libs/signal/src/filter_design/filter_design.cpp
//
// Butterworth IIR design (butter) + windowed-sinc FIR design (fir1).
// freqz / phasez / grpdelay (frequency-domain analysis of an existing
// filter) live in filter_analysis/frequency_response.cpp.

#include <numkit/signal/filter_design/filter_design.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"           // Complex typedef
#include "poly_helpers.hpp"             // polyExpandFromRoots

#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <memory_resource>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

namespace {

ScratchVec<Complex> butterworthPoles(std::pmr::memory_resource *mr, int N)
{
    ScratchVec<Complex> poles(mr);
    poles.reserve(N);
    for (int k = 0; k < N; ++k) {
        const double theta = M_PI * (2.0 * k + N + 1) / (2.0 * N);
        poles.emplace_back(std::cos(theta), std::sin(theta));
    }
    return poles;
}

using numkit::builtin::detail::polyExpandFromRoots;

// Bilinear-transform helper: take an arbitrary set of analog poles and
// zeros (already pre-warped + frequency-transformed) and produce the
// real-coefficient digital (b, a). Caller is responsible for the analog
// → analog transformations (LP scale, LP→HP); this function just maps
// each s-plane root through z = (2+s)/(2-s).
void bilinearTransformPZ(std::pmr::memory_resource *mr,
                         const Complex *sPoles, std::size_t pN,
                         const Complex *sZeros, std::size_t zN,
                         ScratchVec<double> &bOut,
                         ScratchVec<double> &aOut)
{
    ScratchVec<Complex> zPoles(pN, mr);
    for (std::size_t i = 0; i < pN; ++i) {
        const Complex sp = sPoles[i];
        zPoles[i] = (2.0 + sp) / (2.0 - sp);
    }
    aOut = polyExpandFromRoots(mr, zPoles.data(), zPoles.size());

    // Map each finite s-plane zero. Any "zero at infinity" in the analog
    // domain (count = pN - zN, classic for an N-pole all-pole prototype
    // like Butterworth LP) maps to z = -1 by the bilinear transform.
    const std::size_t totalZ = pN;
    ScratchVec<Complex> zZeros(totalZ, mr);
    for (std::size_t i = 0; i < zN; ++i) {
        const Complex sz = sZeros[i];
        zZeros[i] = (2.0 + sz) / (2.0 - sz);
    }
    for (std::size_t i = zN; i < totalZ; ++i)
        zZeros[i] = Complex(-1.0, 0.0);

    bOut = polyExpandFromRoots(mr, zZeros.data(), zZeros.size());
}

// Normalise b so that |H(z0)| == 1 at the reference frequency z0 (== 1
// for LP, == -1 for HP).
void normaliseAtRef(ScratchVec<double> &b, const ScratchVec<double> &a,
                    Complex z0)
{
    Complex num(0, 0), den(0, 0);
    Complex zk(1, 0);
    for (std::size_t i = 0; i < std::max(b.size(), a.size()); ++i) {
        if (i < b.size()) num += b[i] * zk;
        if (i < a.size()) den += a[i] * zk;
        zk *= z0;
    }
    const double mag = std::abs(num / den);
    if (mag > 0.0)
        for (double &v : b) v /= mag;
}

} // anonymous namespace

std::tuple<Value, Value>
butter(std::pmr::memory_resource *mr, int N, double Wn, const std::string &type)
{
    if (Wn <= 0.0 || Wn >= 1.0)
        throw Error("butter: Wn must be between 0 and 1",
                     0, 0, "butter", "", "m:butter:badWn");
    if (type != "low" && type != "high")
        throw Error("butter: type must be 'low' or 'high'",
                     0, 0, "butter", "", "m:butter:badType");

    // Pre-warp the digital cutoff to the analog domain.
    const double Wa = 2.0 * std::tan(M_PI * Wn / 2.0);

    ScratchArena scratch(mr);
    auto sPoles = butterworthPoles(&scratch, N);   // unit-cutoff prototype

    // Apply the LP scale or LP→HP transform IN THE ANALOG DOMAIN before
    // the bilinear map. For LP: s_k = sp_k * Wa, no finite zeros. For
    // HP: s_k = Wa / sp_k, plus N zeros at s = 0 (which map to z = 1
    // through the bilinear).
    ScratchVec<Complex> sP(static_cast<std::size_t>(N), &scratch);
    ScratchVec<Complex> sZ(&scratch);
    if (type == "low") {
        for (int i = 0; i < N; ++i) sP[i] = sPoles[i] * Wa;
        // sZ stays empty — Butterworth LP has all zeros at infinity.
    } else {
        for (int i = 0; i < N; ++i) sP[i] = Wa / sPoles[i];
        sZ.assign(static_cast<std::size_t>(N), Complex(0.0, 0.0));
    }

    ScratchVec<double> b(&scratch), a(&scratch);
    bilinearTransformPZ(&scratch, sP.data(), sP.size(),
                        sZ.data(), sZ.size(), b, a);

    // Normalise the gain at the reference frequency: DC (z=1) for LP,
    // Nyquist (z=-1) for HP.
    normaliseAtRef(b, a, type == "low" ? Complex(1.0, 0.0)
                                       : Complex(-1.0, 0.0));

    auto bv = Value::matrix(1, b.size(), ValueType::DOUBLE, mr);
    auto av = Value::matrix(1, a.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < b.size(); ++i)
        bv.doubleDataMut()[i] = b[i];
    for (size_t i = 0; i < a.size(); ++i)
        av.doubleDataMut()[i] = a[i];

    return std::make_tuple(std::move(bv), std::move(av));
}

// ── firls helpers ────────────────────────────────────────────────────

namespace {

// Solve A·x = b for symmetric positive-definite A via Cholesky
// (in-place on copies in scratch). A is n×n row-major.
// Returns false on non-PD pivot.
bool solveSPD(std::pmr::memory_resource *mr,
              const double *A_in, std::size_t n,
              const double *b_in, double *x_out)
{
    ScratchArena scratch(mr);
    ScratchVec<double> L(n * n, &scratch);
    ScratchVec<double> y(n, &scratch);

    // Cholesky: L·L' = A
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j <= i; ++j) {
            double s = A_in[i * n + j];
            for (std::size_t k = 0; k < j; ++k)
                s -= L[i * n + k] * L[j * n + k];
            if (i == j) {
                if (s <= 0.0) return false;
                L[i * n + j] = std::sqrt(s);
            } else {
                L[i * n + j] = s / L[j * n + j];
            }
        }
    }
    // Forward solve L·y = b
    for (std::size_t i = 0; i < n; ++i) {
        double s = b_in[i];
        for (std::size_t k = 0; k < i; ++k)
            s -= L[i * n + k] * y[k];
        y[i] = s / L[i * n + i];
    }
    // Back solve L'·x = y
    for (std::size_t ii = 0; ii < n; ++ii) {
        std::size_t i = n - 1 - ii;
        double s = y[i];
        for (std::size_t k = i + 1; k < n; ++k)
            s -= L[k * n + i] * x_out[k];
        x_out[i] = s / L[i * n + i];
    }
    return true;
}

// Antiderivative of cos(k·ω):
//   k == 0: ω
//   k != 0: sin(k·ω)/k
inline double Ic(int k, double w)
{
    return (k == 0) ? w : std::sin(k * w) / k;
}

// Antiderivative of (ω - ω0)·cos(k·ω) (for the linear-amp segment):
//   k == 0: (ω - ω0)²/2
//   k != 0: (ω - ω0)·sin(k·ω)/k + cos(k·ω)/k²
inline double Iclin(int k, double w, double w0)
{
    if (k == 0) {
        const double d = w - w0;
        return 0.5 * d * d;
    }
    return (w - w0) * std::sin(k * w) / k + std::cos(k * w) / (double(k) * k);
}

} // anonymous namespace

Value firls(std::pmr::memory_resource *mr, int N,
            const double *F, std::size_t Fn,
            const double *A, std::size_t An)
{
    if (N < 2 || (N % 2) != 0)
        throw Error("firls: filter order must be even (Type-I) and >= 2",
                    0, 0, "firls", "", "m:firls:badOrder");
    if (Fn == 0 || Fn != An)
        throw Error("firls: F and A must have the same non-empty length",
                    0, 0, "firls", "", "m:firls:badLen");
    if ((Fn % 2) != 0)
        throw Error("firls: F must have even length (band-edge pairs)",
                    0, 0, "firls", "", "m:firls:badLen");
    for (std::size_t i = 0; i < Fn; ++i)
        if (F[i] < 0.0 || F[i] > 1.0)
            throw Error("firls: F values must be in [0, 1]",
                        0, 0, "firls", "", "m:firls:badF");
    for (std::size_t i = 1; i < Fn; ++i)
        if (F[i] < F[i - 1])
            throw Error("firls: F must be non-decreasing",
                        0, 0, "firls", "", "m:firls:badF");

    const std::size_t M = static_cast<std::size_t>(N / 2);
    const std::size_t M1 = M + 1;
    const std::size_t numBands = Fn / 2;

    ScratchArena scratch(mr);
    ScratchVec<double> Q(M1 * M1, 0.0, &scratch);
    ScratchVec<double> bvec(M1, 0.0, &scratch);

    // For each band [w1, w2] = pi*[F[2k], F[2k+1]] with desired amp
    // linearly interpolated from D1=A[2k] to D2=A[2k+1]:
    //   Q[i,j] += (1/pi) * int_{w1}^{w2} cos(i*w)*cos(j*w) dw
    //          =  (1/(2*pi)) * [Ic(i+j, w2)-Ic(i+j, w1)
    //                           + Ic(|i-j|, w2)-Ic(|i-j|, w1)]
    //   bvec[i] += (1/pi) * int D(w)*cos(i*w) dw
    //   D(w) = D1 + slope*(w - w1), slope = (D2-D1)/(w2-w1)
    for (std::size_t k = 0; k < numBands; ++k) {
        const double w1 = M_PI * F[2 * k];
        const double w2 = M_PI * F[2 * k + 1];
        if (w2 <= w1) continue;
        const double D1 = A[2 * k];
        const double D2 = A[2 * k + 1];
        const double slope = (D2 - D1) / (w2 - w1);

        for (std::size_t i = 0; i < M1; ++i) {
            for (std::size_t j = 0; j < M1; ++j) {
                const int sij = static_cast<int>(i + j);
                const int dij = static_cast<int>(i > j ? i - j : j - i);
                const double Iplus  = Ic(sij, w2) - Ic(sij, w1);
                const double Iminus = Ic(dij, w2) - Ic(dij, w1);
                Q[i * M1 + j] += (Iplus + Iminus) / (2.0 * M_PI);
            }
            const int ii = static_cast<int>(i);
            // const-amp contribution: D1 * (sin(i*w)/i)|w1^w2  (i!=0)  or D1*(w2-w1) (i==0)
            const double Iconst = D1 * (Ic(ii, w2) - Ic(ii, w1)) / M_PI;
            // linear-amp contribution: slope * (∫(w - w1)·cos(i·ω) dω)|w1^w2
            const double Ilin = slope * (Iclin(ii, w2, w1) - Iclin(ii, w1, w1)) / M_PI;
            bvec[i] += Iconst + Ilin;
        }
    }

    ScratchVec<double> c(M1, 0.0, &scratch);
    if (!solveSPD(&scratch, Q.data(), M1, bvec.data(), c.data()))
        throw Error("firls: Q matrix is not positive-definite",
                    0, 0, "firls", "", "m:firls:singular");

    // Reconstruct symmetric impulse response of length N+1.
    // c[0] is the center coefficient h[M]; c[k] for k>=1 is 2·h[M-k].
    auto bv = Value::matrix(1, M1 + M, ValueType::DOUBLE, mr);
    auto *bd = bv.doubleDataMut();
    bd[M] = c[0];
    for (std::size_t k = 1; k <= M; ++k) {
        const double v = 0.5 * c[k];
        bd[M - k] = v;
        bd[M + k] = v;
    }
    return bv;
}

Value fir1(std::pmr::memory_resource *mr, int N, double Wn, const std::string &type)
{
    if (Wn <= 0.0 || Wn >= 1.0)
        throw Error("fir1: Wn must be between 0 and 1",
                     0, 0, "fir1", "", "m:fir1:badWn");
    if (type != "low" && type != "high")
        throw Error("fir1: type must be 'low' or 'high'",
                     0, 0, "fir1", "", "m:fir1:badType");

    const size_t filtLen = N + 1;
    const double wc = M_PI * Wn;
    const double half = N / 2.0;

    ScratchArena scratch(mr);
    auto h = ScratchVec<double>(filtLen, &scratch);
    double hSum = 0.0;

    for (size_t i = 0; i < filtLen; ++i) {
        const double n = i - half;
        const double sinc = (std::abs(n) < 1e-12) ? wc / M_PI
                                                  : std::sin(wc * n) / (M_PI * n);
        const double win = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / N);
        h[i] = sinc * win;
        hSum += h[i];
    }

    if (type == "low") {
        for (size_t i = 0; i < filtLen; ++i)
            h[i] /= hSum;
    } else { // "high"
        for (size_t i = 0; i < filtLen; ++i)
            h[i] /= hSum;
        for (size_t i = 0; i < filtLen; ++i)
            h[i] = -h[i];
        h[static_cast<size_t>(half)] += 1.0;
    }

    auto bv = Value::matrix(1, filtLen, ValueType::DOUBLE, mr);
    for (size_t i = 0; i < filtLen; ++i)
        bv.doubleDataMut()[i] = h[i];
    return bv;
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void butter_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("butter: requires at least 2 arguments",
                     0, 0, "butter", "", "m:butter:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const double Wn = args[1].toScalar();
    std::string type = "low";
    if (args.size() >= 3 && args[2].isChar())
        type = args[2].toString();

    auto [bv, av] = butter(ctx.engine->resource(), N, Wn, type);
    outs[0] = std::move(bv);
    if (nargout > 1)
        outs[1] = std::move(av);
}

void fir1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fir1: requires at least 2 arguments",
                     0, 0, "fir1", "", "m:fir1:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const double Wn = args[1].toScalar();
    std::string type = "low";
    if (args.size() >= 3 && args[2].isChar())
        type = args[2].toString();

    outs[0] = fir1(ctx.engine->resource(), N, Wn, type);
}

void firls_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("firls: requires 3 arguments (N, F, A)",
                    0, 0, "firls", "", "m:firls:nargin");
    const int N = static_cast<int>(args[0].toScalar());

    auto extractRow = [](const Value &v, std::vector<double> &dst) {
        const std::size_t n = v.numel();
        dst.resize(n);
        for (std::size_t i = 0; i < n; ++i) dst[i] = v.elemAsDouble(i);
    };

    std::vector<double> Fv, Av;
    extractRow(args[1], Fv);
    extractRow(args[2], Av);

    outs[0] = firls(ctx.engine->resource(), N,
                    Fv.data(), Fv.size(), Av.data(), Av.size());
}

} // namespace detail

} // namespace numkit::signal
