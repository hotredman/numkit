#include "MDspHelpers.hpp"
#include "MDspLibrary.hpp"
#include "MStdHelpers.hpp"

#include <cmath>

namespace numkit::m {

void DspLibrary::registerSignalCoreFunctions(Engine &engine)
{
    // --- nextpow2(n) ---
    engine.registerFunction("nextpow2",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                double n = args[0].toScalar();
                                if (n <= 0) {
                                    outs[0] = MValue::scalar(0.0, alloc);
                                    return;
                                }
                                {
                                    outs[0] = MValue::scalar(std::ceil(std::log2(n)), alloc);
                                    return;
                                }
                            });

    // Helper: run fft/ifft along a given dimension of a matrix.
    // dir=1 for fft, dir=-1 for ifft.
    // N=0 means "use input length along that dimension".
    auto fftAlongDim = [](const MValue &x, size_t N, int dim, int dir, Allocator *alloc) -> MValue {
        auto &dims = x.dims();
        size_t rows = dims.rows(), cols = dims.cols();

        // Vector case (dim irrelevant or dim=1 for column, dim=2 for row)
        if (rows == 1 || cols == 1) {
            size_t inputLen = x.numel();
            size_t outLen = (N > 0) ? N : inputLen;
            size_t useLen = std::min(inputLen, outLen);
            size_t fftLen = nextPow2(outLen);
            auto buf = prepareFFTBuffer(x, useLen, fftLen);

            if (dir == -1) {
                for (auto &v : buf) v = std::conj(v);
                fftRadix2(buf, 1);
                double invN = 1.0 / static_cast<double>(fftLen);
                for (auto &v : buf) v = std::conj(v) * invN;

                bool isReal = true;
                for (size_t i = 0; i < outLen && isReal; ++i)
                    if (std::abs(buf[i].imag()) > 1e-10) isReal = false;
                if (isReal) {
                    bool isRow = (rows == 1);
                    auto r = isRow ? MValue::matrix(1, outLen, MType::DOUBLE, alloc)
                                   : MValue::matrix(outLen, 1, MType::DOUBLE, alloc);
                    for (size_t i = 0; i < outLen; ++i)
                        r.doubleDataMut()[i] = buf[i].real();
                    return r;
                }
                bool isRow = (rows == 1);
                auto r = isRow ? MValue::complexMatrix(1, outLen, alloc)
                               : MValue::complexMatrix(outLen, 1, alloc);
                for (size_t i = 0; i < outLen; ++i)
                    r.complexDataMut()[i] = buf[i];
                return r;
            }

            fftRadix2(buf, 1);
            bool isRow = (rows == 1);
            auto r = isRow ? MValue::complexMatrix(1, outLen, alloc)
                           : MValue::complexMatrix(outLen, 1, alloc);
            for (size_t i = 0; i < outLen; ++i)
                r.complexDataMut()[i] = buf[i];
            return r;
        }

        // Matrix case: apply FFT column-wise (dim=1) or row-wise (dim=2)
        if (dim == 1) {
            // FFT along columns
            size_t outRows = (N > 0) ? N : rows;
            auto result = MValue::complexMatrix(outRows, cols, alloc);
            Complex *dst = result.complexDataMut();
            for (size_t c = 0; c < cols; ++c) {
                size_t useLen = std::min(rows, outRows);
                size_t fftLen = nextPow2(outRows);
                std::vector<Complex> buf(fftLen, Complex(0.0, 0.0));
                if (x.isComplex()) {
                    const Complex *src = x.complexData();
                    for (size_t r = 0; r < useLen; ++r)
                        buf[r] = src[c * rows + r];
                } else {
                    const double *src = x.doubleData();
                    for (size_t r = 0; r < useLen; ++r)
                        buf[r] = Complex(src[c * rows + r], 0.0);
                }
                if (dir == -1) {
                    for (auto &v : buf) v = std::conj(v);
                    fftRadix2(buf, 1);
                    double invN = 1.0 / static_cast<double>(fftLen);
                    for (auto &v : buf) v = std::conj(v) * invN;
                } else {
                    fftRadix2(buf, 1);
                }
                for (size_t r = 0; r < outRows; ++r)
                    dst[c * outRows + r] = buf[r];
            }
            return result;
        } else {
            // FFT along rows (dim=2)
            size_t outCols = (N > 0) ? N : cols;
            auto result = MValue::complexMatrix(rows, outCols, alloc);
            Complex *dst = result.complexDataMut();
            for (size_t r = 0; r < rows; ++r) {
                size_t useLen = std::min(cols, outCols);
                size_t fftLen = nextPow2(outCols);
                std::vector<Complex> buf(fftLen, Complex(0.0, 0.0));
                if (x.isComplex()) {
                    const Complex *src = x.complexData();
                    for (size_t c = 0; c < useLen; ++c)
                        buf[c] = src[c * rows + r];
                } else {
                    const double *src = x.doubleData();
                    for (size_t c = 0; c < useLen; ++c)
                        buf[c] = Complex(src[c * rows + r], 0.0);
                }
                if (dir == -1) {
                    for (auto &v : buf) v = std::conj(v);
                    fftRadix2(buf, 1);
                    double invN = 1.0 / static_cast<double>(fftLen);
                    for (auto &v : buf) v = std::conj(v) * invN;
                } else {
                    fftRadix2(buf, 1);
                }
                for (size_t c = 0; c < outCols; ++c)
                    dst[c * rows + r] = buf[c];
            }
            return result;
        }
    };

    // --- fft(x), fft(x,N), fft(x,[],dim), fft(x,N,dim) ---
    engine.registerFunction("fft",
                            [fftAlongDim](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.empty())
                                    throw std::runtime_error("fft requires at least 1 argument");
                                auto &x = args[0];
                                size_t N = 0; // 0 = use input length
                                int dim = 1;  // default: along columns (dim=1)
                                if (args.size() >= 2 && !args[1].isEmpty())
                                    N = static_cast<size_t>(args[1].toScalar());
                                if (args.size() >= 3)
                                    dim = static_cast<int>(args[2].toScalar());
                                outs[0] = fftAlongDim(x, N, dim, 1, alloc);
                            });

    // --- ifft(X), ifft(X,N), ifft(X,[],dim), ifft(X,N,dim) ---
    engine.registerFunction("ifft",
                            [fftAlongDim](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.empty())
                                    throw std::runtime_error("ifft requires at least 1 argument");
                                auto &x = args[0];
                                size_t N = 0;
                                int dim = 1;
                                if (args.size() >= 2 && !args[1].isEmpty())
                                    N = static_cast<size_t>(args[1].toScalar());
                                if (args.size() >= 3)
                                    dim = static_cast<int>(args[2].toScalar());
                                outs[0] = fftAlongDim(x, N, dim, -1, alloc);
                            });

    // --- fftshift(X) ---
    engine.registerFunction(
        "fftshift",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty())
                throw std::runtime_error("fftshift requires 1 argument");
            auto &x = args[0];
            size_t N = x.numel();
            size_t shift = N / 2;

            if (x.isComplex()) {
                auto r = createLike(x, MType::COMPLEX, alloc);
                const Complex *src = x.complexData();
                Complex *dst = r.complexDataMut();
                for (size_t i = 0; i < N; ++i)
                    dst[i] = src[(i + shift) % N];
                {
                    outs[0] = r;
                    return;
                }
            }
            auto r = createLike(x, MType::DOUBLE, alloc);
            const double *src = x.doubleData();
            double *dst = r.doubleDataMut();
            for (size_t i = 0; i < N; ++i)
                dst[i] = src[(i + shift) % N];
            {
                outs[0] = r;
                return;
            }
        });

    // --- ifftshift(X) ---
    engine.registerFunction(
        "ifftshift",
        [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.empty())
                throw std::runtime_error("ifftshift requires 1 argument");
            auto &x = args[0];
            size_t N = x.numel();
            size_t shift = (N + 1) / 2;

            if (x.isComplex()) {
                auto r = createLike(x, MType::COMPLEX, alloc);
                const Complex *src = x.complexData();
                Complex *dst = r.complexDataMut();
                for (size_t i = 0; i < N; ++i)
                    dst[i] = src[(i + shift) % N];
                {
                    outs[0] = r;
                    return;
                }
            }
            auto r = createLike(x, MType::DOUBLE, alloc);
            const double *src = x.doubleData();
            double *dst = r.doubleDataMut();
            for (size_t i = 0; i < N; ++i)
                dst[i] = src[(i + shift) % N];
            {
                outs[0] = r;
                return;
            }
        });
}

} // namespace numkit::m