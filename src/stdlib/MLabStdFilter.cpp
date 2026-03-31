#include "MLabStdLibrary.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace mlab {

void StdLibrary::registerFilterFunctions(Engine &engine)
{
    // --- filter(b, a, x) --- Direct Form II transposed
    engine.registerFunction("filter",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 3)
                                    throw std::runtime_error("filter requires 3 arguments");
                                auto &bv = args[0];
                                auto &av = args[1];
                                auto &xv = args[2];
                                size_t nb = bv.numel(), na = av.numel(), nx = xv.numel();
                                const double *b = bv.doubleData();
                                const double *a = av.doubleData();
                                const double *x = xv.doubleData();

                                double a0 = a[0];
                                if (a0 == 0.0)
                                    throw std::runtime_error("filter: a(1) must be nonzero");

                                std::vector<double> bn(nb), an(na);
                                for (size_t i = 0; i < nb; ++i) bn[i] = b[i] / a0;
                                for (size_t i = 0; i < na; ++i) an[i] = a[i] / a0;

                                size_t nfilt = std::max(nb, na);
                                std::vector<double> z(nfilt, 0.0);

                                auto r = MValue::matrix(xv.dims().rows(), xv.dims().cols(), MType::DOUBLE, alloc);
                                double *y = r.doubleDataMut();

                                for (size_t n = 0; n < nx; ++n) {
                                    y[n] = (nb > 0 ? bn[0] : 0.0) * x[n] + z[0];
                                    for (size_t i = 1; i < nfilt; ++i) {
                                        z[i - 1] = (i < nb ? bn[i] : 0.0) * x[n]
                                                  - (i < na ? an[i] : 0.0) * y[n]
                                                  + (i < nfilt - 1 ? z[i] : 0.0);
                                    }
                                }
                                { outs[0] = r; return; }
                            });

    // --- filtfilt(b, a, x) --- Zero-phase filtering (forward-backward)
    engine.registerFunction("filtfilt",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 3)
                                    throw std::runtime_error("filtfilt requires 3 arguments");
                                auto &bv = args[0];
                                auto &av = args[1];
                                auto &xv = args[2];
                                size_t nb = bv.numel(), na = av.numel(), nx = xv.numel();
                                const double *b = bv.doubleData();
                                const double *a = av.doubleData();
                                const double *x = xv.doubleData();

                                double a0 = a[0];
                                if (a0 == 0.0)
                                    throw std::runtime_error("filtfilt: a(1) must be nonzero");

                                std::vector<double> bn(nb), an(na);
                                for (size_t i = 0; i < nb; ++i) bn[i] = b[i] / a0;
                                for (size_t i = 0; i < na; ++i) an[i] = a[i] / a0;

                                size_t nfilt = std::max(nb, na);

                                auto applyFilter = [&](const std::vector<double> &input) -> std::vector<double> {
                                    size_t len = input.size();
                                    std::vector<double> out(len);
                                    std::vector<double> z(nfilt, 0.0);
                                    for (size_t n = 0; n < len; ++n) {
                                        out[n] = (nb > 0 ? bn[0] : 0.0) * input[n] + z[0];
                                        for (size_t i = 1; i < nfilt; ++i) {
                                            z[i - 1] = (i < nb ? bn[i] : 0.0) * input[n]
                                                      - (i < na ? an[i] : 0.0) * out[n]
                                                      + (i < nfilt - 1 ? z[i] : 0.0);
                                        }
                                    }
                                    return out;
                                };

                                // Edge extension (3x filter order)
                                size_t nEdge = 3 * nfilt;
                                if (nEdge >= nx) nEdge = nx - 1;

                                // Reflect edges to reduce transients
                                size_t extLen = nx + 2 * nEdge;
                                std::vector<double> ext(extLen);
                                for (size_t i = 0; i < nEdge; ++i)
                                    ext[i] = 2.0 * x[0] - x[nEdge - i];
                                for (size_t i = 0; i < nx; ++i)
                                    ext[nEdge + i] = x[i];
                                for (size_t i = 0; i < nEdge; ++i)
                                    ext[nEdge + nx + i] = 2.0 * x[nx - 1] - x[nx - 2 - i];

                                // Forward → reverse → backward → reverse → extract
                                auto fwd = applyFilter(ext);
                                std::reverse(fwd.begin(), fwd.end());
                                auto bwd = applyFilter(fwd);
                                std::reverse(bwd.begin(), bwd.end());

                                auto r = MValue::matrix(xv.dims().rows(), xv.dims().cols(), MType::DOUBLE, alloc);
                                for (size_t i = 0; i < nx; ++i)
                                    r.doubleDataMut()[i] = bwd[nEdge + i];
                                { outs[0] = r; return; }
                            });
}

} // namespace mlab
