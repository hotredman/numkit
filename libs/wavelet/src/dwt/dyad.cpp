// libs/wavelet/src/dwt/dyad.cpp
//
// Dyadic upsample/downsample helpers and wmaxlev.

#include <numkit/wavelet/dwt/dyad.hpp>
#include <numkit/wavelet/filter/wfilters.hpp>

// Compute-only TU: Value substrate + Error, no engine. The dyaddown /
// dyadup / wmaxlev builtins (CallContext wrappers) live in dwt/dyad_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cmath>
#include <string>

namespace numkit::wavelet {

namespace {

void readShape(const Value &x, size_t &rows, size_t &cols)
{
    rows = x.dims().rows();
    cols = x.dims().cols();
    if (rows == 0 && cols == 0) { rows = 1; cols = 0; }
}

// Return shape with dimension `outN` along whichever axis was the
// non-singleton in the input. For column inputs (rows>1, cols==1)
// the output is M×1; otherwise 1×M.
void outShape(size_t inRows, size_t inCols, size_t outN,
              size_t &outRows, size_t &outCols)
{
    if (inCols == 1 && inRows > 1) { outRows = outN; outCols = 1; }
    else                            { outRows = 1;   outCols = outN; }
}

inline size_t outLenDown(size_t N, int odd)
{
    return (odd == 0) ? (N / 2) : ((N + 1) / 2);
}
inline size_t outLenUp(size_t N, int odd)
{
    if (N == 0) return 0;
    return (odd == 0) ? (2 * N - 1) : (2 * N + 1);
}
inline bool isMatrix(size_t rows, size_t cols)
{
    return rows > 1 && cols > 1;
}

} // anonymous

// ── Public C++ API (see dwt/dyad.hpp) ─────────────────────────────────

Value dyaddown(const Value &x, int odd, char type,
               std::pmr::memory_resource *mr)
{
    size_t rows, cols;
    readShape(x, rows, cols);

    if (!isMatrix(rows, cols)) {
        const size_t N = rows * cols;
        const size_t outN = outLenDown(N, odd);
        size_t outRows, outCols;
        outShape(rows, cols, outN, outRows, outCols);
        Value y = Value::matrix(outRows, outCols, ValueType::DOUBLE, mr);
        if (outN == 0) return y;
        double *yd = y.doubleDataMut();
        const size_t start = (odd == 0) ? 1 : 0;
        for (size_t k = 0; k < outN; ++k)
            yd[k] = x.elemAsDouble(start + 2 * k);
        return y;
    }

    // 2-D matrix: column-major data layout.
    const size_t startC = (odd == 0) ? 1 : 0;
    const size_t startR = (odd == 0) ? 1 : 0;
    const size_t outC   = outLenDown(cols, odd);
    const size_t outR   = outLenDown(rows, odd);

    size_t resR, resC;
    if (type == 'c') { resR = rows; resC = outC; }
    else if (type == 'r') { resR = outR; resC = cols; }
    else { resR = outR; resC = outC; }

    Value y = Value::matrix(resR, resC, ValueType::DOUBLE, mr);
    if (resR == 0 || resC == 0) return y;
    double *yd = y.doubleDataMut();

    if (type == 'c') {
        for (size_t kc = 0; kc < outC; ++kc) {
            const size_t srcC = startC + 2 * kc;
            for (size_t r = 0; r < rows; ++r)
                yd[kc * resR + r] = x.elemAsDouble(srcC * rows + r);
        }
    } else if (type == 'r') {
        for (size_t c = 0; c < cols; ++c) {
            for (size_t kr = 0; kr < outR; ++kr) {
                const size_t srcR = startR + 2 * kr;
                yd[c * resR + kr] = x.elemAsDouble(c * rows + srcR);
            }
        }
    } else {  // 'm'
        for (size_t kc = 0; kc < outC; ++kc) {
            const size_t srcC = startC + 2 * kc;
            for (size_t kr = 0; kr < outR; ++kr) {
                const size_t srcR = startR + 2 * kr;
                yd[kc * resR + kr] = x.elemAsDouble(srcC * rows + srcR);
            }
        }
    }
    return y;
}

Value dyadup(const Value &x, int odd, char type,
             std::pmr::memory_resource *mr)
{
    size_t rows, cols;
    readShape(x, rows, cols);

    if (!isMatrix(rows, cols)) {
        const size_t N = rows * cols;
        const size_t outN = outLenUp(N, odd);
        size_t outRows, outCols;
        outShape(rows, cols, outN, outRows, outCols);
        Value y = Value::matrix(outRows, outCols, ValueType::DOUBLE, mr);
        if (outN == 0) return y;
        double *yd = y.doubleDataMut();
        const size_t start = (odd == 0) ? 0 : 1;
        for (size_t k = 0; k < outN; ++k) yd[k] = 0.0;
        for (size_t i = 0; i < N; ++i)
            yd[start + 2 * i] = x.elemAsDouble(i);
        return y;
    }

    // 2-D matrix: insert zeros along the chosen axis.
    const size_t outC   = outLenUp(cols, odd);
    const size_t outR   = outLenUp(rows, odd);
    const size_t startC = (odd == 0) ? 0 : 1;
    const size_t startR = (odd == 0) ? 0 : 1;

    size_t resR, resC;
    if (type == 'c') { resR = rows; resC = outC; }
    else if (type == 'r') { resR = outR; resC = cols; }
    else { resR = outR; resC = outC; }
    Value y = Value::matrix(resR, resC, ValueType::DOUBLE, mr);
    if (resR == 0 || resC == 0) return y;
    double *yd = y.doubleDataMut();
    for (size_t i = 0; i < resR * resC; ++i) yd[i] = 0.0;

    if (type == 'c') {
        for (size_t i = 0; i < cols; ++i) {
            const size_t dstC = startC + 2 * i;
            for (size_t r = 0; r < rows; ++r)
                yd[dstC * resR + r] = x.elemAsDouble(i * rows + r);
        }
    } else if (type == 'r') {
        for (size_t c = 0; c < cols; ++c) {
            for (size_t i = 0; i < rows; ++i) {
                const size_t dstR = startR + 2 * i;
                yd[c * resR + dstR] = x.elemAsDouble(c * rows + i);
            }
        }
    } else {  // 'm'
        for (size_t i = 0; i < cols; ++i) {
            const size_t dstC = startC + 2 * i;
            for (size_t k = 0; k < rows; ++k) {
                const size_t dstR = startR + 2 * k;
                yd[dstC * resR + dstR] = x.elemAsDouble(i * rows + k);
            }
        }
    }
    return y;
}

Value wmaxlev(const Value &N, const std::string &wname,
              std::pmr::memory_resource *mr)
{
    // Accept scalar N or a 1×k vector; use min(N) as the effective length.
    const size_t k = N.numel();
    if (k == 0)
        throw Error("wmaxlev: N must not be empty",
                    0, 0, "wmaxlev", "", "numkit:wmaxlev:empty");
    double Nmin = N.elemAsDouble(0);
    for (size_t i = 1; i < k; ++i) {
        const double v = N.elemAsDouble(i);
        if (v < Nmin) Nmin = v;
    }

    auto fb = wavelet_filters(wname);
    const size_t Lf = fb.Lo_D.size();
    if (Lf < 2)
        throw Error("wmaxlev: filter length must be ≥ 2",
                    0, 0, "wmaxlev", "", "numkit:wmaxlev:filter");

    double L = 0.0;
    if (Nmin >= static_cast<double>(Lf - 1) + 1.0) {
        const double r = Nmin / static_cast<double>(Lf - 1);
        L = std::floor(std::log2(r));
        if (L < 0.0) L = 0.0;
    }
    return Value::scalar(L, mr);
}

} // namespace numkit::wavelet
