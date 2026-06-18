// toolboxes/wavelet/src/denoise/denoise.cpp
//
// Wavelet thresholding primitives (wthresh / wnoisest) and the
// composite VisuShrink-style denoiser (wdenoise). All built on the
// existing wavedec / waverec / detcoef plumbing — no extra DSP
// machinery here, just thresholding and a robust σ estimate.

#include <numkit/wavelet/denoise/denoise.hpp>
#include <numkit/wavelet/dwt/multilevel.hpp>

// Compute-only TU: Value substrate + Error, no engine. The wthresh /
// wnoisest / wdenoise builtins (CallContext wrappers) live in
// denoise/denoise_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace numkit::wavelet {

namespace {

double median_abs(std::vector<double> v) {
    if (v.empty()) return 0.0;
    for (auto &x : v) x = std::abs(x);
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    return (n % 2 == 1)
               ? v[n / 2]
               : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

std::vector<double> vecFromValue(const Value &v) {
    std::vector<double> out(v.numel());
    for (size_t i = 0; i < v.numel(); ++i) out[i] = v.elemAsDouble(i);
    return out;
}

} // anonymous

Value wthresh(const Value &X, const std::string &sorh, double T, std::pmr::memory_resource *mr)
{
    const size_t N = X.numel();
    Value Y = Value::matrix(X.dims().rows(), X.dims().cols(),
                            ValueType::DOUBLE, mr);
    if (N == 0) return Y;

    double *yd = Y.doubleDataMut();
    const bool hard = (sorh == "h" || sorh == "H");
    const bool soft = (sorh == "s" || sorh == "S");
    if (!hard && !soft)
        throw Error("wthresh: sorh must be 'h' or 's'",
                    0, 0, "wthresh", "", "numkit:wthresh:sorh");

    for (size_t i = 0; i < N; ++i) {
        const double x = X.elemAsDouble(i);
        const double ax = std::abs(x);
        if (hard) {
            yd[i] = (ax > T) ? x : 0.0;
        } else {
            // soft: sign(x) * max(|x| - T, 0)
            const double mag = ax - T;
            yd[i] = (mag > 0.0) ? std::copysign(mag, x) : 0.0;
        }
    }
    return Y;
}

Value wnoisest(const Value &C, const Value &L, const Value &S, std::pmr::memory_resource *mr)
{
    const size_t k = S.numel();
    Value out = Value::matrix(1, k, ValueType::DOUBLE, mr);
    if (k == 0) return out;
    double *od = out.doubleDataMut();

    for (size_t i = 0; i < k; ++i) {
        const int level = static_cast<int>(S.elemAsDouble(i));
        Value cD = detcoef(C, L, level, mr);
        auto v = vecFromValue(cD);
        const double med = median_abs(std::move(v));
        od[i] = med / 0.6745;
    }
    return out;
}

Value wdenoise(const Value &x, int level, const std::string &wname, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    if (N < 2) {
        // Nothing to denoise — return a copy as a row.
        Value y = Value::matrix(x.dims().rows(), x.dims().cols(),
                                ValueType::DOUBLE, mr);
        double *yd = y.doubleDataMut();
        for (size_t i = 0; i < N; ++i) yd[i] = x.elemAsDouble(i);
        return y;
    }
    if (level <= 0) {
        // MATLAB default: a few levels but not so deep that the
        // approximation band collapses below the filter length. We
        // approximate with min(floor(log2(N)), 5).
        const int maxLog = static_cast<int>(std::floor(std::log2(double(N))));
        level = std::min(maxLog, 5);
        if (level < 1) level = 1;
    }
    const std::string w = wname.empty() ? std::string("sym4") : wname;

    // 1. Multi-level decomposition.
    auto [C, L] = wavedec(x, level, w, mr);

    // 2. Robust noise σ from the finest detail band (MATLAB default).
    {
        Value cD1 = detcoef(C, L, 1, mr);
        auto v = vecFromValue(cD1);
        const double sigma = median_abs(std::move(v)) / 0.6745;

        // 3. Universal (VisuShrink) threshold.
        const double T = sigma * std::sqrt(2.0 * std::log(double(N)));

        // 4. Soft-threshold every detail band in-place inside C.
        // Layout of C: [cA_level | cD_level | cD_{level-1} | ... | cD_1].
        const Value &Lref = L;   // plain reference: capturing a structured
                                 // binding directly in a lambda is C++20.
        auto sliceLen = [&](size_t idx) -> size_t {
            return static_cast<size_t>(Lref.elemAsDouble(idx));
        };
        size_t off = sliceLen(0); // skip cA
        double *Cd = C.doubleDataMut();
        for (int k = 0; k < level; ++k) {
            const size_t dLen = sliceLen(1 + k);
            for (size_t i = 0; i < dLen; ++i) {
                const double xv = Cd[off + i];
                const double ax = std::abs(xv);
                const double mag = ax - T;
                Cd[off + i] = (mag > 0.0) ? std::copysign(mag, xv) : 0.0;
            }
            off += dLen;
        }
    }

    // 5. Reconstruct.
    return waverec(C, L, w, mr);
}

Value wentropy(const Value &X, const std::string &type, double param,
               std::pmr::memory_resource *mr)
{
    std::string t = type;
    std::transform(t.begin(), t.end(), t.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto x = vecFromValue(X);
    const std::size_t n = x.size();
    double e = 0.0;

    if (t == "shannon") {
        for (double v : x) { const double s2 = v * v; if (s2 > 0.0) e -= s2 * std::log(s2); }
    } else if (t == "log energy" || t == "logenergy") {
        for (double v : x) { const double s2 = v * v; if (s2 > 0.0) e += std::log(s2); }
    } else if (t == "threshold") {
        for (double v : x) if (std::fabs(v) > param) e += 1.0;
    } else if (t == "sure") {
        e = static_cast<double>(n);
        const double t2 = param * param;
        for (double v : x) {
            if (std::fabs(v) <= param) e -= 2.0;
            e += std::min(v * v, t2);
        }
    } else if (t == "norm") {
        if (param < 1.0)
            throw Error("wentropy: 'norm' exponent P must be >= 1",
                        0, 0, "wentropy", "", "numkit:wentropy:norm");
        for (double v : x) e += std::pow(std::fabs(v), param);
    } else {
        throw Error("wentropy: unknown type '" + type +
                        "' (shannon / log energy / threshold / sure / norm)",
                    0, 0, "wentropy", "", "numkit:wentropy:type");
    }
    return Value::scalar(e, mr);
}

} // namespace numkit::wavelet
