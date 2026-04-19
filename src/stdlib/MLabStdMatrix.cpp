#include "MLabStdHelpers.hpp"
#include "MLabStdLibrary.hpp"

#include <algorithm>
#include <cstring>

namespace mlab {

void StdLibrary::registerMatrixFunctions(Engine &engine)
{
    // --- zeros ---
    engine.registerFunction("zeros",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto d = parseDimsArgs(args);
                                outs[0] = createMatrix(d, MType::DOUBLE, &ctx.engine->allocator());
                            });

    // --- ones ---
    engine.registerFunction("ones",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto d = parseDimsArgs(args);
                                auto m = createMatrix(d, MType::DOUBLE, &ctx.engine->allocator());
                                double *p = m.doubleDataMut();
                                for (size_t i = 0; i < m.numel(); ++i)
                                    p[i] = 1.0;
                                outs[0] = std::move(m);
                            });

    // --- eye ---
    engine.registerFunction("eye",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto d = parseDimsArgs(args);
                                auto m = createMatrix(d, MType::DOUBLE, &ctx.engine->allocator());
                                for (size_t i = 0; i < std::min(d.rows, d.cols); ++i)
                                    m.elem(i, i) = 1.0;
                                outs[0] = std::move(m);
                            });

    // --- size ---
    engine.registerFunction(
        "size", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("Not enough input arguments");
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            auto &dims = a.dims();

            if (args.size() >= 2) {
                int dim = static_cast<int>(args[1].toScalar());
                outs[0] = MValue::scalar(static_cast<double>(dims.dimSize(dim - 1)), alloc);
                return;
            }

            if (nargout > 1) {
                outs[0] = MValue::scalar(static_cast<double>(dims.rows()), alloc);
                if (nargout > 1)
                    outs[1] = MValue::scalar(static_cast<double>(dims.cols()), alloc);
                if (nargout > 2)
                    outs[2] = MValue::scalar(static_cast<double>(dims.pages()), alloc);
                return;
            }

            if (dims.is3D()) {
                auto sv = MValue::matrix(1, 3, MType::DOUBLE, alloc);
                double *d = sv.doubleDataMut();
                d[0] = static_cast<double>(dims.rows());
                d[1] = static_cast<double>(dims.cols());
                d[2] = static_cast<double>(dims.pages());
                outs[0] = std::move(sv);
            } else {
                auto sv = MValue::matrix(1, 2, MType::DOUBLE, alloc);
                double *d = sv.doubleDataMut();
                d[0] = static_cast<double>(dims.rows());
                d[1] = static_cast<double>(dims.cols());
                outs[0] = std::move(sv);
            }
            return;
        });

    // --- length ---
    engine.registerFunction("length",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                if (a.isEmpty() || a.numel() == 0) {
                                    outs[0] = MValue::scalar(0.0, alloc);
                                    return;
                                }
                                auto &dims = a.dims();
                                double len = static_cast<double>(
                                    std::max({dims.rows(), dims.cols(), dims.pages()}));
                                outs[0] = MValue::scalar(len, alloc);
                                return;
                            });

    // --- numel ---
    engine.registerFunction("numel",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                {
                                    outs[0] = MValue::scalar(static_cast<double>(args[0].numel()),
                                                             alloc);
                                    return;
                                }
                            });

    // --- ndims ---
    engine.registerFunction(
        "ndims", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            {
                outs[0] = MValue::scalar(static_cast<double>(args[0].dims().ndims()), alloc);
                return;
            }
        });

    // --- reshape ---
    // reshape(A, m, n) | reshape(A, m, n, p) | reshape(A, [m n]) |
    // reshape(A, [m n p]) | reshape(A, m, [], ...) — one [] placeholder
    // in the scalar-args form is inferred from numel(A). Data layout
    // is column-major and contiguous, so a flat memcpy preserves the
    // logical order across any rank.
    engine.registerFunction("reshape",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "reshape requires at least 2 arguments");
                                auto &a = args[0];
                                DimsArg d;
                                // Dims-vector form does not support [] inference.
                                if (args.size() == 2 && !args[1].isScalar()
                                    && !args[1].isEmpty()) {
                                    d = parseDimsArgs(args.subspan(1));
                                } else {
                                    // Scalar-args form. One [] placeholder is
                                    // inferred from numel(A).
                                    size_t dimCount = args.size() - 1;
                                    size_t dims[3] = {1, 1, 0};
                                    int inferPos = -1;
                                    size_t knownProd = 1;
                                    for (size_t i = 0; i < dimCount && i < 3; ++i) {
                                        if (args[i + 1].isEmpty()) {
                                            if (inferPos >= 0)
                                                throw std::runtime_error(
                                                    "reshape: only one dimension "
                                                    "may be inferred via []");
                                            inferPos = static_cast<int>(i);
                                        } else {
                                            dims[i] = static_cast<size_t>(
                                                args[i + 1].toScalar());
                                            knownProd *= dims[i];
                                        }
                                    }
                                    if (inferPos >= 0) {
                                        if (knownProd == 0
                                            || a.numel() % knownProd != 0)
                                            throw std::runtime_error(
                                                "reshape: size of array must be "
                                                "divisible by product of known dims");
                                        dims[inferPos] = a.numel() / knownProd;
                                    }
                                    d = {dims[0], dims[1],
                                         dimCount >= 3 ? dims[2] : 0};
                                }
                                size_t newNumel = d.rows * d.cols *
                                                  (d.pages > 0 ? d.pages : 1);
                                if (newNumel != a.numel())
                                    throw std::runtime_error(
                                        "Number of elements must not change in reshape");
                                // CELL and STRING store their elements in
                                // cellData (a vector<MValue>), not in the
                                // raw buffer — memcpy wouldn't copy them.
                                if (a.type() == MType::CELL ||
                                    a.type() == MType::STRING) {
                                    const bool is3D = d.pages > 0;
                                    MValue r = (a.type() == MType::CELL)
                                        ? (is3D ? MValue::cell3D(d.rows, d.cols, d.pages)
                                                : MValue::cell(d.rows, d.cols))
                                        : (is3D ? MValue::stringArray3D(d.rows, d.cols, d.pages)
                                                : MValue::stringArray(d.rows, d.cols));
                                    auto &src = a.cellDataVec();
                                    auto &dst = r.cellDataVec();
                                    for (size_t i = 0; i < src.size() && i < dst.size(); ++i)
                                        dst[i] = src[i];
                                    outs[0] = r;
                                    return;
                                }
                                auto r = createMatrix(d, a.type(), alloc);
                                if (a.rawBytes() > 0)
                                    std::memcpy(r.rawDataMut(), a.rawData(), a.rawBytes());
                                outs[0] = r;
                            });

    // --- transpose (function form) ---
    engine.registerFunction("transpose",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                size_t rows = a.dims().rows(), cols = a.dims().cols();
                                auto r = MValue::matrix(cols, rows, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < rows; ++i)
                                    for (size_t j = 0; j < cols; ++j)
                                        r.elem(j, i) = a(i, j);
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- diag ---
    engine.registerFunction("diag",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                if (a.dims().isVector()) {
                                    size_t n = a.numel();
                                    auto r = MValue::matrix(n, n, MType::DOUBLE, alloc);
                                    for (size_t i = 0; i < n; ++i)
                                        r.elem(i, i) = a.doubleData()[i];
                                    {
                                        outs[0] = r;
                                        return;
                                    }
                                }
                                size_t n = std::min(a.dims().rows(), a.dims().cols());
                                auto r = MValue::matrix(n, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < n; ++i)
                                    r.doubleDataMut()[i] = a(i, i);
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- sort ---
    engine.registerFunction(
        "sort", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            std::vector<double> vals(a.doubleData(), a.doubleData() + a.numel());
            std::sort(vals.begin(), vals.end());
            bool isRow = a.dims().rows() == 1;
            auto r = isRow ? MValue::matrix(1, vals.size(), MType::DOUBLE, alloc)
                           : MValue::matrix(vals.size(), 1, MType::DOUBLE, alloc);
            std::memcpy(r.doubleDataMut(), vals.data(), vals.size() * sizeof(double));
            {
                outs[0] = r;
                return;
            }
        });

    // --- find ---
    engine.registerFunction("find",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                std::vector<double> indices;
                                if (a.isLogical()) {
                                    const uint8_t *ld = a.logicalData();
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        if (ld[i])
                                            indices.push_back(static_cast<double>(i + 1));
                                } else {
                                    const double *dd = a.doubleData();
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        if (dd[i] != 0.0)
                                            indices.push_back(static_cast<double>(i + 1));
                                }
                                auto r = MValue::matrix(1, indices.size(), MType::DOUBLE, alloc);
                                if (!indices.empty())
                                    std::memcpy(r.doubleDataMut(),
                                                indices.data(),
                                                indices.size() * sizeof(double));
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- horzcat ---
    engine.registerFunction("horzcat",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                if (args.empty()) {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                                outs[0] = MValue::horzcat(
                                    args.data(), args.size(), &ctx.engine->allocator());
                            });

    // --- vertcat ---
    engine.registerFunction("vertcat",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                if (args.empty()) {
                                    outs[0] = MValue::empty();
                                    return;
                                }
                                outs[0] = MValue::vertcat(
                                    args.data(), args.size(), &ctx.engine->allocator());
                            });

    // --- meshgrid ---
    engine.registerFunction("meshgrid",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("meshgrid requires 2 arguments");
                                auto &xv = args[0];
                                auto &yv = args[1];
                                size_t nx = xv.numel(), ny = yv.numel();
                                auto X = MValue::matrix(ny, nx, MType::DOUBLE, alloc);
                                auto Y = MValue::matrix(ny, nx, MType::DOUBLE, alloc);
                                for (size_t r = 0; r < ny; ++r)
                                    for (size_t c = 0; c < nx; ++c) {
                                        X.elem(r, c) = xv.doubleData()[c];
                                        Y.elem(r, c) = yv.doubleData()[r];
                                    }
                                {
                                    outs[0] = X;
                                    if (nargout > 1)
                                        outs[1] = Y;
                                    return;
                                }
                            });

    // --- cumsum ---
    engine.registerFunction(
        "cumsum", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.dims().isVector() || a.isScalar()) {
                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
                double s = 0;
                for (size_t i = 0; i < a.numel(); ++i) {
                    s += a.doubleData()[i];
                    r.doubleDataMut()[i] = s;
                }
                {
                    outs[0] = r;
                    return;
                }
            }
            size_t R = a.dims().rows(), C = a.dims().cols();
            auto r = MValue::matrix(R, C, MType::DOUBLE, alloc);
            for (size_t c = 0; c < C; ++c) {
                double s = 0;
                for (size_t rr = 0; rr < R; ++rr) {
                    s += a(rr, c);
                    r.elem(rr, c) = s;
                }
            }
            {
                outs[0] = r;
                return;
            }
        });

    // --- cross ---
    engine.registerFunction("cross",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("cross requires 2 arguments");
                                auto &a = args[0];
                                auto &b = args[1];
                                if (a.numel() != 3 || b.numel() != 3)
                                    throw std::runtime_error("cross requires 3-element vectors");
                                auto r = MValue::matrix(1, 3, MType::DOUBLE, alloc);
                                r.doubleDataMut()[0] = a.doubleData()[1] * b.doubleData()[2]
                                                       - a.doubleData()[2] * b.doubleData()[1];
                                r.doubleDataMut()[1] = a.doubleData()[2] * b.doubleData()[0]
                                                       - a.doubleData()[0] * b.doubleData()[2];
                                r.doubleDataMut()[2] = a.doubleData()[0] * b.doubleData()[1]
                                                       - a.doubleData()[1] * b.doubleData()[0];
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- dot ---
    engine.registerFunction("dot",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("dot requires 2 arguments");
                                auto &a = args[0];
                                auto &b = args[1];
                                if (a.numel() != b.numel())
                                    throw std::runtime_error("dot: vectors must have same length");
                                double s = 0;
                                for (size_t i = 0; i < a.numel(); ++i)
                                    s += a.doubleData()[i] * b.doubleData()[i];
                                {
                                    outs[0] = MValue::scalar(s, alloc);
                                    return;
                                }
                            });
}

} // namespace mlab