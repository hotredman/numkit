// libs/image/src/quality/quality.cpp

#include <numkit/image/quality/quality.hpp>

#include <numkit/image/filter/filter.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace numkit::image {

namespace {

double class_peak(ValueType t) {
    switch (t) {
        case ValueType::UINT8:  return 255.0;
        case ValueType::UINT16: return 65535.0;
        case ValueType::INT16:  return 32767.0;
        default:                return 1.0;
    }
}

// Element-wise difference squared, summed.
double sum_squared_error(const Value &A, const Value &B) {
    const size_t N = A.numel();
    double s = 0.0;
    for (size_t i = 0; i < N; ++i) {
        const double d = A.elemAsDouble(i) - B.elemAsDouble(i);
        s += d * d;
    }
    return s;
}

} // anonymous

Value immse(const Value &A, const Value &B, std::pmr::memory_resource *mr) {
    if (A.numel() != B.numel())
        throw Error("immse: A and B must have the same number of elements",
                    0, 0, "immse", "", "m:immse:size");
    const double mse = (A.numel() > 0)
                       ? sum_squared_error(A, B) / double(A.numel())
                       : 0.0;
    return Value::scalar(mse, mr);
}

Value psnr(const Value &A, const Value &B, double peak, std::pmr::memory_resource *mr) {
    if (A.numel() != B.numel())
        throw Error("psnr: A and B must have the same number of elements",
                    0, 0, "psnr", "", "m:psnr:size");
    if (std::isnan(peak)) peak = class_peak(A.type());
    if (A.numel() == 0) return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    const double mse = sum_squared_error(A, B) / double(A.numel());
    if (mse == 0.0) return Value::scalar(std::numeric_limits<double>::infinity(), mr);
    const double db = 10.0 * std::log10(peak * peak / mse);
    return Value::scalar(db, mr);
}

Value ssim(const Value &A, const Value &B, std::pmr::memory_resource *mr) {
    if (A.numel() != B.numel())
        throw Error("ssim: A and B must have the same number of elements",
                    0, 0, "ssim", "", "m:ssim:size");
    const size_t H = A.dims().rows();
    const size_t W = A.dims().cols();
    if (H == 0 || W == 0) return Value::scalar(1.0, mr);

    // Convert to DOUBLE in [0, 1] for stable SSIM computation.
    auto unit = [&](const Value &X) {
        std::vector<double> out(X.numel());
        const double scale = 1.0 / class_peak(X.type());
        for (size_t i = 0; i < X.numel(); ++i)
            out[i] = X.elemAsDouble(i) * scale;
        return out;
    };
    auto Au = unit(A), Bu = unit(B);

    // Gaussian window kernel: 11×11, σ=1.5.
    Value gK = fspecial("gaussian", { 11.0, 11.0, 1.5 }, mr);

    // Wrap Au and Bu back as Values so we can re-use imfilter.
    auto pack = [&](const std::vector<double> &v) {
        Value V = Value::matrix(H, W, ValueType::DOUBLE, mr);
        double *vd = V.doubleDataMut();
        // Au is in column-major (because Value is column-major and we
        // copied via elemAsDouble which respects col-major linear index).
        for (size_t i = 0; i < v.size(); ++i) vd[i] = v[i];
        return V;
    };
    Value Av = pack(Au), Bv = pack(Bu);

    // Filtered means.
    auto box_filt = [&](const Value &X) {
        return imfilter(X, gK, PadMode::Replicate, 0.0, /*full=*/false, /*flip_kernel=*/false, mr);
    };

    Value mu_a = box_filt(Av);
    Value mu_b = box_filt(Bv);

    // Variances and covariance.
    Value Aa = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value Bb = Value::matrix(H, W, ValueType::DOUBLE, mr);
    Value Ab = Value::matrix(H, W, ValueType::DOUBLE, mr);
    {
        double *aa = Aa.doubleDataMut();
        double *bb = Bb.doubleDataMut();
        double *ab = Ab.doubleDataMut();
        for (size_t i = 0; i < H * W; ++i) {
            const double a = Av.doubleData()[i];
            const double b = Bv.doubleData()[i];
            aa[i] = a * a;
            bb[i] = b * b;
            ab[i] = a * b;
        }
    }
    Value sa2 = box_filt(Aa);
    Value sb2 = box_filt(Bb);
    Value sab = box_filt(Ab);

    const double K1 = 0.01, K2 = 0.03, L = 1.0;  // L = dynamic range (we scaled to [0,1])
    const double C1 = (K1 * L) * (K1 * L);
    const double C2 = (K2 * L) * (K2 * L);

    double total = 0.0;
    size_t cnt = 0;
    for (size_t i = 0; i < H * W; ++i) {
        const double mua = mu_a.doubleData()[i];
        const double mub = mu_b.doubleData()[i];
        const double va  = sa2.doubleData()[i] - mua * mua;
        const double vb  = sb2.doubleData()[i] - mub * mub;
        const double cov = sab.doubleData()[i] - mua * mub;

        const double num = (2.0 * mua * mub + C1) * (2.0 * cov + C2);
        const double den = (mua * mua + mub * mub + C1) * (va + vb + C2);
        if (den > 0.0) { total += num / den; ++cnt; }
    }
    const double s = (cnt > 0) ? total / double(cnt) : 1.0;
    return Value::scalar(s, mr);
}

Value mean2(const Value &A, std::pmr::memory_resource *mr)
{
    const size_t N = A.numel();
    if (N == 0) return Value::scalar(std::nan(""), mr);
    long double s = 0.0L;
    for (size_t i = 0; i < N; ++i) s += A.elemAsDouble(i);
    return Value::scalar(static_cast<double>(s / static_cast<long double>(N)),
                         mr);
}

Value std2(const Value &A, std::pmr::memory_resource *mr)
{
    const size_t N = A.numel();
    if (N == 0) return Value::scalar(std::nan(""), mr);
    if (N == 1) return Value::scalar(0.0, mr);
    long double s = 0.0L;
    for (size_t i = 0; i < N; ++i) s += A.elemAsDouble(i);
    const long double mu = s / static_cast<long double>(N);
    long double v = 0.0L;
    for (size_t i = 0; i < N; ++i) {
        const long double d = A.elemAsDouble(i) - mu;
        v += d * d;
    }
    // std2 wraps std(I(:)) → sample std (normalize by N-1).
    v /= static_cast<long double>(N - 1);
    return Value::scalar(static_cast<double>(std::sqrt((double)v)), mr);
}

Value corr2(const Value &A, const Value &B, std::pmr::memory_resource *mr)
{
    const size_t N = A.numel();
    if (B.numel() != N)
        throw Error("corr2: A and B must have the same number of elements",
                    0, 0, "corr2", "", "m:corr2:size");
    if (N == 0) return Value::scalar(std::nan(""), mr);
    long double sa = 0.0L, sb = 0.0L;
    for (size_t i = 0; i < N; ++i) {
        sa += A.elemAsDouble(i);
        sb += B.elemAsDouble(i);
    }
    const long double ma = sa / static_cast<long double>(N);
    const long double mb = sb / static_cast<long double>(N);
    long double cov = 0.0L, va = 0.0L, vb = 0.0L;
    for (size_t i = 0; i < N; ++i) {
        const long double da = A.elemAsDouble(i) - ma;
        const long double db = B.elemAsDouble(i) - mb;
        cov += da * db;
        va  += da * da;
        vb  += db * db;
    }
    const long double denom = std::sqrt((double)(va * vb));
    if (denom == 0.0L) return Value::scalar(std::nan(""), mr);
    return Value::scalar(static_cast<double>(cov / denom), mr);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void immse_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("immse: requires (A, B)", 0, 0, "immse", "",
                    "m:immse:nargin");
    outs[0] = immse(args[0], args[1], ctx.engine->resource());
}

void psnr_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("psnr: requires (A, B[, peak])", 0, 0, "psnr", "",
                    "m:psnr:nargin");
    const double peak = (args.size() >= 3 && !args[2].isEmpty())
                        ? args[2].toScalar() : std::nan("");
    outs[0] = psnr(args[0], args[1], peak, ctx.engine->resource());
}

void ssim_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ssim: requires (A, B)", 0, 0, "ssim", "",
                    "m:ssim:nargin");
    outs[0] = ssim(args[0], args[1], ctx.engine->resource());
}

void mean2_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mean2: requires (A)", 0, 0, "mean2", "",
                    "m:mean2:nargin");
    outs[0] = mean2(args[0], ctx.engine->resource());
}

void std2_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("std2: requires (A)", 0, 0, "std2", "",
                    "m:std2:nargin");
    outs[0] = std2(args[0], ctx.engine->resource());
}

void corr2_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("corr2: requires (A, B)", 0, 0, "corr2", "",
                    "m:corr2:nargin");
    outs[0] = corr2(args[0], args[1], ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
