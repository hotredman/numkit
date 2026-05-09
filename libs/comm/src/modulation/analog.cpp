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

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::comm {

// ── pmmod ──────────────────────────────────────────────────────────
// Per MATLAB R2025b's pmmod.m:
//   t = (0:1/Fs:(N-1)/Fs)'
//   y = cos(2*pi*Fc*t + phasedev*x + ini_phase)
//
// Vector-orientation contract: 1-D row preserved as row in output;
// columns processed independently.
Value pmmod(std::pmr::memory_resource *mr, const Value &x,
            double fc, double fs, double phasedev, double ini_phase)
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
Value ammod(std::pmr::memory_resource *mr, const Value &x,
            double fc, double fs, double ini_phase, double carr_amp)
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
    outs[0] = pmmod(ctx.engine->resource(), args[0], fc, fs, phasedev,
                    ini_phase);
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
    outs[0] = ammod(ctx.engine->resource(), args[0], fc, fs, ini_phase,
                    carr_amp);
}

} // namespace detail

} // namespace numkit::comm
