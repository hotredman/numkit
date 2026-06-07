// libs/signal/src/measurements/vibration_reg.cpp
//
// CallContext register half of measurements/vibration.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/measurements/vibration.hpp>
#include <numkit/signal/transforms/fft.hpp>
#include <numkit/signal/transforms/hilbert.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

void envspectrum_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("envspectrum: requires 1 argument",
                     0, 0, "envspectrum", "", "numkit:envspectrum:nargin");
    double fs = 0.0;
    if (args.size() >= 2 && !args[1].isEmpty()) fs = args[1].toScalar();
    auto [Es, F] = envspectrum(args[0], fs, ctx.engine->resource());
    outs[0] = std::move(Es);
    if (nargout > 1) outs[1] = std::move(F);
}

void tachorpm_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tachorpm: requires (x, fs[, threshold[, ppr]])",
                     0, 0, "tachorpm", "", "numkit:tachorpm:nargin");
    const double fs = args[1].toScalar();
    const Value &thr = (args.size() >= 3) ? args[2] : Value::Empty;
    int ppr = 1;
    if (args.size() >= 4 && !args[3].isEmpty()) ppr = static_cast<int>(args[3].toScalar());
    auto [rpm, t] = tachorpm(args[0], fs, thr, ppr, ctx.engine->resource());
    outs[0] = std::move(rpm);
    if (nargout > 1) outs[1] = std::move(t);
}

void rainflow_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rainflow: requires 1 argument",
                     0, 0, "rainflow", "", "numkit:rainflow:nargin");
    outs[0] = rainflow(args[0], ctx.engine->resource());
}

// MATLAB tsa convention: tsa(x, fs, tPulse) where tPulse is a vector
// of pulse arrival times (seconds) defining revolutions. Output is
// the average waveform over those revolutions.
//
// numkit historic convention: tsa(x, fs, rpm, fs_rpm[, n_per_rev])
// where rpm is a continuously-sampled rotation-rate signal.
//
// Dispatcher: 3-arg call -> MATLAB form; 4+ arg call -> numkit form.
namespace {

std::tuple<Value, Value>
tsa_matlab_form(std::pmr::memory_resource *mr, const Value &x, double fs,
                const Value &tPulse)
{
    const size_t nx = x.numel();
    const size_t np = tPulse.numel();
    if (nx == 0 || np < 2) {
        return std::make_tuple(
            Value::matrix(0, 1, ValueType::DOUBLE, mr),
            Value::matrix(0, 1, ValueType::DOUBLE, mr));
    }
    const double *xd = x.doubleData();
    std::vector<double> tv(np);
    for (size_t i = 0; i < np; ++i) tv[i] = tPulse.elemAsDouble(i);

    // Decide output length L = nominal samples per revolution. Use the
    // floor of the average inter-pulse interval times fs, matching
    // MATLAB's default behaviour.
    const double T_total = tv[np - 1] - tv[0];
    const size_t nRev = np - 1;
    const double T_avg = T_total / static_cast<double>(nRev);
    size_t L = static_cast<size_t>(std::floor(T_avg * fs));
    if (L < 1) L = 1;

    // For each revolution, linear-interpolate x onto an L-point grid
    // that spans [tv[i], tv[i+1]) and accumulate.
    std::vector<double> avg(L, 0.0);
    size_t nValidRevs = 0;
    for (size_t r = 0; r < nRev; ++r) {
        const double t0 = tv[r];
        const double t1 = tv[r + 1];
        const double dur = t1 - t0;
        if (dur <= 0.0) continue;
        bool revOK = true;
        std::vector<double> sample(L, 0.0);
        for (size_t k = 0; k < L; ++k) {
            const double tk = t0 + dur * static_cast<double>(k)
                                    / static_cast<double>(L);
            const double idxF = tk * fs;
            const long  i0   = static_cast<long>(std::floor(idxF));
            const double frac = idxF - static_cast<double>(i0);
            if (i0 < 0 || static_cast<size_t>(i0 + 1) >= nx) { revOK = false; break; }
            sample[k] = (1.0 - frac) * xd[i0] + frac * xd[i0 + 1];
        }
        if (!revOK) continue;
        for (size_t k = 0; k < L; ++k) avg[k] += sample[k];
        ++nValidRevs;
    }
    if (nValidRevs > 0) {
        for (size_t k = 0; k < L; ++k)
            avg[k] /= static_cast<double>(nValidRevs);
    }

    auto Ya = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    auto Ta = Value::matrix(L, 1, ValueType::DOUBLE, mr);
    double *ya = Ya.doubleDataMut();
    double *ta = Ta.doubleDataMut();
    for (size_t k = 0; k < L; ++k) {
        ya[k] = avg[k];
        ta[k] = T_avg * static_cast<double>(k) / static_cast<double>(L);
    }
    return std::make_tuple(std::move(Ya), std::move(Ta));
}

} // namespace

void tsa_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("tsa: requires (x, fs, tPulse) or (x, fs, rpm, fs_rpm[, n_per_rev])",
                     0, 0, "tsa", "", "numkit:tsa:nargin");
    const double fs = args[1].toScalar();
    if (args.size() == 3) {
        // MATLAB form: tsa(x, fs, tPulse)
        auto [avg, th] = tsa_matlab_form(ctx.engine->resource(),
                                         args[0], fs, args[2]);
        outs[0] = std::move(avg);
        if (nargout > 1) outs[1] = std::move(th);
        return;
    }
    // 4+ args -> legacy numkit form (continuous rpm signal).
    const double fs_rpm = args[3].toScalar();
    int npr = 1024;
    if (args.size() >= 5 && !args[4].isEmpty()) npr = static_cast<int>(args[4].toScalar());
    auto [avg, th] = tsa(args[0], fs, args[2], fs_rpm, npr, ctx.engine->resource());
    outs[0] = std::move(avg);
    if (nargout > 1) outs[1] = std::move(th);
}

} // namespace detail

} // namespace numkit::signal
