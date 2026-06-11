// toolboxes/signal/src/filter_design/filter_design.cpp
// Butterworth IIR design (butter) + windowed-sinc FIR design (fir1).
// freqz / phasez / grpdelay (frequency-domain analysis of an existing
// filter) live in filter_analysis/frequency_response.cpp.

#include <numkit/signal/filter_design/filter_design.hpp>
#include <numkit/signal/transforms/fft.hpp>
#include <numkit/math/poly/polynomials.hpp>  // tf2zp (ZPK 3-output)

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

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

// Extract a numeric Value (any class) into a contiguous double row on
// the supplied arena. Mirrors the `extractRow` lambda that used to be
// duplicated in every filter_design dispatcher.
ScratchVec<double> valueToDoubleRow(const Value &v, ScratchArena &arena)
{
    const std::size_t n = v.numel();
    ScratchVec<double> dst(n, &arena);
    for (std::size_t i = 0; i < n; ++i)
        dst[i] = v.elemAsDouble(i);
    return dst;
}

ScratchVec<Complex> butterworthPoles(int N, std::pmr::memory_resource *mr)
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
void bilinearTransformPZ(const Complex *sPoles, std::size_t pN, const Complex *sZeros, std::size_t zN, ScratchVec<double> &bOut, ScratchVec<double> &aOut, std::pmr::memory_resource *mr)
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
butter(int N, double Wn, const std::string &type, std::pmr::memory_resource *mr)
{
    if (Wn <= 0.0 || Wn >= 1.0)
        throw Error("butter: Wn must be between 0 and 1",
                     0, 0, "butter", "", "numkit:butter:badWn");
    if (type != "low" && type != "high")
        throw Error("butter: type must be 'low' or 'high'",
                     0, 0, "butter", "", "numkit:butter:badType");

    // Pre-warp the digital cutoff to the analog domain.
    const double Wa = 2.0 * std::tan(M_PI * Wn / 2.0);

    ScratchArena scratch(mr);
    auto sPoles = butterworthPoles(N, &scratch);   // unit-cutoff prototype

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
    bilinearTransformPZ(sP.data(), sP.size(), sZ.data(), sZ.size(), b, a, &scratch);

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
bool solveSPD(const double *A_in, std::size_t n, const double *b_in, double *x_out, std::pmr::memory_resource *mr)
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

Value firls(int N, const Value &Farg, const Value &Aarg,
            std::pmr::memory_resource *mr)
{
    if (N < 2 || (N % 2) != 0)
        throw Error("firls: filter order must be even (Type-I) and >= 2",
                    0, 0, "firls", "", "numkit:firls:badOrder");

    ScratchArena scratch(mr);
    auto Fv = valueToDoubleRow(Farg, scratch);
    auto Av = valueToDoubleRow(Aarg, scratch);
    const double *F = Fv.data();
    const double *A = Av.data();
    const std::size_t Fn = Fv.size();
    const std::size_t An = Av.size();

    if (Fn == 0 || Fn != An)
        throw Error("firls: F and A must have the same non-empty length",
                    0, 0, "firls", "", "numkit:firls:badLen");
    if ((Fn % 2) != 0)
        throw Error("firls: F must have even length (band-edge pairs)",
                    0, 0, "firls", "", "numkit:firls:badLen");
    for (std::size_t i = 0; i < Fn; ++i)
        if (F[i] < 0.0 || F[i] > 1.0)
            throw Error("firls: F values must be in [0, 1]",
                        0, 0, "firls", "", "numkit:firls:badF");
    for (std::size_t i = 1; i < Fn; ++i)
        if (F[i] < F[i - 1])
            throw Error("firls: F must be non-decreasing",
                        0, 0, "firls", "", "numkit:firls:badF");

    const std::size_t M = static_cast<std::size_t>(N / 2);
    const std::size_t M1 = M + 1;
    const std::size_t numBands = Fn / 2;

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
    if (!solveSPD(Q.data(), M1, bvec.data(), c.data(), &scratch))
        throw Error("firls: Q matrix is not positive-definite",
                    0, 0, "firls", "", "numkit:firls:singular");

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

Value fir1(int N, double Wn, const std::string &type, std::pmr::memory_resource *mr)
{
    if (Wn <= 0.0 || Wn >= 1.0)
        throw Error("fir1: Wn must be between 0 and 1",
                     0, 0, "fir1", "", "numkit:fir1:badWn");
    if (type != "low" && type != "high")
        throw Error("fir1: type must be 'low' or 'high'",
                     0, 0, "fir1", "", "numkit:fir1:badType");

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

// ── cell2sos (Phase 4.10) ─────────────────────────────────────────────
// Convert cell array C = {{B1, A1}, {B2, A2}, ...} → L×6 SOS matrix.
// Each Bi/Ai is zero-padded on the right to length 3. 2-output form
// extracts a leading scalar gain section if present.

std::tuple<Value, Value>
cell2sos(const Value &C, std::pmr::memory_resource *mr)
{
    if (!C.isCell())
        throw Error("cell2sos: input must be a cell array",
                    0, 0, "cell2sos", "", "numkit:cell2sos:NotCell");
    const std::size_t L = C.numel();
    if (L == 0) {
        Value empty = Value::matrix(0, 6, ValueType::DOUBLE, mr);
        return {empty, Value::scalar(1.0, mr)};
    }

    // Detect leading-scalar gain section.
    double g = 1.0;
    std::size_t startIdx = 0;
    {
        const Value &first = C.cellAt(0);
        if (first.isCell() && first.numel() == 2) {
            const Value &b0 = first.cellAt(0);
            const Value &a0 = first.cellAt(1);
            if (b0.numel() == 1 && a0.numel() == 1) {
                const double bv = b0.toScalar();
                const double av = a0.toScalar();
                if (av != 0.0) {
                    g = bv / av;
                    startIdx = 1;
                }
            }
        }
    }

    const std::size_t outRows = L - startIdx;
    Value S = Value::matrix(outRows, 6, ValueType::DOUBLE, mr);
    if (outRows == 0) {
        return {S, Value::scalar(g, mr)};
    }

    double *Sd = S.doubleDataMut();
    std::fill(Sd, Sd + outRows * 6, 0.0);
    for (std::size_t i = 0; i < outRows; ++i) {
        const Value &pair = C.cellAt(startIdx + i);
        if (!pair.isCell() || pair.numel() != 2)
            throw Error("cell2sos: each cell must be {B, A} pair",
                        0, 0, "cell2sos", "", "numkit:cell2sos:BadPair");
        const Value &bv = pair.cellAt(0);
        const Value &av = pair.cellAt(1);
        if (bv.numel() > 3 || av.numel() > 3)
            throw Error("cell2sos: each B/A must have at most 3 elements",
                        0, 0, "cell2sos", "", "numkit:cell2sos:OrderTooHigh");
        for (std::size_t k = 0; k < bv.numel(); ++k)
            Sd[i + k * outRows] = bv.elemAsDouble(k);
        for (std::size_t k = 0; k < av.numel(); ++k)
            Sd[i + (3 + k) * outRows] = av.elemAsDouble(k);
    }
    return {S, Value::scalar(g, mr)};
}


// ── fir2 — frequency-sampling FIR filter design ──────────────────────
// Clean-room implementation written from the
// public references it cites:
//   * A. V. Oppenheim & R. W. Schafer, Discrete-Time Signal Processing,
//     3rd ed., 2010 — §7.4-7.5, FIR design by the frequency-sampling
//     method (sample the desired response on a uniform grid, inverse-
//     transform, window);
//   * L. R. Rabiner & B. Gold, Theory and Application of Digital Signal
//     Processing, 1975 — frequency-sampling FIR design;
//   * T. W. Parks & C. S. Burrus, Digital Filter Design, 1987.
//   b = fir2(n, f, m [, npt] [, lap] [, window])
// Piecewise-linearly interpolate the desired (f, m) magnitude response
// onto a uniform DC..Nyquist grid, apply a linear-phase delay, inverse-
// transform, and window. Full MATLAB argument set: npt, lap, window,
// plus the odd-order Nyquist correction.

namespace {

// Smallest power of two >= v (for v >= 1).
int nextPow2Int(int v)
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

// Read a short real vector (row or column) into a ScratchVec<double>.
void readVec(const Value &v, ScratchVec<double> &out)
{
    const size_t n = v.numel();
    out.resize(n);
    for (size_t i = 0; i < n; ++i)
        out[i] = v.elemAsDouble(i);
}

} // anonymous namespace

Value fir2(int n, const Value &f, const Value &m,
           const Fir2Options &opts, std::pmr::memory_resource *mr)
{
    ScratchArena arena(mr);

    // ── 1. Validate n ──────────────────────────────────────────────────
    if (n <= 0)
        throw numkit::Error("n must be a positive integer.", 0, 0, "fir2", "",
                            "numkit:fir2:BadN");

    // ── 2. Read and validate f, m ──────────────────────────────────────
    ScratchVec<double> fv(0, &arena), mv(0, &arena);
    readVec(f, fv);
    readVec(m, mv);

    if (fv.size() != mv.size())
        throw numkit::Error("The frequency and magnitude vectors must be the "
                            "same length.",
                            0, 0, "fir2", "", "numkit:fir2:MismatchedDimensions");
    if (fv.size() < 2)
        throw numkit::Error("The frequency vector must have at least 2 "
                            "elements.",
                            0, 0, "fir2", "", "numkit:fir2:BadFLen");

    const size_t nf = fv.size();

    // f must start at 0 and end at 1.
    if (fv[0] != 0.0 || fv[nf - 1] != 1.0)
        throw numkit::Error("The first frequency must be 0 and the last 1.",
                            0, 0, "fir2", "", "numkit:fir2:InvalidRange");

    // f must be non-decreasing.
    for (size_t i = 1; i < nf; ++i) {
        if (fv[i] < fv[i - 1])
            throw numkit::Error("Frequencies must be non-decreasing.", 0, 0,
                                "fir2", "", "numkit:fir2:InvalidFreqVec");
    }

    // ── 3. Odd-order correction (spec §2.1) ────────────────────────────
    // A symmetric FIR filter of odd order has a forced zero at Nyquist.
    // If n is odd and the requested Nyquist magnitude is non-zero, bump n.
    if ((n % 2) == 1 && mv[nf - 1] != 0.0)
        n += 1;

    const int nn = n + 1;  // final filter length

    // ── 4. Grid size npt (spec §2.2) ───────────────────────────────────
    const bool nptDefaulted = (opts.npt <= 0);
    int npt = nptDefaulted ? 512 : opts.npt;

    if (!nptDefaulted) {
        // A user-supplied npt must satisfy 2*npt >= nn.
        if (2 * npt < nn)
            throw numkit::Error("The number of grid points must be greater "
                                "than or equal to ceil(nn/2).",
                                0, 0, "fir2", "", "numkit:fir2:InvalidNpt");
    } else {
        // Defaulted: grow the request if the filter is longer than 2*512.
        if (2 * npt < nn)
            npt = (nn + 1) / 2;  // ceil(nn/2)
    }

    // Working grid size = requested npt rounded up to a power of two.
    npt = nextPow2Int(npt);

    // ── 5. lap (spec §2.2 default) ─────────────────────────────────────
    int lap = (opts.lap > 0) ? opts.lap : (npt / 25);

    // ── 6. Build the half-band response H (spec §2.3) ──────────────────
    // H has npt+1 points covering DC..Nyquist; grid point j (1-based)
    // is normalised frequency (j-1)/npt.
    const int npt1 = npt + 1;
    ScratchVec<Complex> H(static_cast<size_t>(npt1), &arena);
    for (int k = 0; k < npt1; ++k)
        H[k] = Complex(0.0, 0.0);

    // Helper: range-checked 1-based store into H.
    auto putH = [&](int j1, double val) {
        if (j1 < 1 || j1 > npt1)
            throw numkit::Error("Internal grid index out of range.", 0, 0,
                                "fir2", "", "numkit:fir2:SignalErr");
        H[j1 - 1] = Complex(val, 0.0);
    };

    putH(1, mv[0]);
    int nb = 1;  // 1-based start of the next segment

    for (size_t i = 0; i + 1 < nf; ++i) {
        const double df = fv[i + 1] - fv[i];
        int ne;
        if (df == 0.0) {
            // Duplicated frequency — a response discontinuity. Back up
            // half the smoothing window, then ramp across `lap` points.
            nb = static_cast<int>(std::ceil(nb - lap / 2.0));
            ne = nb + lap;
        } else {
            ne = static_cast<int>(std::floor(fv[i + 1] * npt1));
        }

        if (nb < 1 || ne > npt1)
            throw numkit::Error("Internal grid index out of range.", 0, 0,
                                "fir2", "", "numkit:fir2:SignalErr");

        for (int j = nb; j <= ne; ++j) {
            double inc;
            if (ne == nb)
                inc = 0.0;
            else
                inc = static_cast<double>(j - nb)
                      / static_cast<double>(ne - nb);
            putH(j, inc * mv[i + 1] + (1.0 - inc) * mv[i]);
        }
        nb = ne + 1;
    }

    // ── 7. Linear-phase delay (spec §2.4) ──────────────────────────────
    // Multiply by a pure delay of dt = (nn-1)/2 samples.
    const double dt = (nn - 1) / 2.0;
    for (int k = 0; k <= npt; ++k) {
        const double phase = -M_PI * dt * k / npt;
        H[k] *= Complex(std::cos(phase), std::sin(phase));
    }

    // ── 8. Mirror, inverse transform (spec §2.5) ───────────────────────
    // Full 2*npt spectrum by Hermitian mirroring (excl. DC and Nyquist):
    //   Hfull = [ H[0..npt], conj(H[npt-1]), ..., conj(H[1]) ]
    const int nFull = 2 * npt;  // power of two
    Value spectrum =
        Value::matrix(static_cast<size_t>(nFull), 1, ValueType::COMPLEX, mr);
    Complex *sp = spectrum.complexDataMut();
    for (int k = 0; k <= npt; ++k)
        sp[k] = H[k];
    for (int k = 1; k < npt; ++k)
        sp[npt + k] = std::conj(H[npt - k]);

    // Inverse DFT of length 2*npt (already normalised by the transform).
    Value timeDom = ifft(spectrum, nFull, 1, &arena);

    // ── 9. Take the first nn samples and window (spec §2.5) ────────────
    Value b = Value::matrix(1, static_cast<size_t>(nn), ValueType::DOUBLE, mr);
    double *bd = b.doubleDataMut();

    // The window: user-supplied or a default Hamming of length nn.
    ScratchVec<double> win(0, &arena);
    if (!opts.window.isEmpty()) {
        if (static_cast<int>(opts.window.numel()) != nn)
            throw numkit::Error("The window length must equal the filter "
                                "length.",
                                0, 0, "fir2", "", "numkit:fir2:BadWindow");
        readVec(opts.window, win);
    } else {
        win.resize(static_cast<size_t>(nn));
        if (nn == 1) {
            win[0] = 1.0;
        } else {
            for (int k = 0; k < nn; ++k)
                win[k] = 0.54
                         - 0.46 * std::cos(2.0 * M_PI * k / (nn - 1));
        }
    }

    // ifft may return DOUBLE (conjugate-symmetric input) or COMPLEX.
    if (timeDom.type() == ValueType::COMPLEX) {
        const Complex *td = timeDom.complexData();
        for (int k = 0; k < nn; ++k)
            bd[k] = td[k].real() * win[k];
    } else {
        const double *td = timeDom.doubleData();
        for (int k = 0; k < nn; ++k)
            bd[k] = td[k] * win[k];
    }

    return b;
}


// ── firpm (Parks-McClellan, Type I only — even N) ────────────────────
// Equiripple FIR design via Remez exchange. Reference: McClellan, Parks,
// Rabiner, 1973 (FORTRAN), Burrus DSP texts. Algorithm:
//   For Type I (N even, h symmetric of length N+1):
//     H(ω) = Σ_{k=0..L} a[k] cos(kω),  L = N/2
//   Minimize  max_ω∈F  |W(ω) · (D(ω) - H(ω))|  via the alternation
//   theorem: H equioscillates at L+2 extremal frequencies with sign
//   (-1)^k · δ where δ is the peak ripple.
//   1. Dense grid: lgrid·(L+2) points covering all bands proportionally.
//   2. Initial extremals: equispaced over the grid.
//   3. Repeat until max|E| - |δ| ≈ 0:
//      a. Lagrange weights γk on x_k = cos(ω_k).
//      b. Solve for δ via the closed-form ratio (Cheney/Powell).
//      c. C_k = D_k - (-1)^k · δ / W_k  (interpolation values).
//      d. Barycentric Lagrange evaluation of H on the dense grid.
//      e. New extremals: alternating-sign local maxima of |W·(D-H)|.
//   4. Final cosine coefficients via L+1 cosine-node samples + inverse
//      DCT-I; reconstruct symmetric h.
namespace {

// Barycentric Lagrange eval of degree-(m-1) polynomial defined by
// (x_k, y_k) and precomputed weights g_k = 1/Π_{j≠k}(x_k - x_j).
inline double bary_eval(const double *xk, const double *yk, const double *gk,
                        std::size_t m, double x)
{
    for (std::size_t k = 0; k < m; ++k)
        if (std::fabs(x - xk[k]) < 1e-15) return yk[k];
    double num = 0.0, den = 0.0;
    for (std::size_t k = 0; k < m; ++k) {
        const double t = gk[k] / (x - xk[k]);
        num += t * yk[k];
        den += t;
    }
    return num / den;
}

struct PMGridPoint {
    double w;     // ω in [0, π]
    double D;     // desired amplitude
    double W;     // weight
    int    band;  // index of source band
};

} // anonymous namespace

std::tuple<Value, double>
firpm(int N, const Value &Farg, const Value &Aarg, const Value &Warg,
      const std::string &ftype, std::pmr::memory_resource *mr)
{
    if (N < 3)
        throw Error("Filter order must be 3 or more",
                    0, 0, "firpm", "", "numkit:firpm:badOrder");

    ScratchArena arena(mr);
    auto Fv = valueToDoubleRow(Farg, arena);
    auto Av = valueToDoubleRow(Aarg, arena);
    auto Wv = valueToDoubleRow(Warg, arena);
    const double *F = Fv.data();
    const double *A = Av.data();
    const double *W = Wv.empty() ? nullptr : Wv.data();
    const std::size_t Fn = Fv.size();
    const std::size_t An = Av.size();
    const std::size_t Wn = Wv.size();

    if (Fn == 0 || Fn != An || (Fn % 2) != 0)
        throw Error("firpm: F and A must be non-empty equal-length even-length",
                    0, 0, "firpm", "", "numkit:firpm:badLen");
    const std::size_t numBands = Fn / 2;
    if (W != nullptr && Wn != numBands)
        throw Error("firpm: W must have one weight per band",
                    0, 0, "firpm", "", "numkit:firpm:badW");
    for (std::size_t i = 0; i < Fn; ++i)
        if (F[i] < 0.0 || F[i] > 1.0)
            throw Error("firpm: F values must be in [0, 1]",
                        0, 0, "firpm", "", "numkit:firpm:badF");
    for (std::size_t i = 1; i < Fn; ++i)
        if (F[i] < F[i - 1])
            throw Error("firpm: F must be non-decreasing",
                        0, 0, "firpm", "", "numkit:firpm:badF");

    // Identify the linear-phase FIR type:
    //   Type I  (even N, symmetric)       H(ω) = Σ a[k]·cos(kω)
    //   Type II (odd  N, symmetric)       H(ω) = cos(ω/2) · Σ a[k]·cos(kω)
    //   Type III(even N, anti-symmetric)  H(ω) = sin(ω)   · Σ a[k]·cos(kω)
    //   Type IV (odd  N, anti-symmetric)  H(ω) = sin(ω/2) · Σ a[k]·cos(kω)
    // All four reduce to a single canonical polynomial-Chebyshev
    // problem via the Q(ω) type factor: D'(ω) = D / Q, W'(ω) = W · Q,
    // then run the same Remez kernel. Reconstruction back to h[k]
    // differs per type — handled in the final section.
    // Polynomial degree L (so we have L+1 cosine coefficients a[0..L]):
    //   Type I  : L = N/2
    //   Type II : L = (N-1)/2
    //   Type III: L = N/2 - 1
    //   Type IV : L = (N-1)/2
    auto lower_eq = [](const std::string &s, const char *t) {
        if (s.size() != std::strlen(t)) return false;
        for (std::size_t i = 0; i < s.size(); ++i)
            if (std::tolower(s[i]) != std::tolower(t[i])) return false;
        return true;
    };
    const bool isHilbert   = lower_eq(ftype, "hilbert");
    const bool isDiff      = lower_eq(ftype, "differentiator");
    const bool antiSym     = isHilbert || isDiff;
    if (!ftype.empty() && !isHilbert && !isDiff)
        throw Error("firpm: ftype must be 'hilbert' or 'differentiator'",
                    0, 0, "firpm", "", "numkit:firpm:badFtype");
    enum { TYPE_I = 1, TYPE_II = 2, TYPE_III = 3, TYPE_IV = 4 } ;
    int filterType;
    if (!antiSym) filterType = (N % 2 == 0) ? TYPE_I  : TYPE_II;
    else          filterType = (N % 2 == 0) ? TYPE_III : TYPE_IV;
    std::size_t L = 0;
    switch (filterType) {
        case TYPE_I:   L = static_cast<std::size_t>(N) / 2;     break;
        case TYPE_II:  L = static_cast<std::size_t>(N - 1) / 2; break;
        case TYPE_III: L = static_cast<std::size_t>(N) / 2 - 1; break;
        case TYPE_IV:  L = static_cast<std::size_t>(N - 1) / 2; break;
    }
    const std::size_t r     = L + 1;       // # cosine basis coefficients
    const std::size_t nExtr = r + 1;       // # extremal frequencies (= L+2)
    constexpr int lgrid = 16;              // dense-grid density per extremal

    // Total fractional width across all bands (proportional grid alloc).
    double totalWidth = 0.0;
    for (std::size_t k = 0; k < numBands; ++k)
        totalWidth += F[2 * k + 1] - F[2 * k];
    if (totalWidth <= 0.0)
        throw Error("firpm: all bands have zero width",
                    0, 0, "firpm", "", "numkit:firpm:badF");

    const std::size_t gridTarget =
        static_cast<std::size_t>(lgrid) * (L + 2);

    ScratchVec<PMGridPoint> grid(&arena);
    grid.reserve(gridTarget + 4 * numBands);

    for (std::size_t k = 0; k < numBands; ++k) {
        const double f1 = F[2 * k], f2 = F[2 * k + 1];
        const double bw = f2 - f1;
        if (bw <= 0.0) continue;
        const double a1 = A[2 * k], a2 = A[2 * k + 1];
        const double wt = (W ? W[k] : 1.0);
        if (wt <= 0.0)
            throw Error("firpm: weights must be positive",
                        0, 0, "firpm", "", "numkit:firpm:badW");
        std::size_t nb = static_cast<std::size_t>(
            std::ceil(double(gridTarget) * bw / totalWidth));
        if (nb < 4) nb = 4;
        // Differentiator: apply the 1/f weighting only in NON-ZERO
        // amplitude bands (where A(l+1) >= 0.0001). Stopbands keep the
        // user weight.
        const bool diffActive = isDiff && std::fabs(a2) >= 1e-4;
        for (std::size_t i = 0; i < nb; ++i) {
            const double t = double(i) / double(nb - 1);
            PMGridPoint p;
            p.w = M_PI * (f1 + t * bw);
            // Desired amplitude — straight linear-interp between band
            // edges. MATLAB DOES NOT pre-scale by f for differentiator;
            // the linear-in-frequency desired comes from the user passing
            // A = [0 slope*Fmax] (e.g. A=[0 0.9] → D rises 0 → 0.9 across).
            const double D_raw = a1 + t * (a2 - a1);
            // Q-factor + endpoint skips per filter type.
            double Q = 1.0;
            switch (filterType) {
                case TYPE_I:
                    Q = 1.0;
                    break;
                case TYPE_II:
                    if (p.w > M_PI * 0.99999) continue;
                    Q = std::cos(0.5 * p.w);
                    break;
                case TYPE_III:
                    if (p.w < M_PI * 1e-5 || p.w > M_PI * 0.99999) continue;
                    Q = std::sin(p.w);
                    break;
                case TYPE_IV:
                    if (p.w < M_PI * 1e-5) continue;
                    Q = std::sin(0.5 * p.w);
                    break;
            }
            // Apply user weight + 1/f scaling for differentiator
            // passbands (matching firpmfrf): W_eff = wt / (f_norm/2)
            // where f_norm = ω/π ∈ [0, 1].
            double W_eff = wt;
            if (diffActive) {
                const double f_norm = p.w / M_PI;
                if (f_norm < 1e-12) continue;
                W_eff = wt / (0.5 * f_norm);
            }
            if (filterType != TYPE_I) {
                p.D = D_raw / Q;
                p.W = W_eff * Q;
            } else {
                p.D = D_raw;
                p.W = W_eff;
            }
            p.band = int(k);
            grid.push_back(p);
        }
    }
    if (grid.size() < nExtr + 1)
        throw Error("firpm: dense grid too small for filter order",
                    0, 0, "firpm", "", "numkit:firpm:internal");

    // Initial extremals — distribute proportionally to band widths so
    // every band gets at least 2 candidates (its edges) and wider bands
    // get more. MPR73-style band-aware seed.
    ScratchVec<std::size_t> extr(nExtr, &arena);
    {
        // Locate band span in the grid.
        ScratchVec<std::size_t> bandStart(numBands, &arena);
        ScratchVec<std::size_t> bandEnd  (numBands, &arena);
        int curBand = -1;
        for (std::size_t i = 0; i < grid.size(); ++i) {
            const int b = grid[i].band;
            if (b != curBand) {
                bandStart[b] = i;
                if (curBand >= 0) bandEnd[curBand] = i - 1;
                curBand = b;
            }
        }
        bandEnd[curBand] = grid.size() - 1;

        // Distribute nExtr across bands proportional to width, with at
        // least 2 per band (band edges).
        ScratchVec<std::size_t> perBand(numBands, &arena);
        std::size_t assigned = 0;
        for (std::size_t k = 0; k < numBands; ++k) {
            const double bw = F[2 * k + 1] - F[2 * k];
            std::size_t n =
                std::max<std::size_t>(2,
                    static_cast<std::size_t>(std::round(
                        double(nExtr) * bw / totalWidth)));
            perBand[k] = n;
            assigned   += n;
        }
        // Rebalance to total exactly nExtr.
        while (assigned > nExtr) {
            std::size_t kmax = 0;
            for (std::size_t k = 1; k < numBands; ++k)
                if (perBand[k] > perBand[kmax]) kmax = k;
            if (perBand[kmax] <= 2) break;
            perBand[kmax]--;
            assigned--;
        }
        while (assigned < nExtr) {
            std::size_t kmax = 0;
            for (std::size_t k = 1; k < numBands; ++k)
                if (perBand[k] > perBand[kmax]) kmax = k;
            perBand[kmax]++;
            assigned++;
        }
        // Lay them down per band (equispaced including both edges).
        std::size_t out = 0;
        for (std::size_t k = 0; k < numBands; ++k) {
            const std::size_t n = perBand[k];
            const std::size_t s = bandStart[k], e = bandEnd[k];
            for (std::size_t i = 0; i < n && out < nExtr; ++i) {
                std::size_t idx = (n == 1)
                    ? s
                    : s + ((e - s) * i) / (n - 1);
                extr[out++] = idx;
            }
        }
    }

    // Per-iteration scratch.
    ScratchVec<double> xk(nExtr, &arena), gammak(nExtr, &arena), Ck(nExtr, &arena);
    ScratchVec<double> xkP(r, &arena), CkP(r, &arena), gkP(r, &arena);
    ScratchVec<double> err_grid(grid.size(), &arena);
    ScratchVec<std::size_t> newExtr(&arena);
    newExtr.reserve(nExtr + 8);

    double delta = 0.0;
    constexpr int kMaxIter = 50;
    constexpr double kConvTol = 1e-9;

    for (int iter = 0; iter < kMaxIter; ++iter) {
        // (a) Lagrange weights γk on extremal x's.
        for (std::size_t k = 0; k < nExtr; ++k)
            xk[k] = std::cos(grid[extr[k]].w);
        for (std::size_t k = 0; k < nExtr; ++k) {
            double prod = 1.0;
            for (std::size_t j = 0; j < nExtr; ++j)
                if (j != k) prod *= (xk[k] - xk[j]);
            gammak[k] = 1.0 / prod;
        }

        // (b) δ — peak ripple of current Chebyshev approximation.
        double num = 0.0, den = 0.0;
        for (std::size_t k = 0; k < nExtr; ++k) {
            num += gammak[k] * grid[extr[k]].D;
            const double sg = (k % 2 == 0) ? 1.0 : -1.0;
            den += sg * gammak[k] / grid[extr[k]].W;
        }
        delta = num / den;

        // (c) C_k — interpolation values for polynomial through L+1 of the
        //     extremals (last one's residual matches the δ alternation).
        for (std::size_t k = 0; k < nExtr; ++k) {
            const double sg = (k % 2 == 0) ? 1.0 : -1.0;
            Ck[k] = grid[extr[k]].D - sg * delta / grid[extr[k]].W;
        }

        // Build degree-L polynomial through the first L+1 extremals.
        for (std::size_t k = 0; k < r; ++k) {
            xkP[k] = xk[k];
            CkP[k] = Ck[k];
        }
        for (std::size_t k = 0; k < r; ++k) {
            double prod = 1.0;
            for (std::size_t j = 0; j < r; ++j)
                if (j != k) prod *= (xkP[k] - xkP[j]);
            gkP[k] = 1.0 / prod;
        }

        // (d) Error E(ω) on the full grid.
        for (std::size_t i = 0; i < grid.size(); ++i) {
            int extrIdx = -1;
            for (std::size_t k = 0; k < nExtr; ++k)
                if (extr[k] == i) { extrIdx = int(k); break; }
            if (extrIdx >= 0) {
                const double sg = (extrIdx % 2 == 0) ? 1.0 : -1.0;
                err_grid[i] = sg * delta;
                continue;
            }
            const double xi = std::cos(grid[i].w);
            const double Hi = bary_eval(xkP.data(), CkP.data(), gkP.data(), r, xi);
            err_grid[i] = grid[i].W * (grid[i].D - Hi);
        }

        // (e) Multiple-exchange Remez update: find all local |E| maxima
        //     within bands, greedy-merge to alternating sequence, trim
        //     ends until exactly nExtr remain. Robust for arbitrary
        //     weighted band patterns — every iteration globally scans the
        //     grid so sign-locked stalls (which plague single-exchange)
        //     cannot happen.
        newExtr.clear();
        {
            ScratchVec<std::size_t> peaks(&arena);
            peaks.reserve(grid.size() / 2 + 8);
            for (std::size_t i = 0; i < grid.size(); ++i) {
                const double absE = std::fabs(err_grid[i]);
                const bool sameBandLeft  =
                    (i > 0) && (grid[i - 1].band == grid[i].band);
                const bool sameBandRight =
                    (i + 1 < grid.size()) && (grid[i + 1].band == grid[i].band);
                const double absL =
                    sameBandLeft ? std::fabs(err_grid[i - 1]) : -1.0;
                const double absR =
                    sameBandRight ? std::fabs(err_grid[i + 1]) : -1.0;
                bool isPeak;
                if (!sameBandLeft || !sameBandRight) {
                    // Band edge — always a candidate (matches classical
                    // MPR73 / Remez: alternation theorem reaches its
                    // L+2 extrema using both interior peaks AND every
                    // band edge as a candidate; the alternation-merge
                    // below filters out edges whose error sign matches
                    // an adjacent peak with larger |E|).
                    isPeak = (absE > 0.0);
                } else {
                    // Interior of a band — strict local maximum.
                    isPeak = (absE > absL) && (absE >= absR);
                }
                if (isPeak) peaks.push_back(i);
            }
            for (std::size_t i : peaks) {
                if (newExtr.empty()) { newExtr.push_back(i); continue; }
                const double eLast = err_grid[newExtr.back()];
                const double e     = err_grid[i];
                if ((e >= 0.0) == (eLast >= 0.0)) {
                    if (std::fabs(e) > std::fabs(eLast)) newExtr.back() = i;
                } else {
                    newExtr.push_back(i);
                }
            }
            while (newExtr.size() > nExtr) {
                if (std::fabs(err_grid[newExtr.front()]) <
                    std::fabs(err_grid[newExtr.back()]))
                    newExtr.erase(newExtr.begin());
                else
                    newExtr.pop_back();
            }
        }
        if (newExtr.size() < nExtr) {
            // Too few alternating peaks — keep old extr, retry next iter.
            continue;
        }

        // Convergence: max|E| should equal |δ|.
        double maxE = 0.0;
        for (std::size_t i = 0; i < grid.size(); ++i) {
            const double v = std::fabs(err_grid[i]);
            if (v > maxE) maxE = v;
        }
        const double dAbs    = std::fabs(delta);
        const bool   converged = (maxE - dAbs) < kConvTol * (dAbs > 0.0 ? dAbs : 1.0);

        for (std::size_t k = 0; k < nExtr; ++k) extr[k] = newExtr[k];

        if (converged) break;
    }

    // Final pass: recompute δ and C_k with the converged extremal set.
    for (std::size_t k = 0; k < nExtr; ++k)
        xk[k] = std::cos(grid[extr[k]].w);
    for (std::size_t k = 0; k < nExtr; ++k) {
        double prod = 1.0;
        for (std::size_t j = 0; j < nExtr; ++j)
            if (j != k) prod *= (xk[k] - xk[j]);
        gammak[k] = 1.0 / prod;
    }
    {
        double num = 0.0, den = 0.0;
        for (std::size_t k = 0; k < nExtr; ++k) {
            num += gammak[k] * grid[extr[k]].D;
            const double sg = (k % 2 == 0) ? 1.0 : -1.0;
            den += sg * gammak[k] / grid[extr[k]].W;
        }
        delta = num / den;
    }
    for (std::size_t k = 0; k < nExtr; ++k) {
        const double sg = (k % 2 == 0) ? 1.0 : -1.0;
        Ck[k] = grid[extr[k]].D - sg * delta / grid[extr[k]].W;
    }
    for (std::size_t k = 0; k < r; ++k) {
        xkP[k] = xk[k];
        CkP[k] = Ck[k];
    }
    for (std::size_t k = 0; k < r; ++k) {
        double prod = 1.0;
        for (std::size_t j = 0; j < r; ++j)
            if (j != k) prod *= (xkP[k] - xkP[j]);
        gkP[k] = 1.0 / prod;
    }

    // Sample H at the L+1 cosine nodes ωm = π·m/L for m=0..L.
    ScratchVec<double> Hs(L + 1, &arena);
    for (std::size_t m = 0; m <= L; ++m) {
        const double wm = M_PI * double(m) / double(L);
        const double xm = std::cos(wm);
        Hs[m] = bary_eval(xkP.data(), CkP.data(), gkP.data(), r, xm);
    }

    // Inverse cosine-series: a[k] from samples on Chebyshev-Lobatto
    // nodes {x_m = cos(π·m/L) : m=0..L}. Discrete orthogonality of
    // T_k requires endpoint scaling c_0 = c_L = 2 (halved coefficient):
    //   a[k] = (2/(L·c_k)) · [½H[0] + ½(-1)^k·H[L]
    //                         + Σ_{m=1..L-1} H[m]·cos(π·k·m/L)]
    ScratchVec<double> a(L + 1, &arena);
    for (std::size_t k = 0; k <= L; ++k) {
        double sum = 0.5 * Hs[0]
                   + 0.5 * Hs[L] * ((k % 2 == 0) ? 1.0 : -1.0);
        for (std::size_t m = 1; m < L; ++m)
            sum += Hs[m] * std::cos(M_PI * double(k) * double(m) / double(L));
        const double ck = (k == 0 || k == L) ? 2.0 : 1.0;
        a[k] = (2.0 / (double(L) * ck)) * sum;
    }

    Value bOut = Value::matrix(1, std::size_t(N) + 1, ValueType::DOUBLE, mr);
    double *bd = bOut.doubleDataMut();
    for (std::size_t i = 0; i <= static_cast<std::size_t>(N); ++i) bd[i] = 0.0;

    if (filterType == TYPE_I) {
        // Type I (even N): h[L] = a[0], h[L±k] = a[k]/2.
        bd[L] = a[0];
        for (std::size_t k = 1; k <= L; ++k) {
            const double v = 0.5 * a[k];
            bd[L - k] = v;
            bd[L + k] = v;
        }
    } else if (filterType == TYPE_II) {
        // Type II (odd N, length N+1 = 2L+2). Convert a[k] of P (= Σ a[k] cos kω)
        // into b[n] coefficients of H(ω) = Σ b[n] cos((n+½)ω) using
        //   cos(kω) · cos(ω/2) = ½ [cos((k+½)ω) + cos((k-½)ω)]
        //   b[0] = a[0] + a[1]/2,  b[n] = (a[n] + a[n+1])/2 for 1≤n≤L-1,
        //   b[L] = a[L]/2,  h[L - n] = b[n]/2,  h[L+1+n] = h[L-n].
        ScratchVec<double> b(L + 1, &arena);
        b[0] = a[0] + (L >= 1 ? 0.5 * a[1] : 0.0);
        for (std::size_t n = 1; n + 1 <= L; ++n)
            b[n] = 0.5 * (a[n] + a[n + 1]);
        if (L >= 1) b[L] = 0.5 * a[L];
        for (std::size_t n = 0; n <= L; ++n) {
            const double v = 0.5 * b[n];
            bd[L - n]        = v;
            bd[L + 1 + n]    = v;
        }
    } else if (filterType == TYPE_III) {
        // Type III (even N, anti-symmetric, length 2·Lh + 1, Lh = N/2 = L+1).
        // H(ω) = sin(ω) · Σ_{k=0..L} a[k] cos(kω) = Σ_{n=1..Lh} c[n] sin(nω)
        // From cos(kω)·sin(ω) = ½[sin((k+1)ω) − sin((k-1)ω)] (with sin(0)=0,
        // and k=0 contributing all of a[0] to sin(1ω)):
        //   c[1] = a[0] − a[2]/2
        //   c[n] = a[n-1]/2 − a[n+1]/2   for 2 ≤ n ≤ L-1
        //   c[L] = a[L-1]/2
        //   c[L+1] = a[L]/2
        // h[Lh - n] = c[n]/2 for n=1..Lh, h[Lh] = 0, h[Lh + n] = -h[Lh - n].
        const std::size_t Lh = L + 1;
        ScratchVec<double> c(Lh + 1, 0.0, &arena);
        if (L + 1 >= 1) {
            const double a2 = (L >= 2) ? a[2] : 0.0;
            c[1] = a[0] - 0.5 * a2;
        }
        for (std::size_t n = 2; n <= Lh; ++n) {
            const double am1 = (n - 1 <= L) ? a[n - 1] : 0.0;
            const double ap1 = (n + 1 <= L) ? a[n + 1] : 0.0;
            c[n] = 0.5 * am1 - 0.5 * ap1;
        }
        bd[Lh] = 0.0;
        // MATLAB anti-sym convention: h(low index) negative when the
        // ideal sin-series amplitude is positive (j·sign(ω) Hilbert
        // → A > 0 in passband → low-index h is negative for the
        // linear-phase decomposition e^(-jωN/2)·jA used by MATLAB).
        for (std::size_t n = 1; n <= Lh; ++n) {
            const double v = 0.5 * c[n];
            bd[Lh - n] = -v;
            bd[Lh + n] =  v;
        }
    } else /* TYPE_IV */ {
        // Type IV (odd N, anti-symmetric, length 2L+2, L = (N-1)/2).
        // H(ω) = sin(ω/2) · Σ a[k] cos(kω) = Σ_{n=1..L+1} d[n] sin((n-½)ω).
        // From sin(ω/2)·cos(kω) = ½[sin((k+½)ω) + sin((½-k)ω)] and
        // sin(-x) = -sin(x):
        //   k=0 → +a[0] to d[1] (single full contribution)
        //   k≥1 → +a[k]/2 to d[k+1] (sin((k+½)ω)),
        //         -a[k]/2 to d[k]   (sin((k-½)ω))
        // Closed form: d[1] = a[0] - a[1]/2,
        //              d[n] = (a[n-1] - a[n])/2  for 2 ≤ n ≤ L,
        //              d[L+1] = a[L]/2.
        // h[L - n + 1] = d[n]/2 for n=1..L+1, mirror: h[L+1+m] = -h[L-m].
        ScratchVec<double> dvec(L + 2, 0.0, &arena);
        if (L >= 1) dvec[1] = a[0] - 0.5 * a[1];
        else        dvec[1] = a[0];
        for (std::size_t n = 2; n <= L; ++n)
            dvec[n] = 0.5 * (a[n - 1] - a[n]);
        if (L >= 1) dvec[L + 1] = 0.5 * a[L];
        else        dvec[L + 1] = 0.0;
        // Same sign convention as Type III.
        for (std::size_t n = 1; n <= L + 1; ++n) {
            const double v = 0.5 * dvec[n];
            const std::size_t lo = L + 1 - n;   // h[L - n + 1] in 0-based
            bd[lo]            = -v;
            bd[(N) - lo]      =  v;  // mirror about (N+1)/2-1/2
        }
    }

    // Differentiator sign correction: flip the entire impulse response
    // when neg && !hilbert.
    if (isDiff) {
        for (std::size_t i = 0; i <= static_cast<std::size_t>(N); ++i)
            bd[i] = -bd[i];
    }

    return std::make_tuple(std::move(bOut), std::fabs(delta));
}


} // namespace numkit::signal
