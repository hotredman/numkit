#include "MDspLibrary.hpp"

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit {

void DspLibrary::registerResampleFunctions(Engine &engine)
{
    // --- downsample(x, n) ---
    engine.registerFunction("downsample",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("downsample requires 2 arguments");
                                auto &xv = args[0];
                                size_t n = static_cast<size_t>(args[1].toScalar());
                                size_t nx = xv.numel();
                                size_t outLen = (nx + n - 1) / n;

                                bool isRow = xv.dims().rows() == 1;
                                auto r = isRow ? MValue::matrix(1, outLen, MType::DOUBLE, alloc)
                                               : MValue::matrix(outLen, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < outLen; ++i)
                                    r.doubleDataMut()[i] = xv.doubleData()[i * n];
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- upsample(x, n) ---
    engine.registerFunction("upsample",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("upsample requires 2 arguments");
                                auto &xv = args[0];
                                size_t n = static_cast<size_t>(args[1].toScalar());
                                size_t nx = xv.numel();
                                size_t outLen = nx * n;

                                bool isRow = xv.dims().rows() == 1;
                                auto r = isRow ? MValue::matrix(1, outLen, MType::DOUBLE, alloc)
                                               : MValue::matrix(outLen, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < outLen; ++i)
                                    r.doubleDataMut()[i] = 0.0;
                                for (size_t i = 0; i < nx; ++i)
                                    r.doubleDataMut()[i * n] = xv.doubleData()[i];
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- decimate(x, r) --- downsample with anti-aliasing FIR
    engine.registerFunction("decimate",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("decimate requires 2 arguments");
                                auto &xv = args[0];
                                size_t factor = static_cast<size_t>(args[1].toScalar());
                                size_t nx = xv.numel();
                                const double *x = xv.doubleData();

                                // FIR lowpass: windowed-sinc, cutoff at 1/(2*factor)
                                size_t filtOrder = 8 * factor;
                                if (filtOrder >= nx)
                                    filtOrder = nx - 1;
                                size_t filtLen = filtOrder + 1;
                                double wc = M_PI / factor;

                                std::vector<double> h(filtLen);
                                double half = filtOrder / 2.0;
                                double hSum = 0.0;
                                for (size_t i = 0; i < filtLen; ++i) {
                                    double n = i - half;
                                    double sinc = (std::abs(n) < 1e-12)
                                                      ? wc / M_PI
                                                      : std::sin(wc * n) / (M_PI * n);
                                    double win = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / filtOrder);
                                    h[i] = sinc * win;
                                    hSum += h[i];
                                }
                                for (size_t i = 0; i < filtLen; ++i)
                                    h[i] /= hSum;

                                // Apply FIR filter (Direct Form II transposed)
                                std::vector<double> filtered(nx);
                                std::vector<double> z(filtLen, 0.0);
                                for (size_t n = 0; n < nx; ++n) {
                                    filtered[n] = h[0] * x[n] + z[0];
                                    for (size_t i = 1; i < filtLen; ++i)
                                        z[i - 1] = h[i] * x[n] + (i < filtLen - 1 ? z[i] : 0.0);
                                }

                                // Downsample
                                size_t outLen = (nx + factor - 1) / factor;
                                bool isRow = xv.dims().rows() == 1;
                                auto r = isRow ? MValue::matrix(1, outLen, MType::DOUBLE, alloc)
                                               : MValue::matrix(outLen, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < outLen; ++i)
                                    r.doubleDataMut()[i] = filtered[i * factor];
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- resample(x, p, q) --- rational rate change p/q
    engine.registerFunction("resample",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 3)
                                    throw std::runtime_error("resample requires 3 arguments");
                                auto &xv = args[0];
                                size_t p = static_cast<size_t>(args[1].toScalar());
                                size_t q = static_cast<size_t>(args[2].toScalar());
                                size_t nx = xv.numel();
                                const double *x = xv.doubleData();

                                // Upsample by p
                                size_t upLen = nx * p;
                                std::vector<double> up(upLen, 0.0);
                                for (size_t i = 0; i < nx; ++i)
                                    up[i * p] = static_cast<double>(p) * x[i];

                                // Anti-aliasing FIR lowpass
                                size_t filtOrder = 10 * std::max(p, q);
                                if (filtOrder >= upLen)
                                    filtOrder = upLen - 1;
                                size_t filtLen = filtOrder + 1;
                                double wc = M_PI / std::max(p, q);

                                std::vector<double> h(filtLen);
                                double half = filtOrder / 2.0;
                                double hSum = 0.0;
                                for (size_t i = 0; i < filtLen; ++i) {
                                    double n = i - half;
                                    double sinc = (std::abs(n) < 1e-12)
                                                      ? wc / M_PI
                                                      : std::sin(wc * n) / (M_PI * n);
                                    double win = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / filtOrder);
                                    h[i] = sinc * win;
                                    hSum += h[i];
                                }
                                for (size_t i = 0; i < filtLen; ++i)
                                    h[i] /= hSum;

                                // Apply FIR
                                std::vector<double> filtered(upLen);
                                std::vector<double> z(filtLen, 0.0);
                                for (size_t n = 0; n < upLen; ++n) {
                                    filtered[n] = h[0] * up[n] + z[0];
                                    for (size_t i = 1; i < filtLen; ++i)
                                        z[i - 1] = h[i] * up[n] + (i < filtLen - 1 ? z[i] : 0.0);
                                }

                                // Downsample by q
                                size_t outLen = (upLen + q - 1) / q;
                                bool isRow = xv.dims().rows() == 1;
                                auto r = isRow ? MValue::matrix(1, outLen, MType::DOUBLE, alloc)
                                               : MValue::matrix(outLen, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < outLen; ++i) {
                                    size_t idx = i * q;
                                    r.doubleDataMut()[i] = (idx < upLen) ? filtered[idx] : 0.0;
                                }
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });
}

} // namespace numkit