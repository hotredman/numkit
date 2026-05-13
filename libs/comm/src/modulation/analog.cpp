// libs/comm/src/modulation/analog.cpp
//
// Analog modulators: pmmod (phase modulation).
//
// Forms a new TU since the analog mod/demod family expands over time
// (ammod, fmmod, ssbmod planned). Each is a closed-form expression
// in cos / sin of a phase term; demods can pull in filtfilt or
// Hilbert and live alongside.

#include <numkit/comm/modulation/analog.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/signal/transforms/hilbert.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::comm {

using Cd = std::complex<double>;

// ── pmmod ──────────────────────────────────────────────────────────
// Per MATLAB R2025b's pmmod.m:
//   t = (0:1/Fs:(N-1)/Fs)'
//   y = cos(2*pi*Fc*t + phasedev*x + ini_phase)
//
// Vector-orientation contract: 1-D row preserved as row in output;
// columns processed independently.
Value pmmod(const Value &x, double fc, double fs, double phasedev,
            double ini_phase, std::pmr::memory_resource *mr)
{
    if (!(fs > 0.0))
        throw Error("pmmod: Fs must be positive",
                    0, 0, "pmmod", "", "m:pmmod:Fs");
    if (!(fc > 0.0))
        throw Error("pmmod: Fc must be positive",
                    0, 0, "pmmod", "", "m:pmmod:Fc");
    if (fs < 2.0 * fc)
        throw Error("pmmod: Fs must be >= 2*Fc",
                    0, 0, "pmmod", "", "m:pmmod:FsLessThan2Fc");
    if (!(phasedev > 0.0))
        throw Error("pmmod: phasedev must be positive",
                    0, 0, "pmmod", "", "m:pmmod:InvalidPhaseDev");

    const auto &d = x.dims();
    size_t H = d.rows();
    size_t W = d.cols();
    const bool was_row = (H == 1 && W >= 1);
    if (was_row) {
        // Reorient row -> column for processing.
        std::swap(H, W);
    }

    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();

    const double twoPiFc = 2.0 * M_PI * fc;
    const double inv_fs  = 1.0 / fs;

    // Process column-major: for each column index c, walk rows.
    for (size_t c = 0; c < W; ++c) {
        for (size_t r = 0; r < H; ++r) {
            const double t  = static_cast<double>(r) * inv_fs;
            const double xi = was_row
                                  ? x.elemAsDouble(r)
                                  : x.elemAsDouble(c * H + r);
            const double phase = twoPiFc * t + phasedev * xi + ini_phase;
            o[c * H + r] = std::cos(phase);
        }
    }

    if (was_row) {
        // Output should be 1xN row to mirror input.
        Value row = Value::matrix(1, H, ValueType::DOUBLE, mr);
        std::copy(o, o + H, row.doubleDataMut());
        return row;
    }
    return out;
}

// ── ammod ──────────────────────────────────────────────────────────
// Per MATLAB R2025b's ammod.m:
//   t = (0:1/Fs:(N-1)/Fs)'
//   y = (x + carr_amp) .* cos(2*pi*Fc*t + ini_phase)
//
// carr_amp == 0 -> DSB-SC (suppressed carrier)
// carr_amp != 0 -> DSB-TC (transmitted carrier)
//
// Vector-orientation contract identical to pmmod.
Value ammod(const Value &x, double fc, double fs, double ini_phase,
            double carr_amp, std::pmr::memory_resource *mr)
{
    if (!(fs > 0.0))
        throw Error("ammod: Fs must be positive",
                    0, 0, "ammod", "", "m:ammod:Fs");
    if (!(fc > 0.0))
        throw Error("ammod: Fc must be positive",
                    0, 0, "ammod", "", "m:ammod:Fc");
    if (fs < 2.0 * fc)
        throw Error("ammod: Fs must be >= 2*Fc",
                    0, 0, "ammod", "", "m:ammod:FsLessThan2Fc");

    const auto &d = x.dims();
    size_t H = d.rows();
    size_t W = d.cols();
    const bool was_row = (H == 1 && W >= 1);
    if (was_row) {
        std::swap(H, W);
    }

    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();

    const double twoPiFc = 2.0 * M_PI * fc;
    const double inv_fs  = 1.0 / fs;

    for (size_t c = 0; c < W; ++c) {
        for (size_t r = 0; r < H; ++r) {
            const double t  = static_cast<double>(r) * inv_fs;
            const double xi = was_row
                                  ? x.elemAsDouble(r)
                                  : x.elemAsDouble(c * H + r);
            o[c * H + r] = (xi + carr_amp)
                         * std::cos(twoPiFc * t + ini_phase);
        }
    }

    if (was_row) {
        Value row = Value::matrix(1, H, ValueType::DOUBLE, mr);
        std::copy(o, o + H, row.doubleDataMut());
        return row;
    }
    return out;
}

// ── fmmod ──────────────────────────────────────────────────────────
// Per MATLAB R2025b's fmmod.m:
//   t      = (0:1/Fs:(N-1)/Fs)'
//   int_x  = cumsum(x) / Fs       (column-wise cumulative sum)
//   y      = cos(2*pi*Fc*t + 2*pi*freqdev*int_x + ini_phase)
//
// Vector-orientation contract identical to pmmod / ammod.
Value fmmod(const Value &x, double fc, double fs, double freqdev,
            double ini_phase, std::pmr::memory_resource *mr)
{
    if (!(fs > 0.0))
        throw Error("fmmod: Fs must be positive",
                    0, 0, "fmmod", "", "m:fmmod:Fs");
    if (fc < 0.0)
        throw Error("fmmod: Fc must be non-negative",
                    0, 0, "fmmod", "", "m:fmmod:Fc");
    if (fs < 2.0 * fc)
        throw Error("fmmod: Fs must be >= 2*Fc",
                    0, 0, "fmmod", "", "m:fmmod:FsLessThan2Fc");
    if (!(freqdev > 0.0))
        throw Error("fmmod: freqdev must be positive",
                    0, 0, "fmmod", "", "m:fmmod:InvalidFreqdev");

    const auto &d = x.dims();
    size_t H = d.rows();
    size_t W = d.cols();
    const bool was_row = (H == 1 && W >= 1);
    if (was_row) {
        std::swap(H, W);
    }

    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();

    const double twoPiFc      = 2.0 * M_PI * fc;
    const double twoPiFreqDev = 2.0 * M_PI * freqdev;
    const double inv_fs       = 1.0 / fs;

    // Per column: walk samples, accumulating int_x = cumsum(x)/Fs.
    for (size_t c = 0; c < W; ++c) {
        double acc = 0.0;
        for (size_t r = 0; r < H; ++r) {
            const double xi = was_row
                                  ? x.elemAsDouble(r)
                                  : x.elemAsDouble(c * H + r);
            acc += xi * inv_fs;            // running cumsum / Fs
            const double t = static_cast<double>(r) * inv_fs;
            o[c * H + r] = std::cos(twoPiFc * t
                                  + twoPiFreqDev * acc
                                  + ini_phase);
        }
    }

    if (was_row) {
        Value row = Value::matrix(1, H, ValueType::DOUBLE, mr);
        std::copy(o, o + H, row.doubleDataMut());
        return row;
    }
    return out;
}

// ── mskmod (differential variant) ──────────────────────────────────
// Per MATLAB R2025b's mskmod.m, differentially-encoded path:
//
//   xPm    = 2*x - 1                       (bits {0,1} -> {-1,+1})
//   xCum   = cumsum([0; xPm])              (length N+1)
//   coarseTime = (0:N)'
//   fineTime   = (0 : 1/nSamp : N - 1/nSamp)'   (length N*nSamp)
//   phaseVec   = pi/2 * interp1(coarseTime, xCum, fineTime)
//   y          = exp(1i * (phaseVec + ini_phase))
//
// Linear interpolation: between adjacent integer samples j and j+1,
// xCum changes monotonically, and the cumulative phase ramps linearly
// so the exponential traces a half-arc of the unit circle per symbol.
//
// KNOWN GAP: non-differential MSK path is deferred -- it requires
// rectpulse + circshift on the I/Q rails (we have rectpulse but the
// arrangement is more involved; will get its own cycle).
Value mskmod(const Value &x, int nSamp, double ini_phase,
             std::pmr::memory_resource *mr)
{
    if (nSamp <= 0)
        throw Error("mskmod: nSamp must be a positive integer",
                    0, 0, "mskmod", "", "m:mskmod:nSamp");

    const auto &d = x.dims();
    size_t H = d.rows();
    size_t W = d.cols();
    const bool was_row = (H == 1 && W >= 1);
    if (was_row) std::swap(H, W);

    // Build xCum per column.
    const size_t Nout = H * static_cast<size_t>(nSamp);
    Value out = Value::matrix(Nout, W, ValueType::COMPLEX, mr);
    Cd *o = out.complexDataMut();

    std::vector<double> xCum(H + 1);
    for (size_t c = 0; c < W; ++c) {
        // Compute xCum: xCum[0] = 0; xCum[k+1] = xCum[k] + (2*x[k] - 1)
        xCum[0] = 0.0;
        for (size_t r = 0; r < H; ++r) {
            const double xi = was_row
                                  ? x.elemAsDouble(r)
                                  : x.elemAsDouble(c * H + r);
            // Validate {0, 1}.
            if (xi != 0.0 && xi != 1.0)
                throw Error("mskmod: input must be binary (0 or 1)",
                            0, 0, "mskmod", "", "m:mskmod:NotBinary");
            xCum[r + 1] = xCum[r] + (2.0 * xi - 1.0);
        }

        const double inv_nSamp = 1.0 / static_cast<double>(nSamp);
        for (size_t j = 0; j < Nout; ++j) {
            const double tFine = static_cast<double>(j) * inv_nSamp;
            const size_t base  = static_cast<size_t>(std::floor(tFine));
            const double frac  = tFine - static_cast<double>(base);
            // Guard tail: at tFine = N - 1/nSamp the base = N-1, so
            // base+1 = N is in range.
            const double interp = xCum[base]
                                + frac * (xCum[base + 1] - xCum[base]);
            const double phase  = M_PI * 0.5 * interp + ini_phase;
            o[c * Nout + j] = Cd(std::cos(phase), std::sin(phase));
        }
    }

    if (was_row) {
        // Output matches MATLAB: 1xN row -> 1x(N*nSamp) row.
        Value row = Value::matrix(1, Nout, ValueType::COMPLEX, mr);
        std::copy(o, o + Nout, row.complexDataMut());
        return row;
    }
    return out;
}

// ── ssbmod ─────────────────────────────────────────────────────────
// Per MATLAB R2025b's ssbmod.m:
//   t = (0:1/Fs:(N-1)/Fs)'
//   Lower sideband (default):
//     y = x.*cos(2π·Fc·t + ini_phase)
//       + imag(hilbert(x)).*sin(2π·Fc·t + ini_phase)
//   Upper sideband ('upper'):
//     y = x.*cos(2π·Fc·t + ini_phase)
//       - imag(hilbert(x)).*sin(2π·Fc·t + ini_phase)
//
// Vector-orientation contract identical to pmmod / ammod / fmmod.
Value ssbmod(const Value &x, double fc, double fs, double ini_phase,
             bool upper, std::pmr::memory_resource *mr)
{
    if (!(fs > 0.0))
        throw Error("ssbmod: Fs must be positive",
                    0, 0, "ssbmod", "", "m:ssbmod:Fs");
    if (!(fc > 0.0))
        throw Error("ssbmod: Fc must be positive",
                    0, 0, "ssbmod", "", "m:ssbmod:Fc");
    if (fs <= 2.0 * fc)
        throw Error("ssbmod: Fs must be > 2*Fc",
                    0, 0, "ssbmod", "", "m:ssbmod:Fs2Fc");

    const auto &d = x.dims();
    size_t H = d.rows();
    size_t W = d.cols();
    const bool was_row = (H == 1 && W >= 1);
    if (was_row) {
        std::swap(H, W);
    }

    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();

    const double twoPiFc = 2.0 * M_PI * fc;
    const double inv_fs  = 1.0 / fs;
    const double sign    = upper ? -1.0 : +1.0;

    // hilbert is column-wise per MATLAB. Extract each column into a
    // 1-D Value and call signal::hilbert on it.
    for (size_t c = 0; c < W; ++c) {
        // Build column vector for this c
        Value col_in = Value::matrix(H, 1, ValueType::DOUBLE, mr);
        double *cp = col_in.doubleDataMut();
        for (size_t r = 0; r < H; ++r) {
            cp[r] = was_row ? x.elemAsDouble(r)
                            : x.elemAsDouble(c * H + r);
        }
        Value analytic = numkit::signal::hilbert(col_in, mr);
        // analytic.complexData()[i].imag() is imag(hilbert(col_in))
        const auto *cdat = analytic.complexData();
        for (size_t r = 0; r < H; ++r) {
            const double t  = static_cast<double>(r) * inv_fs;
            const double xi = cp[r];
            const double ang = twoPiFc * t + ini_phase;
            o[c * H + r] = xi * std::cos(ang)
                         + sign * cdat[r].imag() * std::sin(ang);
        }
    }

    if (was_row) {
        Value row = Value::matrix(1, H, ValueType::DOUBLE, mr);
        std::copy(o, o + H, row.doubleDataMut());
        return row;
    }
    return out;
}

namespace detail {

void pmmod_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("pmmod: requires (x, Fc, Fs, phasedev [, ini_phase])",
                    0, 0, "pmmod", "", "m:pmmod:nargin");
    const double fc = args[1].toScalar();
    const double fs = args[2].toScalar();
    const double phasedev = args[3].toScalar();
    double ini_phase = 0.0;
    if (args.size() >= 5 && !args[4].isEmpty()) {
        ini_phase = args[4].toScalar();
    }
    outs[0] = pmmod(args[0], fc, fs, phasedev, ini_phase,
                    ctx.engine->resource());
}

void ammod_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ammod: requires (x, Fc, Fs [, ini_phase [, carr_amp]])",
                    0, 0, "ammod", "", "m:ammod:nargin");
    const double fc = args[1].toScalar();
    const double fs = args[2].toScalar();
    double ini_phase = 0.0;
    double carr_amp  = 0.0;
    if (args.size() >= 4 && !args[3].isEmpty())
        ini_phase = args[3].toScalar();
    if (args.size() >= 5 && !args[4].isEmpty())
        carr_amp = args[4].toScalar();
    outs[0] = ammod(args[0], fc, fs, ini_phase, carr_amp,
                    ctx.engine->resource());
}

void fmmod_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("fmmod: requires (x, Fc, Fs, freqdev [, ini_phase])",
                    0, 0, "fmmod", "", "m:fmmod:nargin");
    const double fc = args[1].toScalar();
    const double fs = args[2].toScalar();
    const double freqdev = args[3].toScalar();
    double ini_phase = 0.0;
    if (args.size() >= 5 && !args[4].isEmpty())
        ini_phase = args[4].toScalar();
    outs[0] = fmmod(args[0], fc, fs, freqdev, ini_phase,
                    ctx.engine->resource());
}

void mskmod_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mskmod: requires (x, nSamp [, ini_phase])",
                    0, 0, "mskmod", "", "m:mskmod:nargin");
    const int nSamp = static_cast<int>(args[1].toScalar());
    double ini_phase = 0.0;
    if (args.size() >= 3 && !args[2].isEmpty())
        ini_phase = args[2].toScalar();
    outs[0] = mskmod(args[0], nSamp, ini_phase, ctx.engine->resource());
}

void ssbmod_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("ssbmod: requires (x, Fc, Fs [, ini_phase [, 'upper']])",
                    0, 0, "ssbmod", "", "m:ssbmod:nargin");
    const double fc = args[1].toScalar();
    const double fs = args[2].toScalar();
    double ini_phase = 0.0;
    if (args.size() >= 4 && !args[3].isEmpty())
        ini_phase = args[3].toScalar();
    bool upper = false;
    if (args.size() >= 5 && !args[4].isEmpty()) {
        if (!args[4].isChar() && !args[4].isString())
            throw Error("ssbmod: method must be a string ('upper')",
                        0, 0, "ssbmod", "", "m:ssbmod:InvStr");
        std::string m = args[4].toString();
        // Match MATLAB behaviour: any string containing 'up' selects USB.
        for (auto &c : m) c = static_cast<char>(std::tolower(c));
        upper = (m.find("up") != std::string::npos);
        if (!upper && !m.empty())
            throw Error("ssbmod: method must be 'upper'",
                        0, 0, "ssbmod", "", "m:ssbmod:InvStr");
    }
    outs[0] = ssbmod(args[0], fc, fs, ini_phase, upper,
                     ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::comm
