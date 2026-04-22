// libs/builtin/src/MStdMatrix.cpp

#include <numkit/m/builtin/MStdMatrix.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MStdHelpers.hpp"
#include "MStdReductionHelpers.hpp"

#include <algorithm>
#include <cstring>
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
    if (dims.is3D()) {
        auto sv = MValue::matrix(1, 3, MType::DOUBLE, &alloc);
        sv.doubleDataMut()[0] = static_cast<double>(dims.rows());
        sv.doubleDataMut()[1] = static_cast<double>(dims.cols());
        sv.doubleDataMut()[2] = static_cast<double>(dims.pages());
        return sv;
    }
    auto sv = MValue::matrix(1, 2, MType::DOUBLE, &alloc);
    sv.doubleDataMut()[0] = static_cast<double>(dims.rows());
    sv.doubleDataMut()[1] = static_cast<double>(dims.cols());
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
    if (x.dims().isVector() || x.isScalar()) {
        auto r = MValue::matrix(x.dims().rows(), x.dims().cols(), MType::DOUBLE, &alloc);
        double s = 0;
        for (size_t i = 0; i < x.numel(); ++i) {
            s += x.doubleData()[i];
            r.doubleDataMut()[i] = s;
        }
        return r;
    }
    const size_t R = x.dims().rows(), C = x.dims().cols();
    auto r = MValue::matrix(R, C, MType::DOUBLE, &alloc);
    for (size_t c = 0; c < C; ++c) {
        double s = 0;
        for (size_t rr = 0; rr < R; ++rr) {
            s += x(rr, c);
            r.elem(rr, c) = s;
        }
    }
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
    const size_t R = dd.rows(), C = dd.cols();
    const size_t P = dd.is3D() ? dd.pages() : 1;
    auto r = dd.is3D() ? MValue::matrix3d(R, C, P, MType::DOUBLE, &alloc)
                       : MValue::matrix(R, C, MType::DOUBLE, &alloc);
    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    if (d == 1) {
        // Walk down rows for each (column, page).
        for (size_t pp = 0; pp < P; ++pp)
            for (size_t c = 0; c < C; ++c) {
                double s = 0;
                const size_t base = pp * R * C + c * R;
                for (size_t rr = 0; rr < R; ++rr) {
                    s += src[base + rr];
                    dst[base + rr] = s;
                }
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
    auto d = parseDimsArgs(args);
    outs[0] = zeros(ctx.engine->allocator(), d.rows, d.cols, d.pages);
}

void ones_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    auto d = parseDimsArgs(args);
    outs[0] = ones(ctx.engine->allocator(), d.rows, d.cols, d.pages);
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
        outs[0] = MValue::scalar(static_cast<double>(dims.rows()), &alloc);
        if (nargout > 1)
            outs[1] = MValue::scalar(static_cast<double>(dims.cols()), &alloc);
        if (nargout > 2)
            outs[2] = MValue::scalar(static_cast<double>(dims.pages()), &alloc);
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
    size_t rows, cols, pages;

    // Dims-vector form: reshape(A, [m n]) or [m n p]. No [] inference supported here.
    if (args.size() == 2 && !args[1].isScalar() && !args[1].isEmpty()) {
        auto d = parseDimsArgs(args.subspan(1));
        rows = d.rows;
        cols = d.cols;
        pages = d.pages;
    } else {
        // Scalar-args form: reshape(A, m, n) or (A, m, n, p). One [] allowed
        // for dimension inference from x.numel().
        const size_t dimCount = args.size() - 1;
        size_t dims[3] = {1, 1, static_cast<size_t>((dimCount >= 3) ? 1 : 0)};
        int inferPos = -1;
        size_t knownProd = 1;
        for (size_t i = 0; i < dimCount && i < 3; ++i) {
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
        rows = dims[0];
        cols = dims[1];
        pages = (dimCount >= 3) ? dims[2] : 0;
    }

    outs[0] = reshape(ctx.engine->allocator(), x, rows, cols, pages);
}

void transpose_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("transpose: requires 1 argument",
                     0, 0, "transpose", "", "m:transpose:nargin");
    outs[0] = transpose(ctx.engine->allocator(), args[0]);
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
