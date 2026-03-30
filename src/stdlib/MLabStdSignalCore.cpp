#include "MLabStdLibrary.hpp"
#include "MLabSignalHelpers.hpp"

#include <cmath>

namespace mlab {

void StdLibrary::registerSignalCoreFunctions(Engine &engine)
{
    // --- nextpow2(n) ---
    engine.registerFunction("nextpow2",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                double n = args[0].toScalar();
                                if (n <= 0)
                                    return {MValue::scalar(0.0, alloc)};
                                return {MValue::scalar(std::ceil(std::log2(n)), alloc)};
                            });

    // --- fft(x) / fft(x, N) ---
    engine.registerFunction("fft",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty())
                                    throw std::runtime_error("fft requires at least 1 argument");
                                auto &x = args[0];
                                size_t inputLen = x.numel();
                                size_t outLen = (args.size() >= 2)
                                                    ? static_cast<size_t>(args[1].toScalar())
                                                    : inputLen;
                                size_t useLen = std::min(inputLen, outLen);
                                size_t fftLen = nextPow2(outLen);

                                auto buf = prepareFFTBuffer(x, useLen, fftLen);
                                fftRadix2(buf, 1);
                                return {packComplexResult(buf, outLen, alloc)};
                            });

    // --- ifft(X) / ifft(X, N) ---
    engine.registerFunction("ifft",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty())
                                    throw std::runtime_error("ifft requires at least 1 argument");
                                auto &x = args[0];
                                size_t inputLen = x.numel();
                                size_t outLen = (args.size() >= 2)
                                                    ? static_cast<size_t>(args[1].toScalar())
                                                    : inputLen;
                                size_t useLen = std::min(inputLen, outLen);
                                size_t fftLen = nextPow2(outLen);

                                auto buf = prepareFFTBuffer(x, useLen, fftLen);

                                // IFFT = conjugate → FFT → conjugate → /N
                                for (auto &v : buf) v = std::conj(v);
                                fftRadix2(buf, 1);
                                double invN = 1.0 / static_cast<double>(fftLen);
                                for (auto &v : buf) v = std::conj(v) * invN;

                                // Check if result is effectively real
                                bool isReal = true;
                                for (size_t i = 0; i < outLen && isReal; ++i)
                                    if (std::abs(buf[i].imag()) > 1e-10)
                                        isReal = false;

                                if (isReal) {
                                    auto r = MValue::matrix(1, outLen, MType::DOUBLE, alloc);
                                    for (size_t i = 0; i < outLen; ++i)
                                        r.doubleDataMut()[i] = buf[i].real();
                                    return {r};
                                }
                                return {packComplexResult(buf, outLen, alloc)};
                            });

    // --- fftshift(X) ---
    engine.registerFunction("fftshift",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty())
                                    throw std::runtime_error("fftshift requires 1 argument");
                                auto &x = args[0];
                                size_t N = x.numel();
                                size_t shift = N / 2;

                                if (x.isComplex()) {
                                    auto r = MValue::complexMatrix(x.dims().rows(), x.dims().cols(), alloc);
                                    const Complex *src = x.complexData();
                                    Complex *dst = r.complexDataMut();
                                    for (size_t i = 0; i < N; ++i)
                                        dst[i] = src[(i + shift) % N];
                                    return {r};
                                }
                                auto r = MValue::matrix(x.dims().rows(), x.dims().cols(), MType::DOUBLE, alloc);
                                const double *src = x.doubleData();
                                double *dst = r.doubleDataMut();
                                for (size_t i = 0; i < N; ++i)
                                    dst[i] = src[(i + shift) % N];
                                return {r};
                            });

    // --- ifftshift(X) ---
    engine.registerFunction("ifftshift",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty())
                                    throw std::runtime_error("ifftshift requires 1 argument");
                                auto &x = args[0];
                                size_t N = x.numel();
                                size_t shift = (N + 1) / 2;

                                if (x.isComplex()) {
                                    auto r = MValue::complexMatrix(x.dims().rows(), x.dims().cols(), alloc);
                                    const Complex *src = x.complexData();
                                    Complex *dst = r.complexDataMut();
                                    for (size_t i = 0; i < N; ++i)
                                        dst[i] = src[(i + shift) % N];
                                    return {r};
                                }
                                auto r = MValue::matrix(x.dims().rows(), x.dims().cols(), MType::DOUBLE, alloc);
                                const double *src = x.doubleData();
                                double *dst = r.doubleDataMut();
                                for (size_t i = 0; i < N; ++i)
                                    dst[i] = src[(i + shift) % N];
                                return {r};
                            });
}

} // namespace mlab
