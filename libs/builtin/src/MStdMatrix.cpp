// libs/builtin/src/MStdMatrix.cpp

#include <numkit/m/builtin/MStdMatrix.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"
#include "MStdReductionHelpers.hpp"
#include "backends/BinaryOpsLoops.hpp"
#include "backends/MStdCumSum.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>
#include <type_traits>
#include <vector>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

// ── Constructors ──────────────────────────────────────────────────────
MValue zeros(Allocator &alloc, size_t rows, size_t cols, size_t pages)
{
    return createMatrix({rows, cols, pages}, MType::DOUBLE, &alloc);
}

MValue ones(Allocator &alloc, size_t rows, size_t cols, size_t pages)
{
    auto m = createMatrix({rows, cols, pages}, MType::DOUBLE, &alloc);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < m.numel(); ++i)
        p[i] = 1.0;
    return m;
}

// ND overloads: caller passes a full dim vector. For nd <= 3 these just
// route to the legacy 2D/3D ctors via createMatrixND; nd > 3 hits the
// MValue::matrixND ctor and the SBO Dims storage.
MValue zerosND(Allocator &alloc, const std::vector<size_t> &dims)
{
    return createMatrixND(dims, MType::DOUBLE, &alloc);
}

MValue onesND(Allocator &alloc, const std::vector<size_t> &dims)
{
    auto m = createMatrixND(dims, MType::DOUBLE, &alloc);
    double *p = m.doubleDataMut();
    for (size_t i = 0; i < m.numel(); ++i)
        p[i] = 1.0;
    return m;
}

MValue eye(Allocator &alloc, size_t rows, size_t cols)
{
    auto m = MValue::matrix(rows, cols, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < std::min(rows, cols); ++i)
        m.elem(i, i) = 1.0;
    return m;
}

// ── Shape queries ────────────────────────────────────────────────────
MValue size(Allocator &alloc, const MValue &x)
{
    const auto &dims = x.dims();
    // Output ndim: at least 2 (MATLAB convention — a row vector reports
    // [1, n], not [n]). Otherwise the actual rank, including any extra
    // dims past 3.
    const int n = std::max(2, dims.ndim());
    auto sv = MValue::matrix(1, n, MType::DOUBLE, &alloc);
    double *out = sv.doubleDataMut();
    for (int i = 0; i < n; ++i)
        out[i] = static_cast<double>(dims.dim(i));
    return sv;
}

MValue size(Allocator &alloc, const MValue &x, int dim)
{
    return MValue::scalar(static_cast<double>(x.dims().dimSize(dim - 1)), &alloc);
}

std::tuple<MValue, MValue> sizePair(Allocator &alloc, const MValue &x)
{
    const auto &dims = x.dims();
    return std::make_tuple(
        MValue::scalar(static_cast<double>(dims.rows()), &alloc),
        MValue::scalar(static_cast<double>(dims.cols()), &alloc));
}

MValue length(Allocator &alloc, const MValue &x)
{
    if (x.isEmpty() || x.numel() == 0)
        return MValue::scalar(0.0, &alloc);
    const auto &dims = x.dims();
    const double len = static_cast<double>(std::max({dims.rows(), dims.cols(), dims.pages()}));
    return MValue::scalar(len, &alloc);
}

MValue numel(Allocator &alloc, const MValue &x)
{
    return MValue::scalar(static_cast<double>(x.numel()), &alloc);
}

MValue ndims(Allocator &alloc, const MValue &x)
{
    return MValue::scalar(static_cast<double>(x.dims().ndims()), &alloc);
}

// ── Shape transformations ────────────────────────────────────────────
MValue reshape(Allocator &alloc, const MValue &x, size_t rows, size_t cols, size_t pages)
{
    const size_t newNumel = rows * cols * (pages == 0 ? 1 : pages);
    if (newNumel != x.numel())
        throw MError("Number of elements must not change in reshape",
                     0, 0, "reshape", "", "m:reshape:elementCountMismatch");

    DimsArg d{rows, cols, pages};

    // CELL and STRING store element-wise, not in the raw buffer — memcpy
    // wouldn't copy MValue members.
    if (x.type() == MType::CELL || x.type() == MType::STRING) {
        const bool is3D = d.pages > 0;
        MValue r = (x.type() == MType::CELL)
            ? (is3D ? MValue::cell3D(d.rows, d.cols, d.pages)
                    : MValue::cell(d.rows, d.cols))
            : (is3D ? MValue::stringArray3D(d.rows, d.cols, d.pages)
                    : MValue::stringArray(d.rows, d.cols));
        auto &src = x.cellDataVec();
        auto &dst = r.cellDataVec();
        for (size_t i = 0; i < src.size() && i < dst.size(); ++i)
            dst[i] = src[i];
        return r;
    }

    auto r = createMatrix(d, x.type(), &alloc);
    if (x.rawBytes() > 0)
        std::memcpy(r.rawDataMut(), x.rawData(), x.rawBytes());
    return r;
}

// ND reshape. Same elem-count check, then route to matrixND for nd > 3.
// CELL/STRING ND not supported yet (matches the 2D/3D behaviour: only
// CELL/STRING currently handles 2D and 3D shapes via cell3D/stringArray3D).
MValue reshapeND(Allocator &alloc, const MValue &x,
                 const std::vector<size_t> &dims)
{
    size_t newNumel = 1;
    for (size_t d : dims) newNumel *= d;
    if (newNumel != x.numel())
        throw MError("Number of elements must not change in reshape",
                     0, 0, "reshape", "", "m:reshape:elementCountMismatch");

    if (x.type() == MType::CELL || x.type() == MType::STRING) {
        if (dims.size() > 3)
            throw MError("reshape: ND CELL/STRING (>3) not yet supported",
                         0, 0, "reshape", "", "m:reshape:cellND");
        // Fall through to legacy path for 2D / 3D cell.
        const size_t r = dims.size() > 0 ? dims[0] : 1;
        const size_t c = dims.size() > 1 ? dims[1] : 1;
        const size_t p = dims.size() > 2 ? dims[2] : 0;
        return reshape(alloc, x, r, c, p);
    }

    auto r = createMatrixND(dims, x.type(), &alloc);
    if (x.rawBytes() > 0)
        std::memcpy(r.rawDataMut(), x.rawData(), x.rawBytes());
    return r;
}

MValue transpose(Allocator &alloc, const MValue &x)
{
    if (x.dims().is3D())
        throw MError("transpose is not defined for N-D arrays",
                     0, 0, "transpose", "", "m:transpose:3DInput");
    const size_t rows = x.dims().rows(), cols = x.dims().cols();
    auto r = MValue::matrix(cols, rows, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < rows; ++i)
        for (size_t j = 0; j < cols; ++j)
            r.elem(j, i) = x(i, j);
    return r;
}

// ── pagemtimes: page-wise matrix multiply ──────────────────────────────
//
// MATLAB R2020b+ batched matmul. Treats axes 1-2 of each operand as the
// matrix (M×K, K×N) and axes ≥3 as a batch index. Output batch shape is
// the NumPy broadcast of the two batch shapes. Supports DOUBLE and
// SINGLE (mixed → SINGLE, matching MATLAB's promotion rule).
//
//   Z = pagemtimes(X, Y)                     // tx = ty = None
//   Z = pagemtimes(X, "transpose", Y, "none")
//
// 'transpose' / 'ctranspose' transpose each X (or Y) page before
// multiply. For real input the two flags are identical (no imaginary
// component to conjugate). 2D × 2D collapses to ordinary matmul. One
// operand may be 2D (broadcast across the other's batch dims).

namespace {

// Per-page matmul kernel, parameterised by element type. The DOUBLE
// specialisation hands off to the SIMD-aware matmulDoubleLoop in the
// backend; the SINGLE one uses the same (j, k, i) ordering as a
// portable inline loop.
template <typename T>
inline void runPageMatmul(const T *, const T *, T *,
                          size_t, size_t, size_t);

template <>
inline void runPageMatmul<double>(const double *a, const double *b, double *c,
                                  size_t M, size_t N, size_t K)
{
    detail::matmulDoubleLoop(a, b, c, M, N, K);
}

template <>
inline void runPageMatmul<float>(const float *a, const float *b, float *c,
                                 size_t M, size_t N, size_t K)
{
    for (size_t j = 0; j < N; ++j) {
        float *cj = c + j * M;
        for (size_t i = 0; i < M; ++i) cj[i] = 0.0f;
        for (size_t k = 0; k < K; ++k) {
            const float bkj = b[j * K + k];
            const float *ak = a + k * M;
            for (size_t i = 0; i < M; ++i)
                cj[i] += ak[i] * bkj;
        }
    }
}

template <>
inline void runPageMatmul<Complex>(const Complex *a, const Complex *b, Complex *c,
                                   size_t M, size_t N, size_t K)
{
    for (size_t j = 0; j < N; ++j) {
        Complex *cj = c + j * M;
        for (size_t i = 0; i < M; ++i) cj[i] = Complex(0.0, 0.0);
        for (size_t k = 0; k < K; ++k) {
            const Complex bkj = b[j * K + k];
            const Complex *ak = a + k * M;
            for (size_t i = 0; i < M; ++i)
                cj[i] += ak[i] * bkj;
        }
    }
}

template <typename T> constexpr MType pagemtimesElemMType();
template <> constexpr MType pagemtimesElemMType<double >() { return MType::DOUBLE;  }
template <> constexpr MType pagemtimesElemMType<float  >() { return MType::SINGLE;  }
template <> constexpr MType pagemtimesElemMType<Complex>() { return MType::COMPLEX; }

// Read element i of `src` as T. For T = Complex, real-typed sources
// upgrade to (real, 0); for T ∈ {double, float}, complex sources are
// rejected upstream so we never reach the if-branch with COMPLEX input.
template <typename T>
inline T readElemAsT(const MValue &src, size_t i, bool typeMatches)
{
    if constexpr (std::is_same_v<T, Complex>) {
        if (typeMatches) return src.complexData()[i];
        return Complex(src.elemAsDouble(i), 0.0);
    } else {
        if (typeMatches) return static_cast<const T *>(src.rawData())[i];
        return static_cast<T>(src.elemAsDouble(i));
    }
}

// Conjugate a value if T is Complex; identity for real T.
template <typename T>
inline T conjIfComplex(T v)
{
    if constexpr (std::is_same_v<T, Complex>) return std::conj(v);
    else return v;
}

// Materialise one page from `src` into typed scratch `dst`, optionally
// transposing (and conjugating, for ctranspose on Complex). Direct copy
// (no per-element conversion) when src already holds the target type
// AND no transpose is needed.
template <typename T>
void materialisePage(T *dst, const MValue &src, size_t pageOff,
                     size_t rowDim, size_t colDim, TranspOp tr)
{
    const size_t pageElems = rowDim * colDim;
    const size_t base = pageOff * pageElems;
    const bool typeMatches = (src.type() == pagemtimesElemMType<T>());

    if (tr == TranspOp::None) {
        if (typeMatches) {
            std::memcpy(dst, static_cast<const T *>(src.rawData()) + base,
                        pageElems * sizeof(T));
        } else {
            for (size_t i = 0; i < pageElems; ++i)
                dst[i] = readElemAsT<T>(src, base + i, false);
        }
        return;
    }
    // Transpose: dst is colDim × rowDim col-major;
    // dst[r * colDim + c] = src[c * rowDim + r] (then conjugate if ctranspose+Complex).
    const bool needsConj = (tr == TranspOp::CTranspose);
    for (size_t r = 0; r < rowDim; ++r) {
        for (size_t c = 0; c < colDim; ++c) {
            const size_t srcOff = base + c * rowDim + r;
            T v = readElemAsT<T>(src, srcOff, typeMatches);
            if (needsConj) v = conjIfComplex<T>(v);
            dst[r * colDim + c] = v;
        }
    }
}

template <typename T>
MValue pagemtimesImpl(Allocator &alloc,
                      const MValue &x, TranspOp tx,
                      const MValue &y, TranspOp ty)
{
    const auto &xd = x.dims();
    const auto &yd = y.dims();
    const int xnd = xd.ndim();
    const int ynd = yd.ndim();
    if (xnd < 2 || ynd < 2)
        throw MError("pagemtimes: each input must have at least 2 dimensions",
                     0, 0, "pagemtimes", "", "m:pagemtimes:rank");

    const size_t xRowDim = xd.dim(0), xColDim = xd.dim(1);
    const size_t yRowDim = yd.dim(0), yColDim = yd.dim(1);

    const size_t M  = (tx == TranspOp::None) ? xRowDim : xColDim;
    const size_t Kx = (tx == TranspOp::None) ? xColDim : xRowDim;
    const size_t Ky = (ty == TranspOp::None) ? yRowDim : yColDim;
    const size_t N  = (ty == TranspOp::None) ? yColDim : yRowDim;
    if (Kx != Ky)
        throw MError("pagemtimes: inner matrix dimensions must agree",
                     0, 0, "pagemtimes", "", "m:pagemtimes:innerdim");
    const size_t K = Kx;

    constexpr int kMaxNd = Dims::kMaxRank;
    const int xb = std::max(0, xnd - 2);
    const int yb = std::max(0, ynd - 2);
    const int outBatchNd = std::max(xb, yb);
    size_t xBatch[kMaxNd], yBatch[kMaxNd], outBatch[kMaxNd];
    for (int i = 0; i < outBatchNd; ++i) {
        xBatch[i] = (i < xb) ? xd.dim(2 + i) : 1;
        yBatch[i] = (i < yb) ? yd.dim(2 + i) : 1;
        if (xBatch[i] != yBatch[i] && xBatch[i] != 1 && yBatch[i] != 1)
            throw MError("pagemtimes: batch dimensions must broadcast "
                         "(each axis must match or be 1)",
                         0, 0, "pagemtimes", "", "m:pagemtimes:dimagree");
        outBatch[i] = std::max(xBatch[i], yBatch[i]);
    }

    size_t batchN = 1;
    for (int i = 0; i < outBatchNd; ++i) batchN *= outBatch[i];

    const int outNd = 2 + outBatchNd;
    size_t outDimArr[kMaxNd];
    outDimArr[0] = M;
    outDimArr[1] = N;
    for (int i = 0; i < outBatchNd; ++i) outDimArr[2 + i] = outBatch[i];
    auto z = createForDims(Dims(outDimArr, outNd), pagemtimesElemMType<T>(), &alloc);
    if (M == 0 || N == 0 || batchN == 0)
        return z;

    T *zData = static_cast<T *>(z.rawDataMut());
    const size_t xPageStride = xRowDim * xColDim;
    const size_t yPageStride = yRowDim * yColDim;
    const size_t zPageStride = M * N;

    // Direct-pass when source already matches T and no transpose is
    // needed; otherwise materialise into typed scratch (one per call,
    // reused across all batch pages).
    const bool xDirect = (x.type() == pagemtimesElemMType<T>()) && (tx == TranspOp::None);
    const bool yDirect = (y.type() == pagemtimesElemMType<T>()) && (ty == TranspOp::None);
    std::vector<T> scratchX, scratchY;
    if (!xDirect) scratchX.resize(xPageStride);
    if (!yDirect) scratchY.resize(yPageStride);

    auto getXPage = [&](size_t pageOff) -> const T * {
        if (xDirect)
            return static_cast<const T *>(x.rawData()) + pageOff * xPageStride;
        materialisePage(scratchX.data(), x, pageOff, xRowDim, xColDim, tx);
        return scratchX.data();
    };
    auto getYPage = [&](size_t pageOff) -> const T * {
        if (yDirect)
            return static_cast<const T *>(y.rawData()) + pageOff * yPageStride;
        materialisePage(scratchY.data(), y, pageOff, yRowDim, yColDim, ty);
        return scratchY.data();
    };

    if (outBatchNd == 0) {
        runPageMatmul<T>(getXPage(0), getYPage(0), zData, M, N, K);
        return z;
    }

    size_t xBatchStride[kMaxNd], yBatchStride[kMaxNd];
    {
        size_t sx = 1, sy = 1;
        for (int i = 0; i < outBatchNd; ++i) {
            xBatchStride[i] = sx;
            yBatchStride[i] = sy;
            sx *= xBatch[i];
            sy *= yBatch[i];
        }
    }

    size_t coords[kMaxNd] = {0};
    Dims outBatchDims(outBatch, outBatchNd);
    size_t pageIdx = 0;
    do {
        size_t xOff = 0, yOff = 0;
        for (int i = 0; i < outBatchNd; ++i) {
            const size_t xc = (xBatch[i] == 1) ? 0 : coords[i];
            const size_t yc = (yBatch[i] == 1) ? 0 : coords[i];
            xOff += xc * xBatchStride[i];
            yOff += yc * yBatchStride[i];
        }
        runPageMatmul<T>(getXPage(xOff), getYPage(yOff),
                         zData + pageIdx * zPageStride,
                         M, N, K);
        ++pageIdx;
    } while (incrementCoords(coords, outBatchDims));

    return z;
}

} // namespace

MValue pagemtimes(Allocator &alloc, const MValue &x, const MValue &y)
{
    return pagemtimes(alloc, x, TranspOp::None, y, TranspOp::None);
}

MValue pagemtimes(Allocator &alloc,
                  const MValue &x, TranspOp tx,
                  const MValue &y, TranspOp ty)
{
    // MATLAB type promotion: COMPLEX wins over real; SINGLE wins over
    // DOUBLE. Integer/logical/char inputs are rejected — pagemtimes
    // requires floating or complex inputs.
    auto isFloatLike = [](MType t) {
        return t == MType::DOUBLE || t == MType::SINGLE || t == MType::COMPLEX;
    };
    if (!isFloatLike(x.type()) || !isFloatLike(y.type()))
        throw MError("pagemtimes: inputs must be 'single', 'double', or complex",
                     0, 0, "pagemtimes", "", "m:pagemtimes:type");
    if (x.isComplex() || y.isComplex())
        return pagemtimesImpl<Complex>(alloc, x, tx, y, ty);
    if (x.type() == MType::SINGLE || y.type() == MType::SINGLE)
        return pagemtimesImpl<float  >(alloc, x, tx, y, ty);
    return     pagemtimesImpl<double >(alloc, x, tx, y, ty);
}

MValue diag(Allocator &alloc, const MValue &x)
{
    if (x.dims().isVector()) {
        const size_t n = x.numel();
        auto r = MValue::matrix(n, n, MType::DOUBLE, &alloc);
        for (size_t i = 0; i < n; ++i)
            r.elem(i, i) = x.doubleData()[i];
        return r;
    }
    const size_t n = std::min(x.dims().rows(), x.dims().cols());
    auto r = MValue::matrix(n, 1, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < n; ++i)
        r.doubleDataMut()[i] = x(i, i);
    return r;
}

// ── Sort / find ──────────────────────────────────────────────────────
std::tuple<MValue, MValue> sort(Allocator &alloc, const MValue &x)
{
    if (x.isScalar())
        return std::make_tuple(x, MValue::scalar(1.0, &alloc));

    const size_t R = x.dims().rows(), C = x.dims().cols();
    const size_t P = x.dims().is3D() ? x.dims().pages() : 1;
    const int sortDim = (R > 1) ? 0 : (C > 1) ? 1 : 2;
    const size_t N = (sortDim == 0) ? R : (sortDim == 1) ? C : P;

    auto r = x.dims().is3D() ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                             : MValue::matrix(R, C, MType::DOUBLE, &alloc);
    auto idx = x.dims().is3D() ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                               : MValue::matrix(R, C, MType::DOUBLE, &alloc);

    const size_t slice0 = (sortDim == 0) ? 1 : R;
    const size_t slice1 = (sortDim == 1) ? 1 : C;
    const size_t slice2 = (sortDim == 2) ? 1 : P;
    std::vector<std::pair<double, size_t>> buf(N);

    for (size_t pp = 0; pp < slice2; ++pp)
        for (size_t c = 0; c < slice1; ++c)
            for (size_t rr = 0; rr < slice0; ++rr) {
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    buf[k] = {x.doubleData()[pIdx * R * C + cIdx * R + rIdx], k};
                }
                std::sort(buf.begin(), buf.end(),
                          [](const auto &a, const auto &b) { return a.first < b.first; });
                for (size_t k = 0; k < N; ++k) {
                    const size_t rIdx = (sortDim == 0) ? k : rr;
                    const size_t cIdx = (sortDim == 1) ? k : c;
                    const size_t pIdx = (sortDim == 2) ? k : pp;
                    const size_t lin = pIdx * R * C + cIdx * R + rIdx;
                    r.doubleDataMut()[lin] = buf[k].first;
                    idx.doubleDataMut()[lin] = static_cast<double>(buf[k].second + 1);
                }
            }
    return std::make_tuple(std::move(r), std::move(idx));
}

MValue find(Allocator &alloc, const MValue &x)
{
    std::vector<double> indices;
    if (x.isLogical()) {
        const uint8_t *ld = x.logicalData();
        for (size_t i = 0; i < x.numel(); ++i)
            if (ld[i])
                indices.push_back(static_cast<double>(i + 1));
    } else {
        const double *dd = x.doubleData();
        for (size_t i = 0; i < x.numel(); ++i)
            if (dd[i] != 0.0)
                indices.push_back(static_cast<double>(i + 1));
    }
    const bool rowResult = !x.dims().is3D() && x.dims().rows() == 1;
    auto r = rowResult ? MValue::matrix(1, indices.size(), MType::DOUBLE, &alloc)
                       : MValue::matrix(indices.size(), 1, MType::DOUBLE, &alloc);
    if (!indices.empty())
        std::memcpy(r.doubleDataMut(), indices.data(), indices.size() * sizeof(double));
    return r;
}

// ── Concatenation ────────────────────────────────────────────────────
MValue horzcat(Allocator &alloc, const MValue *values, size_t count)
{
    if (count == 0)
        return MValue::empty();
    return MValue::horzcat(values, count, &alloc);
}

MValue vertcat(Allocator &alloc, const MValue *values, size_t count)
{
    if (count == 0)
        return MValue::empty();
    return MValue::vertcat(values, count, &alloc);
}

// ── Grids ────────────────────────────────────────────────────────────
std::tuple<MValue, MValue> meshgrid(Allocator &alloc, const MValue &x, const MValue &y)
{
    const size_t nx = x.numel(), ny = y.numel();
    auto X = MValue::matrix(ny, nx, MType::DOUBLE, &alloc);
    auto Y = MValue::matrix(ny, nx, MType::DOUBLE, &alloc);
    for (size_t r = 0; r < ny; ++r)
        for (size_t c = 0; c < nx; ++c) {
            X.elem(r, c) = x.doubleData()[c];
            Y.elem(r, c) = y.doubleData()[r];
        }
    return std::make_tuple(std::move(X), std::move(Y));
}

// ── Reductions and products ──────────────────────────────────────────
MValue cumsum(Allocator &alloc, const MValue &x)
{
    if (x.isScalar()) {
        auto r = MValue::matrix(x.dims().rows(), x.dims().cols(), MType::DOUBLE, &alloc);
        r.doubleDataMut()[0] = x.toScalar();
        return r;
    }
    if (x.dims().isVector()) {
        auto r = MValue::matrix(x.dims().rows(), x.dims().cols(), MType::DOUBLE, &alloc);
        cumsumScan(x.doubleData(), r.doubleDataMut(), x.numel());
        return r;
    }
    const size_t R = x.dims().rows(), C = x.dims().cols();
    auto r = MValue::matrix(R, C, MType::DOUBLE, &alloc);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    // Per-column inclusive scan — column data is contiguous.
    for (size_t c = 0; c < C; ++c)
        cumsumScan(src + c * R, dst + c * R, R);
    return r;
}

// cumsum along an explicit dim. Output shape equals input shape (this is
// not a reduction). Vector / scalar input ignores dim and walks linearly.
MValue cumsum(Allocator &alloc, const MValue &x, int dim)
{
    if (dim <= 0) return cumsum(alloc, x);
    if (x.dims().isVector() || x.isScalar()) return cumsum(alloc, x);

    const int d = detail::resolveDim(x, dim, "cumsum");
    const auto &dd = x.dims();

    // ND fallback for rank ≥ 4: per-slice scan along axis d-1. Inner
    // block size B = prod(dim[0..d-2]); outer count O = prod(dim[d..]).
    if (dd.ndim() >= 4) {
        constexpr int kMaxNd = Dims::kMaxRank;
        if (dd.ndim() > kMaxNd)
            throw MError("cumsum: rank exceeds 32",
                         0, 0, "cumsum", "", "m:cumsum:tooManyDims");
        size_t outDims[kMaxNd];
        for (int i = 0; i < dd.ndim(); ++i) outDims[i] = dd.dim(i);
        auto r = MValue::matrixND(outDims, dd.ndim(), MType::DOUBLE, &alloc);
        const size_t sliceLen = dd.dim(d - 1);
        size_t B = 1;
        for (int i = 0; i < d - 1; ++i) B *= dd.dim(i);
        size_t O = 1;
        for (int i = d; i < dd.ndim(); ++i) O *= dd.dim(i);
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        if (B == 1) {
            for (size_t o = 0; o < O; ++o) {
                const size_t base = o * sliceLen;
                cumsumScan(src + base, dst + base, sliceLen);
            }
        } else {
            for (size_t o = 0; o < O; ++o)
                for (size_t b = 0; b < B; ++b) {
                    const size_t base = o * sliceLen * B + b;
                    if (sliceLen == 0) continue;
                    double acc = src[base];
                    dst[base] = acc;
                    for (size_t k = 1; k < sliceLen; ++k) {
                        acc += src[base + k * B];
                        dst[base + k * B] = acc;
                    }
                }
        }
        return r;
    }

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                       : MValue::matrix(R, C, MType::DOUBLE, &alloc);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    if (d == 1) {
        // dim=1: scan down each column. Column data is contiguous so
        // route through the SIMD prefix-sum kernel.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                cumsumScan(src + base, dst + base, R);
            }
    } else if (d == 2) {
        // Walk across columns for each (row, page). Stride = R.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t rr = 0; rr < R; ++rr) {
                double s = 0;
                const size_t pageBase = pp * R * C;
                for (size_t c = 0; c < C; ++c) {
                    s += src[pageBase + c * R + rr];
                    dst[pageBase + c * R + rr] = s;
                }
            }
    } else if (d == 3) {
        // Walk through pages for each (row, col). Stride = R*C.
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                double s = 0;
                for (size_t pp = 0; pp < P; ++pp) {
                    s += src[pp * R * C + c * R + rr];
                    dst[pp * R * C + c * R + rr] = s;
                }
            }
    }
    return r;
}

// ── Generic cumulative kernel for cumprod / cummax / cummin ─────────
//
// Op is a binary functor (double, double) -> double. Init is the value
// the running accumulator starts at; for cumulative ops we instead seed
// with the first element of the slice, but seeding behavior is still
// captured by Op (e.g. cumprod could just use init=1.0 multiplicative).
// We use the seed-with-first style so cummax / cummin work without
// needing to know +/- infinity for the initial value.
namespace {

template <typename Op>
void cumKernel(const MValue &x, int d, Op op, double *dst)
{
    const auto &dd = x.dims();
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    const double *src = x.doubleData();

    if (d == 1) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                if (R == 0) continue;
                double acc = src[base];
                dst[base] = acc;
                for (size_t rr = 1; rr < R; ++rr) {
                    acc = op(acc, src[base + rr]);
                    dst[base + rr] = acc;
                }
            }
    } else if (d == 2) {
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t rr = 0; rr < R; ++rr) {
                const size_t pageBase = pp * R * C;
                if (C == 0) continue;
                double acc = src[pageBase + rr];
                dst[pageBase + rr] = acc;
                for (size_t c = 1; c < C; ++c) {
                    acc = op(acc, src[pageBase + c * R + rr]);
                    dst[pageBase + c * R + rr] = acc;
                }
            }
    } else if (d == 3) {
        for (size_t c = 0; c < C; ++c)
            for (size_t rr = 0; rr < R; ++rr) {
                if (P == 0) continue;
                double acc = src[c * R + rr];
                dst[c * R + rr] = acc;
                for (size_t pp = 1; pp < P; ++pp) {
                    acc = op(acc, src[pp * R * C + c * R + rr]);
                    dst[pp * R * C + c * R + rr] = acc;
                }
            }
    }
}

template <typename Op>
MValue cumImpl(Allocator &alloc, const MValue &x, int dim,
               Op op, const char *fn)
{
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);

    if (x.dims().isVector() || x.isScalar()) {
        auto r = MValue::matrix(x.dims().rows(), x.dims().cols(),
                                MType::DOUBLE, &alloc);
        if (x.numel() == 0) return r;
        double acc = x.doubleData()[0];
        r.doubleDataMut()[0] = acc;
        for (size_t i = 1; i < x.numel(); ++i) {
            acc = op(acc, x.doubleData()[i]);
            r.doubleDataMut()[i] = acc;
        }
        return r;
    }

    const int d = detail::resolveDim(x, dim, fn);
    const auto &dd = x.dims();
    auto r = dd.is3D() ? MValue::matrix3d(dd.rows(), dd.cols(), dd.pages(),
                                          MType::DOUBLE, &alloc)
                       : MValue::matrix(dd.rows(), dd.cols(),
                                        MType::DOUBLE, &alloc);
    cumKernel(x, d, op, r.doubleDataMut());
    return r;
}

} // namespace

// cumprod / cummax / cummin: SIMD prefix-op kernels in
// backends/MStdCumSum_{simd,portable}.cpp handle vector input and the
// dim=1 (column) path where access is contiguous. For dim=2/3 the
// strided access pattern doesn't benefit from SIMD; cumImpl's scalar
// cumKernel still handles those (with the same Op as before).
namespace {

using ScanFn = void (*)(const double *, double *, std::size_t);

template <typename Op>
MValue cumScanDispatch(Allocator &alloc, const MValue &x, int dim,
                       ScanFn scan, Op scalarOp, const char *fn)
{
    if (x.isEmpty())
        return MValue::matrix(0, 0, MType::DOUBLE, &alloc);
    if (x.isScalar()) {
        auto r = MValue::matrix(x.dims().rows(), x.dims().cols(), MType::DOUBLE, &alloc);
        r.doubleDataMut()[0] = x.toScalar();
        return r;
    }
    if (x.dims().isVector()) {
        auto r = MValue::matrix(x.dims().rows(), x.dims().cols(), MType::DOUBLE, &alloc);
        scan(x.doubleData(), r.doubleDataMut(), x.numel());
        return r;
    }

    const int d = detail::resolveDim(x, dim, fn);
    const auto &dd = x.dims();

    // ND fallback (rank ≥ 4): per-slice scan along axis d-1.
    if (dd.ndim() >= 4) {
        constexpr int kMaxNd = Dims::kMaxRank;
        if (dd.ndim() > kMaxNd)
            throw MError(std::string(fn) + ": rank exceeds 32",
                         0, 0, fn, "", std::string("m:") + fn + ":tooManyDims");
        size_t outDims[kMaxNd];
        for (int i = 0; i < dd.ndim(); ++i) outDims[i] = dd.dim(i);
        auto r = MValue::matrixND(outDims, dd.ndim(), MType::DOUBLE, &alloc);
        const size_t sliceLen = dd.dim(d - 1);
        size_t B = 1;
        for (int i = 0; i < d - 1; ++i) B *= dd.dim(i);
        size_t O = 1;
        for (int i = d; i < dd.ndim(); ++i) O *= dd.dim(i);
        const double *src = x.doubleData();
        double *dst = r.doubleDataMut();
        if (B == 1) {
            for (size_t o = 0; o < O; ++o) {
                const size_t base = o * sliceLen;
                scan(src + base, dst + base, sliceLen);
            }
        } else {
            for (size_t o = 0; o < O; ++o)
                for (size_t b = 0; b < B; ++b) {
                    const size_t base = o * sliceLen * B + b;
                    if (sliceLen == 0) continue;
                    double acc = src[base];
                    dst[base] = acc;
                    for (size_t k = 1; k < sliceLen; ++k) {
                        acc = scalarOp(acc, src[base + k * B]);
                        dst[base + k * B] = acc;
                    }
                }
        }
        return r;
    }

    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                       : MValue::matrix(R, C, MType::DOUBLE, &alloc);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    if (d == 1) {
        // Per-column scan — column data is contiguous, route through SIMD.
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                const size_t base = pp * R * C + c * R;
                scan(src + base, dst + base, R);
            }
    } else {
        // dim=2/3: strided access; reuse the existing scalar cumKernel.
        cumKernel(x, d, scalarOp, dst);
    }
    return r;
}

} // namespace

MValue cumprod(Allocator &alloc, const MValue &x, int dim)
{
    return cumScanDispatch(alloc, x, dim, cumprodScan,
                           [](double a, double b) { return a * b; }, "cumprod");
}

MValue cummax(Allocator &alloc, const MValue &x, int dim)
{
    // NaN propagation: MATLAB cummax skips NaN if 'omitnan' is passed
    // and propagates otherwise. Default = 'omitnan' since R2018a; we
    // skip NaN here, treating them as identity.
    return cumScanDispatch(alloc, x, dim, cummaxScan,
                           [](double a, double b) {
                               if (std::isnan(b)) return a;
                               if (std::isnan(a)) return b;
                               return std::max(a, b);
                           }, "cummax");
}

MValue cummin(Allocator &alloc, const MValue &x, int dim)
{
    return cumScanDispatch(alloc, x, dim, cumminScan,
                           [](double a, double b) {
                               if (std::isnan(b)) return a;
                               if (std::isnan(a)) return b;
                               return std::min(a, b);
                           }, "cummin");
}

// ── any / all moved to backends/MStdLogicalReductions_{simd,portable}.cpp
//
// MATLAB's any(X) returns true if ANY element is non-zero (NaN counts
// as true since NaN != 0). all(X) returns true if ALL elements are
// non-zero. Empty: any → false, all → true (vacuously). The SIMD
// backend scans LOGICAL bytes and DOUBLE lanes directly with early
// exit (Phase P1 of project_perf_optimization_plan.md).

namespace {

// Used by xor below — small inputs, no need for a SIMD path.
MValue promoteToDouble(const MValue &x, Allocator &alloc)
{
    if (x.type() == MType::DOUBLE) return x;
    auto r = createLike(x, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < x.numel(); ++i)
        r.doubleDataMut()[i] = x.elemAsDouble(i);
    return r;
}

} // namespace

// ── xor (elementwise logical) ────────────────────────────────────────
MValue xorOf(Allocator &alloc, const MValue &a, const MValue &b)
{
    auto ad = promoteToDouble(a, alloc);
    auto bd = promoteToDouble(b, alloc);
    auto d = elementwiseDouble(ad, bd,
        [](double aa, double bb) {
            return ((aa != 0.0) != (bb != 0.0)) ? 1.0 : 0.0;
        }, &alloc);
    if (d.isScalar()) return MValue::logicalScalar(d.toScalar() != 0.0, &alloc);
    auto r = createLike(d, MType::LOGICAL, &alloc);
    for (size_t i = 0; i < d.numel(); ++i)
        r.logicalDataMut()[i] = (d.doubleData()[i] != 0.0) ? 1 : 0;
    return r;
}

MValue cross(Allocator &alloc, const MValue &a, const MValue &b)
{
    if (a.numel() != 3 || b.numel() != 3)
        throw MError("cross requires 3-element vectors",
                     0, 0, "cross", "", "m:cross:badSize");
    auto r = MValue::matrix(1, 3, MType::DOUBLE, &alloc);
    r.doubleDataMut()[0] = a.doubleData()[1] * b.doubleData()[2] - a.doubleData()[2] * b.doubleData()[1];
    r.doubleDataMut()[1] = a.doubleData()[2] * b.doubleData()[0] - a.doubleData()[0] * b.doubleData()[2];
    r.doubleDataMut()[2] = a.doubleData()[0] * b.doubleData()[1] - a.doubleData()[1] * b.doubleData()[0];
    return r;
}

MValue dot(Allocator &alloc, const MValue &a, const MValue &b)
{
    if (a.numel() != b.numel())
        throw MError("dot: vectors must have same length",
                     0, 0, "dot", "", "m:dot:lengthMismatch");
    double s = 0;
    for (size_t i = 0; i < a.numel(); ++i)
        s += a.doubleData()[i] * b.doubleData()[i];
    return MValue::scalar(s, &alloc);
}

// ════════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void zeros_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    auto d = parseDimsArgsND(args);
    stripTrailingOnes(d);
    outs[0] = zerosND(ctx.engine->allocator(), d);
}

void ones_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    auto d = parseDimsArgsND(args);
    stripTrailingOnes(d);
    outs[0] = onesND(ctx.engine->allocator(), d);
}

void eye_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    auto d = parseDimsArgs(args);
    outs[0] = eye(ctx.engine->allocator(), d.rows, d.cols);
}

void size_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("Not enough input arguments",
                     0, 0, "size", "", "m:size:nargin");
    auto &alloc = ctx.engine->allocator();

    if (args.size() >= 2) {
        outs[0] = size(alloc, args[0], static_cast<int>(args[1].toScalar()));
        return;
    }

    if (nargout > 1) {
        const auto &dims = args[0].dims();
        // Multi-output form: [r, c] = size(A) or [r, c, p, ...] = size(A).
        // For ND tensors, dims past nargout-1 are gathered into the last
        // requested output (MATLAB behaviour: extra-dim sizes multiplied
        // into the trailing slot). For dims past actual ndim, return 1.
        for (size_t i = 0; i < nargout && i < outs.size(); ++i) {
            double v;
            if (i + 1 < nargout) {
                v = static_cast<double>(dims.dim(static_cast<int>(i)));
            } else {
                // Last requested output: multiply remaining dims (if any).
                size_t prod = 1;
                for (int j = static_cast<int>(i); j < dims.ndim(); ++j)
                    prod *= dims.dim(j);
                if (dims.ndim() <= static_cast<int>(i)) prod = 1;
                v = static_cast<double>(prod);
            }
            outs[i] = MValue::scalar(v, &alloc);
        }
        return;
    }

    outs[0] = size(alloc, args[0]);
}

void length_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("length: requires 1 argument",
                     0, 0, "length", "", "m:length:nargin");
    outs[0] = length(ctx.engine->allocator(), args[0]);
}

void numel_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("numel: requires 1 argument",
                     0, 0, "numel", "", "m:numel:nargin");
    outs[0] = numel(ctx.engine->allocator(), args[0]);
}

void ndims_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("ndims: requires 1 argument",
                     0, 0, "ndims", "", "m:ndims:nargin");
    outs[0] = ndims(ctx.engine->allocator(), args[0]);
}

void reshape_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("reshape: requires at least 2 arguments",
                     0, 0, "reshape", "", "m:reshape:nargin");

    const auto &x = args[0];
    std::vector<size_t> dims;

    // Dims-vector form: reshape(A, [m n p ...]). No [] inference here.
    if (args.size() == 2 && !args[1].isScalar() && !args[1].isEmpty()) {
        dims = parseDimsArgsND(args.subspan(1));
    } else {
        // Scalar-args form: reshape(A, m, n, ...). One [] allowed for
        // dimension inference from x.numel().
        const size_t dimCount = args.size() - 1;
        dims.assign(dimCount, 1);
        int inferPos = -1;
        size_t knownProd = 1;
        for (size_t i = 0; i < dimCount; ++i) {
            if (args[i + 1].isEmpty()) {
                if (inferPos >= 0)
                    throw MError("reshape: only one dimension may be inferred via []",
                                 0, 0, "reshape", "", "m:reshape:tooManyInferred");
                inferPos = static_cast<int>(i);
            } else {
                dims[i] = static_cast<size_t>(args[i + 1].toScalar());
                knownProd *= dims[i];
            }
        }
        if (inferPos >= 0) {
            if (knownProd == 0 || x.numel() % knownProd != 0)
                throw MError("reshape: size of array must be divisible by product of known dims",
                             0, 0, "reshape", "", "m:reshape:indivisible");
            dims[inferPos] = x.numel() / knownProd;
        }
    }

    // Strip trailing 1s past the 2nd dim (MATLAB convention).
    stripTrailingOnes(dims);
    outs[0] = reshapeND(ctx.engine->allocator(), x, dims);
}

void transpose_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("transpose: requires 1 argument",
                     0, 0, "transpose", "", "m:transpose:nargin");
    outs[0] = transpose(ctx.engine->allocator(), args[0]);
}

void pagemtimes_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    auto parseFlag = [](const MValue &v) -> TranspOp {
        if (!v.isChar() && !v.isString())
            throw MError("pagemtimes: transpose flag must be a string",
                         0, 0, "pagemtimes", "", "m:pagemtimes:flagType");
        const std::string s = v.toString();
        if (s == "none")       return TranspOp::None;
        if (s == "transpose")  return TranspOp::Transpose;
        if (s == "ctranspose") return TranspOp::CTranspose;
        throw MError("pagemtimes: invalid transpose flag '" + s
                     + "' (expected 'none', 'transpose', or 'ctranspose')",
                     0, 0, "pagemtimes", "", "m:pagemtimes:invalidFlag");
    };
    Allocator &alloc = ctx.engine->allocator();
    if (args.size() == 2) {
        outs[0] = pagemtimes(alloc, args[0], args[1]);
        return;
    }
    if (args.size() == 4) {
        outs[0] = pagemtimes(alloc,
                             args[0], parseFlag(args[1]),
                             args[2], parseFlag(args[3]));
        return;
    }
    throw MError("pagemtimes: expected 2 or 4 arguments",
                 0, 0, "pagemtimes", "", "m:pagemtimes:nargin");
}

void diag_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("diag: requires 1 argument",
                     0, 0, "diag", "", "m:diag:nargin");
    outs[0] = diag(ctx.engine->allocator(), args[0]);
}

void sort_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("sort: requires 1 argument",
                     0, 0, "sort", "", "m:sort:nargin");
    auto [sorted, idx] = sort(ctx.engine->allocator(), args[0]);
    outs[0] = std::move(sorted);
    if (nargout > 1)
        outs[1] = std::move(idx);
}

void find_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("find: requires 1 argument",
                     0, 0, "find", "", "m:find:nargin");
    outs[0] = find(ctx.engine->allocator(), args[0]);
}

void horzcat_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    outs[0] = horzcat(ctx.engine->allocator(), args.data(), args.size());
}

void vertcat_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    outs[0] = vertcat(ctx.engine->allocator(), args.data(), args.size());
}

void meshgrid_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("meshgrid: requires 2 arguments",
                     0, 0, "meshgrid", "", "m:meshgrid:nargin");
    auto [X, Y] = meshgrid(ctx.engine->allocator(), args[0], args[1]);
    outs[0] = std::move(X);
    if (nargout > 1)
        outs[1] = std::move(Y);
}

void cumsum_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("cumsum: requires at least 1 argument",
                     0, 0, "cumsum", "", "m:cumsum:nargin");
    int dim = 0;
    if (args.size() >= 2 && !args[1].isEmpty())
        dim = static_cast<int>(args[1].toScalar());
    outs[0] = (dim > 0) ? cumsum(ctx.engine->allocator(), args[0], dim)
                        : cumsum(ctx.engine->allocator(), args[0]);
}

#define NK_CUM_REG(name)                                                       \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,               \
                    Span<MValue> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw MError(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        int dim = 0;                                                           \
        if (args.size() >= 2 && !args[1].isEmpty())                            \
            dim = static_cast<int>(args[1].toScalar());                        \
        outs[0] = name(ctx.engine->allocator(), args[0], dim);                 \
    }

NK_CUM_REG(cumprod)
NK_CUM_REG(cummax)
NK_CUM_REG(cummin)

#undef NK_CUM_REG

#define NK_LOGICAL_RED_REG(name, fn)                                           \
    void name##_reg(Span<const MValue> args, size_t /*nargout*/,               \
                    Span<MValue> outs, CallContext &ctx)                       \
    {                                                                          \
        if (args.empty())                                                      \
            throw MError(#name ": requires at least 1 argument",               \
                         0, 0, #name, "", "m:" #name ":nargin");               \
        int dim = 0;                                                           \
        if (args.size() >= 2 && !args[1].isEmpty())                            \
            dim = static_cast<int>(args[1].toScalar());                        \
        outs[0] = fn(ctx.engine->allocator(), args[0], dim);                   \
    }

NK_LOGICAL_RED_REG(any, anyOf)
NK_LOGICAL_RED_REG(all, allOf)

#undef NK_LOGICAL_RED_REG

void xor_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("xor: requires 2 arguments",
                     0, 0, "xor", "", "m:xor:nargin");
    outs[0] = xorOf(ctx.engine->allocator(), args[0], args[1]);
}

void cross_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("cross: requires 2 arguments",
                     0, 0, "cross", "", "m:cross:nargin");
    outs[0] = cross(ctx.engine->allocator(), args[0], args[1]);
}

void dot_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("dot: requires 2 arguments",
                     0, 0, "dot", "", "m:dot:nargin");
    outs[0] = dot(ctx.engine->allocator(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::m::builtin
