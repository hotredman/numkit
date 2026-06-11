// toolboxes/signal/src/waveform_generation/waveform_generation.cpp
//
// rectpuls, tripuls, gauspuls, pulstran (+ pulstranHandle), chirp.
// Split from library.cpp (transform-domain helpers nextpow2 /
// fftshift / ifftshift moved to transforms/transform_helpers.cpp).

#include <numkit/signal/waveform_generation/waveform_generation.hpp>
#include <numkit/signal/filter_design/filter_design.hpp>     // butter
#include <numkit/signal/digital_filtering/filter.hpp>        // filtfilt
#include <numkit/signal/transforms/hilbert.hpp>              // hilbert

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>  // createLike

#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

namespace numkit::signal {

namespace {

double gauspulsAlpha(double fc, double bw)
{
    // MATLAB: gauspuls envelope is exp(-α·t²) where α is chosen so that
    // the spectrum hits -6 dB (bwr) at the bandwidth edge. With bwr = -6 dB
    // hard-coded, α = -(π·bw·fc)² / (4·log(0.5)).
    constexpr double kPi = 3.14159265358979323846;
    const double pifb = kPi * fc * bw;
    return -(pifb * pifb) / (4.0 * std::log(0.5));
}

double rectpulsScalar(double tv, double w)
{
    const double half = 0.5 * w;
    const double a = std::abs(tv);
    if (a < half) return 1.0;
    return 0.0;  // boundary and outside
}

double tripulsScalar(double tv, double w)
{
    const double half = 0.5 * w;
    const double a = std::abs(tv);
    if (a >= half) return 0.0;
    return 1.0 - a / half;
}

double gauspulsScalar(double tv, double fc, double alpha)
{
    constexpr double kTwoPi = 6.28318530717958647692;
    return std::exp(-alpha * tv * tv) * std::cos(kTwoPi * fc * tv);
}

} // namespace

Value rectpuls(const Value &t, double w, std::pmr::memory_resource *mr)
{
    if (w <= 0)
        throw Error("rectpuls: width w must be positive",
                     0, 0, "rectpuls", "", "numkit:rectpuls:badWidth");
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = rectpulsScalar(t.elemAsDouble(i), w);
    return out;
}

Value tripuls(const Value &t, double w, std::pmr::memory_resource *mr)
{
    if (w <= 0)
        throw Error("tripuls: width w must be positive",
                     0, 0, "tripuls", "", "numkit:tripuls:badWidth");
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = tripulsScalar(t.elemAsDouble(i), w);
    return out;
}

Value gauspuls(const Value &t, double fc, double bw, std::pmr::memory_resource *mr)
{
    if (fc <= 0 || bw <= 0)
        throw Error("gauspuls: fc and bw must be positive",
                     0, 0, "gauspuls", "", "numkit:gauspuls:badArg");
    const double alpha = gauspulsAlpha(fc, bw);
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i)
        dst[i] = gauspulsScalar(t.elemAsDouble(i), fc, alpha);
    return out;
}

Value pulstranHandle(Span<const double> t, Span<const double> d,
                     FnHandle fn, std::pmr::memory_resource *mr)
{
    const size_t n  = t.size();
    const size_t nd = d.size();

    Value out = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    if (n) std::memset(dst, 0, n * sizeof(double));

    // Per-iteration shifted-`t` buffer (row vector, matching MATLAB
    // convention for the callback input).
    Value shifted = Value::matrix(1, n, ValueType::DOUBLE, mr);
    double *sh = shifted.doubleDataMut();
    for (size_t k = 0; k < nd; ++k) {
        const double dk = d[k];
        for (size_t i = 0; i < n; ++i)
            sh[i] = t[i] - dk;
        Value r;
        Value args[1] = { shifted };
        Span<const Value> ar(args, 1);
        Span<Value>       ou(&r, 1);
        fn(ar, ou, mr);
        if (r.numel() != n)
            throw Error("pulstran: handle must return a vector of the same "
                         "length as t",
                         0, 0, "pulstran", "", "numkit:pulstran:badHandleOutput");
        for (size_t i = 0; i < n; ++i)
            dst[i] += r.elemAsDouble(i);
    }
    return out;
}

Value pulstran(const Value &t, const Value &d, const std::string &fnName,
               double fcOrW, double bw, std::pmr::memory_resource *mr)
{
    auto out = createLike(t, ValueType::DOUBLE, mr);
    const size_t n = t.numel();
    std::memset(out.doubleDataMut(), 0, n * sizeof(double));
    double *dst = out.doubleDataMut();

    std::string lower;
    lower.reserve(fnName.size());
    for (char c : fnName)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    auto applyKernel = [&](double (*kernel)(double, double, double),
                           double a, double b) {
        const size_t nd = d.numel();
        for (size_t k = 0; k < nd; ++k) {
            const double dk = d.elemAsDouble(k);
            for (size_t i = 0; i < n; ++i)
                dst[i] += kernel(t.elemAsDouble(i) - dk, a, b);
        }
    };

    if (lower == "rectpuls") {
        applyKernel([](double tv, double w, double) { return rectpulsScalar(tv, w); },
                    fcOrW, 0.0);
    } else if (lower == "tripuls") {
        applyKernel([](double tv, double w, double) { return tripulsScalar(tv, w); },
                    fcOrW, 0.0);
    } else if (lower == "gauspuls") {
        const double alpha = gauspulsAlpha(fcOrW, bw);
        applyKernel([](double tv, double fc, double a) { return gauspulsScalar(tv, fc, a); },
                    fcOrW, alpha);
    } else {
        throw Error("pulstran: unsupported pulse function '" + fnName
                     + "' (built-ins: 'rectpuls'/'tripuls'/'gauspuls'). "
                     + "Custom handles need the engine callback API (planned).",
                     0, 0, "pulstran", "", "numkit:pulstran:fnUnsupported");
    }
    return out;
}

Value square(const Value &t, double duty, std::pmr::memory_resource *mr)
{
    if (duty < 0.0 || duty > 100.0)
        throw Error("square: duty cycle must be in [0, 100]",
                     0, 0, "square", "", "numkit:square:badDuty");
    constexpr double kTwoPi = 6.28318530717958647692;
    const double threshold = duty / 100.0;
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i) {
        const double tv = t.elemAsDouble(i);
        // Wrap to [0, 1) within one period.
        double frac = tv / kTwoPi;
        frac -= std::floor(frac);
        dst[i] = (frac < threshold) ? 1.0 : -1.0;
    }
    return out;
}

Value sawtooth(const Value &t, double width, std::pmr::memory_resource *mr)
{
    if (width < 0.0 || width > 1.0)
        throw Error("sawtooth: width must be in [0, 1]",
                     0, 0, "sawtooth", "", "numkit:sawtooth:badWidth");
    constexpr double kTwoPi = 6.28318530717958647692;
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i) {
        double frac = t.elemAsDouble(i) / kTwoPi;
        frac -= std::floor(frac);   // [0, 1)
        // Rising portion: [0, width) → linear from -1 to +1.
        // Falling portion: [width, 1) → linear from +1 down to -1.
        if (width == 0.0) {
            dst[i] = -2.0 * frac + 1.0;
        } else if (width == 1.0) {
            dst[i] = 2.0 * frac - 1.0;
        } else if (frac < width) {
            dst[i] = 2.0 * frac / width - 1.0;
        } else {
            dst[i] = 2.0 * (1.0 - frac) / (1.0 - width) - 1.0;
        }
    }
    return out;
}

Value sinc(const Value &t, std::pmr::memory_resource *mr)
{
    constexpr double kPi = 3.14159265358979323846;
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i) {
        const double tv = t.elemAsDouble(i);
        if (tv == 0.0) {
            dst[i] = 1.0;
        } else {
            const double pt = kPi * tv;
            dst[i] = std::sin(pt) / pt;
        }
    }
    return out;
}

Value gmonopuls(const Value &t, double fc, std::pmr::memory_resource *mr)
{
    if (fc <= 0)
        throw Error("gmonopuls: fc must be positive",
                     0, 0, "gmonopuls", "", "numkit:gmonopuls:badFc");
    // Standard normalised gmonopuls: y(t) = 2·√e·π·fc·t · exp(-2·(π·fc·t)²)
    // peaks at +1 when t = 1/(2π·fc).
    constexpr double kPi = 3.14159265358979323846;
    const double sqrtE = std::sqrt(std::exp(1.0));
    const double k = 2.0 * sqrtE * kPi * fc;
    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t n = t.numel();
    for (size_t i = 0; i < n; ++i) {
        const double tv = t.elemAsDouble(i);
        const double pft = kPi * fc * tv;
        dst[i] = k * tv * std::exp(-2.0 * pft * pft);
    }
    return out;
}

Value diric(const Value &x, int n, std::pmr::memory_resource *mr)
{
    if (n < 1)
        throw Error("diric: n must be a positive integer",
                     0, 0, "diric", "", "numkit:diric:badN");
    constexpr double kTwoPi = 6.28318530717958647692;
    auto out = createLike(x, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t N = x.numel();
    for (size_t i = 0; i < N; ++i) {
        const double xv = x.elemAsDouble(i);
        // Detect xv ≈ 2π·k by checking sin(x/2) ≈ 0.
        const double s = std::sin(xv * 0.5);
        if (std::abs(s) < 1e-12) {
            const long k = std::lround(xv / kTwoPi);
            // Sign: (-1)^(k·(n-1))
            const long sign = ((k * (n - 1)) & 1) ? -1 : 1;
            dst[i] = static_cast<double>(sign);
        } else {
            dst[i] = std::sin(n * xv * 0.5) / (n * s);
        }
    }
    return out;
}

Value chirp(const Value &t, double f0, double t1, double f1,
            const std::string &method, std::pmr::memory_resource *mr)
{
    if (t1 <= 0)
        throw Error("chirp: t1 must be positive",
                     0, 0, "chirp", "", "numkit:chirp:badT1");

    std::string m = method;
    for (auto &c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    enum class Mode { Linear, Quadratic, Logarithmic };
    Mode mode;
    if      (m == "linear" || m.empty())   mode = Mode::Linear;
    else if (m == "quadratic")             mode = Mode::Quadratic;
    else if (m == "logarithmic")           mode = Mode::Logarithmic;
    else
        throw Error("chirp: method must be 'linear', 'quadratic', or 'logarithmic'",
                     0, 0, "chirp", "", "numkit:chirp:badMethod");

    if (mode == Mode::Logarithmic) {
        if (f0 <= 0 || f1 <= 0)
            throw Error("chirp: 'logarithmic' requires f0 > 0 and f1 > 0",
                         0, 0, "chirp", "", "numkit:chirp:badFreq");
        if (f0 == f1)
            throw Error("chirp: 'logarithmic' requires f0 != f1",
                         0, 0, "chirp", "", "numkit:chirp:badFreq");
    }

    auto out = createLike(t, ValueType::DOUBLE, mr);
    double *dst = out.doubleDataMut();
    const size_t N = t.numel();

    constexpr double kTwoPi = 6.28318530717958647692;

    auto readT = [&](size_t i) -> double { return t.elemAsDouble(i); };

    switch (mode) {
    case Mode::Linear: {
        const double k = (f1 - f0) / t1;
        for (size_t i = 0; i < N; ++i) {
            const double tv = readT(i);
            const double phase = kTwoPi * (f0 * tv + 0.5 * k * tv * tv);
            dst[i] = std::cos(phase);
        }
        break;
    }
    case Mode::Quadratic: {
        const double k = (f1 - f0) / (t1 * t1);
        for (size_t i = 0; i < N; ++i) {
            const double tv = readT(i);
            const double phase = kTwoPi * (f0 * tv + (k / 3.0) * tv * tv * tv);
            dst[i] = std::cos(phase);
        }
        break;
    }
    case Mode::Logarithmic: {
        const double beta = std::pow(f1 / f0, 1.0 / t1);
        const double logBeta = std::log(beta);
        for (size_t i = 0; i < N; ++i) {
            const double tv = readT(i);
            const double phase = kTwoPi * f0 * (std::pow(beta, tv) - 1.0) / logBeta;
            dst[i] = std::cos(phase);
        }
        break;
    }
    }
    return out;
}

// ── demod (Phase 4.13) ──────────────────────────────────────────────
//
// Analog demodulation. Supports am / amdsb-sc (alias) / amdsb-tc.
// Pipeline matches MATLAB R2025b demod.m:
//   x = y .* cos(2π Fc t)
//   [b, a] = butter(5, Fc*2/Fs)   (5th-order Butterworth lowpass)
//   x = filtfilt(b, a, x) per column
//   for amdsb-tc: x -= opt (DC offset, default 0)
//
// KNOWN GAPs: fm/pm modes (use hilbert which depends on toolboxes/signal::fft
// sign-convention bug — same blocker as Cycle J / pitch LHS/SRH).
// amssb / pwm / ptm/ppm / qam similarly deferred.
Value demod(const Value &y, double Fc, double Fs,
            const std::string &method, const Value &opt,
            std::pmr::memory_resource *mr)
{
    constexpr double kPi = 3.14159265358979323846;
    if (Fs <= 0.0)
        throw Error("demod: Fs must be positive",
                    0, 0, "demod", "", "numkit:demod:BadFs");

    const std::size_t N = y.numel();
    if (N == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    std::string m = method;
    std::transform(m.begin(), m.end(), m.begin(),
                    [](unsigned char c) { return std::tolower(c); });

    const bool isAmFamily = (m == "am" || m == "amdsb-sc" || m == "amdsb-tc");
    const bool isFmPm = (m == "fm" || m == "pm");
    if (!isAmFamily && !isFmPm)
        throw Error("demod: only am/amdsb-sc/amdsb-tc/fm/pm supported "
                    "(amssb/pwm/ptm/ppm/qam deferred)",
                    0, 0, "demod", "", "numkit:demod:UnsupportedMethod");

    // Convert row vector to column for processing.
    const bool isRowVec = (y.dims().rows() == 1 && y.dims().cols() > 1);
    const std::size_t len = isRowVec ? y.dims().cols() : y.dims().rows();
    const std::size_t cols = isRowVec ? 1 : y.dims().cols();

    if (isFmPm) {
        // FM/PM share most: yq = hilbert(y) .* exp(-j·2π·Fc·t)
        // FM: x = (1/P1) · diff(unwrap(angle(yq))) prepended with 0
        // PM: x = (1/P1) · angle(yq)
        double P1 = 1.0;
        if (!opt.isEmpty()) P1 = opt.toScalar();

        // Compute hilbert on y.
        Value yVal = Value::matrix(len, cols, ValueType::DOUBLE, mr);
        double *yvd = yVal.doubleDataMut();
        for (std::size_t c = 0; c < cols; ++c) {
            for (std::size_t r = 0; r < len; ++r) {
                yvd[r + c * len] = isRowVec ? y.elemAsDouble(r)
                                              : y.elemAsDouble(r + c * len);
            }
        }
        Value h = numkit::signal::hilbert(yVal, mr);
        const Complex *hd = h.complexData();

        Value out = Value::matrix(len, cols, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();

        if (m == "fm") {
            // FM: angle(yq) and unwrap, then differentiate
            for (std::size_t c = 0; c < cols; ++c) {
                // yq[r] = hilbert(y)[r] · exp(-j·2π·Fc·t)
                // angle(yq) = angle(hilbert(y)) - 2π·Fc·t
                std::vector<double> ang(len);
                for (std::size_t r = 0; r < len; ++r) {
                    const Complex hh = hd[r + c * len];
                    const double t = static_cast<double>(r) / Fs;
                    const Complex yq = hh * std::polar(1.0, -2.0 * kPi * Fc * t);
                    ang[r] = std::arg(yq);
                }
                // Unwrap (jump > π implies 2π wrap)
                for (std::size_t r = 1; r < len; ++r) {
                    while (ang[r] - ang[r - 1] > kPi) ang[r] -= 2.0 * kPi;
                    while (ang[r] - ang[r - 1] < -kPi) ang[r] += 2.0 * kPi;
                }
                // diff prepended with 0
                od[0 + c * len] = 0.0;
                for (std::size_t r = 1; r < len; ++r) {
                    od[r + c * len] = (1.0 / P1) * (ang[r] - ang[r - 1]);
                }
            }
        } else {  // pm
            for (std::size_t c = 0; c < cols; ++c) {
                for (std::size_t r = 0; r < len; ++r) {
                    const Complex hh = hd[r + c * len];
                    const double t = static_cast<double>(r) / Fs;
                    const Complex yq = hh * std::polar(1.0, -2.0 * kPi * Fc * t);
                    od[r + c * len] = (1.0 / P1) * std::arg(yq);
                }
            }
        }
        if (isRowVec && cols == 1) {
            Value rowOut = Value::matrix(1, len, ValueType::DOUBLE, mr);
            std::copy(od, od + len, rowOut.doubleDataMut());
            return rowOut;
        }
        return out;
    }

    // Step 1: x = y .* cos(2π Fc t)
    Value mixed = Value::matrix(len, cols, ValueType::DOUBLE, mr);
    double *md = mixed.doubleDataMut();
    for (std::size_t c = 0; c < cols; ++c) {
        for (std::size_t r = 0; r < len; ++r) {
            const double t = static_cast<double>(r) / Fs;
            const double yi = isRowVec ? y.elemAsDouble(r)
                                        : y.elemAsDouble(r + c * len);
            md[r + c * len] = yi * std::cos(2.0 * kPi * Fc * t);
        }
    }

    // Step 2: 5th-order Butterworth lowpass at cutoff 2*Fc/Fs (normalized).
    // butter() needs Wn ∈ (0, 1). For high Fc relative to Fs (Wn ≥ 1), skip filter.
    const double Wn = Fc * 2.0 / Fs;
    Value out;
    if (Wn > 0.0 && Wn < 1.0) {
        auto [bp, ap] = numkit::signal::butter(5, Wn, "low", mr);
        // filtfilt per column: process the entire matrix (filtfilt handles cols).
        out = numkit::signal::filtfilt(bp, ap, mixed, mr);
    } else {
        out = mixed;
    }

    // Step 3: amdsb-tc subtracts opt offset.
    if (m == "amdsb-tc") {
        double offset = 0.0;
        if (!opt.isEmpty()) offset = opt.toScalar();
        if (offset != 0.0) {
            double *od = out.doubleDataMut();
            for (std::size_t i = 0; i < len * cols; ++i) od[i] -= offset;
        }
    }

    // Restore row-vector orientation if input was row.
    if (isRowVec && cols == 1) {
        Value rowOut = Value::matrix(1, len, ValueType::DOUBLE, mr);
        std::copy(out.doubleData(), out.doubleData() + len, rowOut.doubleDataMut());
        return rowOut;
    }
    return out;
}

// ── modulate (Phase 4.12) ───────────────────────────────────────────
//
// Analog modulation. Supports am/amdsb-sc (alias)/amdsb-tc/fm/pm.
// Matches MATLAB R2025b modulate.m for these 4 modes one-to-one.
// amssb (uses hilbert), pwm/ptm/ppm/qam deferred — KNOWN GAPs.
Value modulate(const Value &x, double Fc, double Fs,
               const std::string &method, const Value &opt,
               std::pmr::memory_resource *mr)
{
    constexpr double kPi = 3.14159265358979323846;
    if (Fs <= 0.0)
        throw Error("modulate: Fs must be positive",
                    0, 0, "modulate", "", "numkit:modulate:BadFs");

    const std::size_t N = x.numel();
    if (N == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    std::string m = method;
    std::transform(m.begin(), m.end(), m.begin(),
                    [](unsigned char c) { return std::tolower(c); });

    const bool isRowVec = (x.dims().rows() == 1 && x.dims().cols() > 1);
    const std::size_t len = isRowVec ? x.dims().cols() : x.dims().rows();
    const std::size_t cols = isRowVec ? 1 : x.dims().cols();
    Value out = Value::matrix(len, cols, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    auto getX = [&](std::size_t r, std::size_t c) -> double {
        if (isRowVec) return x.elemAsDouble(r);
        return x.elemAsDouble(r + c * len);
    };

    if (m == "am" || m == "amdsb-sc") {
        for (std::size_t c = 0; c < cols; ++c) {
            for (std::size_t r = 0; r < len; ++r) {
                const double t = static_cast<double>(r) / Fs;
                od[r + c * len] = getX(r, c) * std::cos(2.0 * kPi * Fc * t);
            }
        }
    } else if (m == "amssb") {
        // y = x .* cos(2π·Fc·t) + imag(hilbert(x)) .* sin(2π·Fc·t)
        // Compute hilbert of full signal first.
        Value xVal = Value::matrix(len, cols, ValueType::DOUBLE, mr);
        double *xd = xVal.doubleDataMut();
        for (std::size_t c = 0; c < cols; ++c)
            for (std::size_t r = 0; r < len; ++r)
                xd[r + c * len] = getX(r, c);
        Value h = numkit::signal::hilbert(xVal, mr);
        const Complex *hd = h.complexData();
        for (std::size_t c = 0; c < cols; ++c) {
            for (std::size_t r = 0; r < len; ++r) {
                const double t = static_cast<double>(r) / Fs;
                const double cosT = std::cos(2.0 * kPi * Fc * t);
                const double sinT = std::sin(2.0 * kPi * Fc * t);
                const double xv = xd[r + c * len];
                const double imagH = hd[r + c * len].imag();
                od[r + c * len] = xv * cosT + imagH * sinT;
            }
        }
    } else if (m == "amdsb-tc") {
        double offset;
        if (!opt.isEmpty()) {
            offset = opt.toScalar();
        } else {
            offset = x.elemAsDouble(0);
            for (std::size_t i = 1; i < N; ++i) {
                const double v = x.elemAsDouble(i);
                if (v < offset) offset = v;
            }
        }
        for (std::size_t c = 0; c < cols; ++c) {
            for (std::size_t r = 0; r < len; ++r) {
                const double t = static_cast<double>(r) / Fs;
                od[r + c * len] = (getX(r, c) - offset) * std::cos(2.0 * kPi * Fc * t);
            }
        }
    } else if (m == "fm") {
        double kf;
        if (!opt.isEmpty()) {
            kf = opt.toScalar();
        } else {
            double xMax = 0.0;
            for (std::size_t i = 0; i < N; ++i) {
                const double v = std::abs(x.elemAsDouble(i));
                if (v > xMax) xMax = v;
            }
            kf = (xMax > 0.0) ? (Fc / Fs) * 2.0 * kPi / xMax : 0.0;
        }
        for (std::size_t c = 0; c < cols; ++c) {
            double cum = 0.0;
            for (std::size_t r = 0; r < len; ++r) {
                cum += getX(r, c);
                const double t = static_cast<double>(r) / Fs;
                od[r + c * len] = std::cos(2.0 * kPi * Fc * t + kf * cum);
            }
        }
    } else if (m == "pm") {
        double kp;
        if (!opt.isEmpty()) {
            kp = opt.toScalar();
        } else {
            double xMax = 0.0;
            for (std::size_t i = 0; i < N; ++i) {
                const double v = std::abs(x.elemAsDouble(i));
                if (v > xMax) xMax = v;
            }
            kp = (xMax > 0.0) ? kPi / xMax : 0.0;
        }
        for (std::size_t c = 0; c < cols; ++c) {
            for (std::size_t r = 0; r < len; ++r) {
                const double t = static_cast<double>(r) / Fs;
                od[r + c * len] = std::cos(2.0 * kPi * Fc * t + kp * getX(r, c));
            }
        }
    } else {
        throw Error("modulate: method must be 'am'/'amdsb-sc'/'amdsb-tc'/'amssb'/'fm'/'pm'",
                    0, 0, "modulate", "", "numkit:modulate:UnsupportedMethod");
    }

    if (isRowVec && len > 0) {
        Value rowOut = Value::matrix(1, len, ValueType::DOUBLE, mr);
        std::copy(od, od + len, rowOut.doubleDataMut());
        return rowOut;
    }
    return out;
}

// ── vco (Phase 4.8) ─────────────────────────────────────────────────
//
// Voltage-controlled (frequency-modulated) oscillator. Matches MATLAB
// R2025b vco.m → modulate(...,'fm') one-to-one.
//
// Algorithm: per column, t = (0..N-1)/Fs,  cum = cumsum(x),
//            y = cos(2π·Fc·t + range1·cum)  (rectangular integral approx).
// Where range scalar => Fc=range, range1=(Fc/Fs)·2π;
//       range vector => Fc=mean(range), range1=(range[1]-Fc)/Fs·2π.
namespace {
// Shared body: `Fc` is the centre frequency, `range1` is the
// instantaneous-modulation factor (rad / sample / unit-x).
Value vcoImpl(const Value &x, double Fc, double range1, double fs,
              std::pmr::memory_resource *mr)
{
    constexpr double kPi = 3.14159265358979323846;
    if (fs <= 0.0)
        throw Error("vco: fs must be positive",
                    0, 0, "vco", "", "numkit:vco:BadFs");
    const size_t N = x.numel();
    // Range check: x ∈ [-1, 1].
    for (size_t i = 0; i < N; ++i) {
        const double v = x.elemAsDouble(i);
        if (v > 1.0 || v < -1.0)
            throw Error("vco: x values must be in [-1, 1]",
                        0, 0, "vco", "", "numkit:vco:InvalidRange");
    }

    // Allocate output (same shape as x).
    Value out;
    if (x.dims().is3D())
        out = Value::matrix3d(x.dims().rows(), x.dims().cols(),
                               x.dims().pages(), ValueType::DOUBLE, mr);
    else
        out = Value::matrix(x.dims().rows(), x.dims().cols(),
                             ValueType::DOUBLE, mr);
    if (N == 0) return out;

    double *od = out.doubleDataMut();

    // Determine "rows" axis (along which integration runs):
    // - If x is column vector (or Nx1): integrate along rows (single channel).
    // - If matrix: each column is a channel.
    const size_t R = (x.dims().rows() == 1 && x.dims().cols() > 1)
                      ? x.dims().cols()
                      : x.dims().rows();
    const size_t C = (x.dims().rows() == 1 && x.dims().cols() > 1)
                      ? 1
                      : x.dims().cols();
    const bool rowVec = (x.dims().rows() == 1 && x.dims().cols() > 1);

    for (size_t c = 0; c < C; ++c) {
        double cum = 0.0;
        for (size_t n = 0; n < R; ++n) {
            // Index into x: row vec → x[n]; column-major matrix → x[n + c*R].
            const size_t srcIdx = rowVec ? n : (n + c * R);
            cum += x.elemAsDouble(srcIdx);
            const double t = static_cast<double>(n) / fs;
            od[srcIdx] = std::cos(2.0 * kPi * Fc * t + range1 * cum);
        }
    }
    return out;
}
} // anon

Value vco(const Value &x, double fc, double fs,
          std::pmr::memory_resource *mr)
{
    // Centre-form: Fc = fc, range1 = (Fc / fs) * 2π.
    constexpr double kPi = 3.14159265358979323846;
    const double range1 = (fc / fs) * 2.0 * kPi;
    return vcoImpl(x, fc, range1, fs, mr);
}

Value vco(const Value &x, double fmin, double fmax, double fs,
          std::pmr::memory_resource *mr)
{
    // Range-form: Fc = mean(fmin, fmax), range1 = (fmax - Fc)/fs * 2π.
    constexpr double kPi = 3.14159265358979323846;
    const double fc     = 0.5 * (fmin + fmax);
    const double range1 = (fmax - fc) / fs * 2.0 * kPi;
    return vcoImpl(x, fc, range1, fs, mr);
}

} // namespace numkit::signal
