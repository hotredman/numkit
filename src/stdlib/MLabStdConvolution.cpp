#include "MLabStdLibrary.hpp"
#include "MLabSignalHelpers.hpp"

#include <algorithm>
#include <cmath>

namespace mlab {

void StdLibrary::registerConvolutionFunctions(Engine &engine)
{
    // --- conv(a, b) / conv(a, b, shape) ---
    engine.registerFunction("conv",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("conv requires at least 2 arguments");

                                auto &a = args[0];
                                auto &b = args[1];
                                std::string shape = "full";
                                if (args.size() >= 3 && args[2].isChar())
                                    shape = args[2].toString();

                                size_t na = a.numel(), nb = b.numel();

                                std::vector<double> c;
                                if (na * nb > CONV_FFT_THRESHOLD * CONV_FFT_THRESHOLD)
                                    c = convFFT(a.doubleData(), na, b.doubleData(), nb);
                                else
                                    c = convDirect(a.doubleData(), na, b.doubleData(), nb);

                                size_t nc = c.size();
                                size_t outStart = 0, outLen = nc;
                                if (shape == "same") {
                                    outLen = std::max(na, nb);
                                    outStart = (nc - outLen) / 2;
                                } else if (shape == "valid") {
                                    outLen = (na >= nb) ? na - nb + 1 : nb - na + 1;
                                    outStart = std::min(na, nb) - 1;
                                }

                                auto r = MValue::matrix(1, outLen, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < outLen; ++i)
                                    r.doubleDataMut()[i] = c[outStart + i];
                                return {r};
                            });

    // --- deconv(b, a) --- polynomial long division
    engine.registerFunction("deconv",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("deconv requires 2 arguments");

                                auto &bv = args[0];
                                auto &av = args[1];
                                size_t nb = bv.numel(), na = av.numel();

                                if (na > nb)
                                    throw std::runtime_error("deconv: denominator longer than numerator");

                                std::vector<double> rem(bv.doubleData(), bv.doubleData() + nb);
                                const double *ad = av.doubleData();

                                size_t nq = nb - na + 1;
                                std::vector<double> q(nq);

                                double a0 = ad[0];
                                if (a0 == 0.0)
                                    throw std::runtime_error("deconv: leading coefficient is zero");

                                for (size_t i = 0; i < nq; ++i) {
                                    q[i] = rem[i] / a0;
                                    for (size_t j = 0; j < na; ++j)
                                        rem[i + j] -= q[i] * ad[j];
                                }

                                auto qv = MValue::matrix(1, nq, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < nq; ++i)
                                    qv.doubleDataMut()[i] = q[i];

                                auto rv = MValue::matrix(1, nb, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < nb; ++i)
                                    rv.doubleDataMut()[i] = rem[i];

                                return {qv, rv};
                            });

    // --- xcorr(x) / xcorr(x, y) --- cross-correlation via conv
    engine.registerFunction("xcorr",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty())
                                    throw std::runtime_error("xcorr requires at least 1 argument");

                                const double *x = args[0].doubleData();
                                size_t nx = args[0].numel();

                                const double *y;
                                size_t ny;
                                bool autoCorr = (args.size() < 2 || args[1].isChar());
                                if (autoCorr) {
                                    y = x;
                                    ny = nx;
                                } else {
                                    y = args[1].doubleData();
                                    ny = args[1].numel();
                                }

                                size_t maxLen = std::max(nx, ny);
                                size_t nc = nx + ny - 1;

                                // Reverse y
                                std::vector<double> yRev(ny);
                                for (size_t i = 0; i < ny; ++i)
                                    yRev[i] = y[ny - 1 - i];

                                std::vector<double> c;
                                if (nx * ny > CONV_FFT_THRESHOLD * CONV_FFT_THRESHOLD)
                                    c = convFFT(x, nx, yRev.data(), ny);
                                else
                                    c = convDirect(x, nx, yRev.data(), ny);

                                auto r = MValue::matrix(1, nc, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < nc; ++i)
                                    r.doubleDataMut()[i] = c[i];

                                int maxLag = static_cast<int>(maxLen) - 1;
                                auto lags = MValue::matrix(1, nc, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < nc; ++i)
                                    lags.doubleDataMut()[i] = static_cast<double>(static_cast<int>(i) - maxLag);

                                return {r, lags};
                            });
}

} // namespace mlab
