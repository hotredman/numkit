// libs/wavelet/src/denoise/denoise.cpp
//
// Wavelet thresholding primitives (wthresh / wnoisest) and the
// composite VisuShrink-style denoiser (wdenoise). All built on the
// existing wavedec / waverec / detcoef plumbing — no extra DSP
// machinery here, just thresholding and a robust σ estimate.

#include <numkit/wavelet/denoise/denoise.hpp>
#include <numkit/wavelet/dwt/multilevel.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
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

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected string argument",
                    0, 0, "", "", "numkit:wavelet:type");
    return v.toString();
}

void wthresh_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("wthresh: requires (X, sorh, T)",
                    0, 0, "wthresh", "", "numkit:wthresh:nargin");
    outs[0] = wthresh(args[0], argString(args[1]), args[2].toScalar(), ctx.engine->resource());
}

void wnoisest_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("wnoisest: requires (C, L, S)",
                    0, 0, "wnoisest", "", "numkit:wnoisest:nargin");
    outs[0] = wnoisest(args[0], args[1], args[2], ctx.engine->resource());
}

void wdenoise_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("wdenoise: requires the signal",
                    0, 0, "wdenoise", "", "numkit:wdenoise:nargin");
    int level = -1;
    if (args.size() >= 2 && !args[1].isEmpty())
        level = static_cast<int>(args[1].toScalar());
    std::string wname;
    if (args.size() >= 3 && !args[2].isEmpty())
        wname = argString(args[2]);
    outs[0] = wdenoise(args[0], level, wname, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::wavelet
