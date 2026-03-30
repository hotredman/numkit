#include "MLabStdLibrary.hpp"
#include "MLabStdHelpers.hpp"

#include <algorithm>
#include <cstring>

namespace mlab {

void StdLibrary::registerMatrixFunctions(Engine &engine)
{
    // --- zeros ---
    engine.registerFunction("zeros",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                size_t r = static_cast<size_t>(args[0].toScalar());
                                size_t c = args.size() >= 2 ? static_cast<size_t>(args[1].toScalar()) : r;
                                return {MValue::matrix(r, c, MType::DOUBLE, alloc)};
                            });

    // --- ones ---
    engine.registerFunction("ones",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                size_t r = static_cast<size_t>(args[0].toScalar());
                                size_t c = args.size() >= 2 ? static_cast<size_t>(args[1].toScalar()) : r;
                                auto m = MValue::matrix(r, c, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < m.numel(); ++i)
                                    m.doubleDataMut()[i] = 1.0;
                                return {m};
                            });

    // --- eye ---
    engine.registerFunction("eye", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        size_t r = static_cast<size_t>(args[0].toScalar());
        size_t c = args.size() >= 2 ? static_cast<size_t>(args[1].toScalar()) : r;
        auto m = MValue::matrix(r, c, MType::DOUBLE, alloc);
        for (size_t i = 0; i < std::min(r, c); ++i)
            m.elem(i, i) = 1.0;
        return {m};
    });

    // --- size ---
    engine.registerFunction("size", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        if (args.size() >= 2) {
            int dim = static_cast<int>(args[1].toScalar());
            return {MValue::scalar(static_cast<double>(a.dims().dimSize(dim - 1)), alloc)};
        }
        return {MValue::scalar(static_cast<double>(a.dims().rows()), alloc),
                MValue::scalar(static_cast<double>(a.dims().cols()), alloc)};
    });

    // --- length ---
    engine.registerFunction("length",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                double len = static_cast<double>(
                                    std::max(args[0].dims().rows(), args[0].dims().cols()));
                                return {MValue::scalar(len, alloc)};
                            });

    // --- numel ---
    engine.registerFunction("numel",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::scalar(static_cast<double>(args[0].numel()), alloc)};
                            });

    // --- ndims ---
    engine.registerFunction("ndims",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                return {MValue::scalar(static_cast<double>(args[0].dims().ndims()), alloc)};
                            });

    // --- reshape ---
    engine.registerFunction("reshape",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                size_t newR = static_cast<size_t>(args[1].toScalar());
                                size_t newC = static_cast<size_t>(args[2].toScalar());
                                if (newR * newC != a.numel())
                                    throw std::runtime_error("Number of elements must not change in reshape");
                                auto r = MValue::matrix(newR, newC, a.type(), alloc);
                                if (a.rawBytes() > 0)
                                    std::memcpy(r.rawDataMut(), a.rawData(), a.rawBytes());
                                return {r};
                            });

    // --- transpose (function form) ---
    engine.registerFunction("transpose",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                size_t rows = a.dims().rows(), cols = a.dims().cols();
                                auto r = MValue::matrix(cols, rows, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < rows; ++i)
                                    for (size_t j = 0; j < cols; ++j)
                                        r.elem(j, i) = a(i, j);
                                return {r};
                            });

    // --- diag ---
    engine.registerFunction("diag",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.dims().isVector()) {
                                    size_t n = a.numel();
                                    auto r = MValue::matrix(n, n, MType::DOUBLE, alloc);
                                    for (size_t i = 0; i < n; ++i)
                                        r.elem(i, i) = a.doubleData()[i];
                                    return {r};
                                }
                                size_t n = std::min(a.dims().rows(), a.dims().cols());
                                auto r = MValue::matrix(n, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < n; ++i)
                                    r.doubleDataMut()[i] = a(i, i);
                                return {r};
                            });

    // --- sort ---
    engine.registerFunction("sort", [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
        auto *alloc = &engine.allocator();
        auto &a = args[0];
        std::vector<double> vals(a.doubleData(), a.doubleData() + a.numel());
        std::sort(vals.begin(), vals.end());
        bool isRow = a.dims().rows() == 1;
        auto r = isRow ? MValue::matrix(1, vals.size(), MType::DOUBLE, alloc)
                       : MValue::matrix(vals.size(), 1, MType::DOUBLE, alloc);
        std::memcpy(r.doubleDataMut(), vals.data(), vals.size() * sizeof(double));
        return {r};
    });

    // --- find ---
    engine.registerFunction("find",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                std::vector<double> indices;
                                if (a.isLogical()) {
                                    const uint8_t *ld = a.logicalData();
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        if (ld[i]) indices.push_back(static_cast<double>(i + 1));
                                } else {
                                    const double *dd = a.doubleData();
                                    for (size_t i = 0; i < a.numel(); ++i)
                                        if (dd[i] != 0.0) indices.push_back(static_cast<double>(i + 1));
                                }
                                auto r = MValue::matrix(1, indices.size(), MType::DOUBLE, alloc);
                                if (!indices.empty())
                                    std::memcpy(r.doubleDataMut(), indices.data(), indices.size() * sizeof(double));
                                return {r};
                            });

    // --- horzcat ---
    engine.registerFunction("horzcat",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty()) return {MValue::empty()};
                                size_t rows = args[0].dims().rows();
                                size_t totalCols = 0;
                                for (auto &a : args) {
                                    if (a.dims().rows() != rows)
                                        throw std::runtime_error("Dimensions must agree for horzcat");
                                    totalCols += a.dims().cols();
                                }
                                auto r = MValue::matrix(rows, totalCols, MType::DOUBLE, alloc);
                                size_t colOff = 0;
                                for (auto &a : args) {
                                    for (size_t c = 0; c < a.dims().cols(); ++c)
                                        for (size_t rr = 0; rr < rows; ++rr)
                                            r.elem(rr, colOff + c) = a(rr, c);
                                    colOff += a.dims().cols();
                                }
                                return {r};
                            });

    // --- vertcat ---
    engine.registerFunction("vertcat",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty()) return {MValue::empty()};
                                size_t cols = args[0].dims().cols();
                                size_t totalRows = 0;
                                for (auto &a : args) {
                                    if (a.dims().cols() != cols)
                                        throw std::runtime_error("Dimensions must agree for vertcat");
                                    totalRows += a.dims().rows();
                                }
                                auto r = MValue::matrix(totalRows, cols, MType::DOUBLE, alloc);
                                size_t rowOff = 0;
                                for (auto &a : args) {
                                    for (size_t c = 0; c < cols; ++c)
                                        for (size_t rr = 0; rr < a.dims().rows(); ++rr)
                                            r.elem(rowOff + rr, c) = a(rr, c);
                                    rowOff += a.dims().rows();
                                }
                                return {r};
                            });

    // --- meshgrid ---
    engine.registerFunction("meshgrid",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
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
                                return {X, Y};
                            });

    // --- cumsum ---
    engine.registerFunction("cumsum",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.dims().isVector() || a.isScalar()) {
                                    auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::DOUBLE, alloc);
                                    double s = 0;
                                    for (size_t i = 0; i < a.numel(); ++i) {
                                        s += a.doubleData()[i];
                                        r.doubleDataMut()[i] = s;
                                    }
                                    return {r};
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
                                return {r};
                            });

    // --- cross ---
    engine.registerFunction("cross",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 2) throw std::runtime_error("cross requires 2 arguments");
                                auto &a = args[0]; auto &b = args[1];
                                if (a.numel() != 3 || b.numel() != 3)
                                    throw std::runtime_error("cross requires 3-element vectors");
                                auto r = MValue::matrix(1, 3, MType::DOUBLE, alloc);
                                r.doubleDataMut()[0] = a.doubleData()[1] * b.doubleData()[2] - a.doubleData()[2] * b.doubleData()[1];
                                r.doubleDataMut()[1] = a.doubleData()[2] * b.doubleData()[0] - a.doubleData()[0] * b.doubleData()[2];
                                r.doubleDataMut()[2] = a.doubleData()[0] * b.doubleData()[1] - a.doubleData()[1] * b.doubleData()[0];
                                return {r};
                            });

    // --- dot ---
    engine.registerFunction("dot",
                            [&engine](const std::vector<MValue> &args, size_t /*nargout*/) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 2) throw std::runtime_error("dot requires 2 arguments");
                                auto &a = args[0]; auto &b = args[1];
                                if (a.numel() != b.numel())
                                    throw std::runtime_error("dot: vectors must have same length");
                                double s = 0;
                                for (size_t i = 0; i < a.numel(); ++i)
                                    s += a.doubleData()[i] * b.doubleData()[i];
                                return {MValue::scalar(s, alloc)};
                            });
}

} // namespace mlab
