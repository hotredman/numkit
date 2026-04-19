#include "MStdHelpers.hpp"
#include "MFitLibrary.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace numkit::m {

static size_t findInterval(const double *xData, size_t n, double xq)
{
    if (xq <= xData[0])
        return 0;
    if (xq >= xData[n - 1])
        return n - 2;
    auto it = std::upper_bound(xData, xData + n, xq);
    size_t idx = static_cast<size_t>(it - xData);
    if (idx == 0)
        return 0;
    return idx - 1;
}

static std::vector<double> interpLinear(
    const double *x, const double *y, size_t n, const double *xq, size_t nq)
{
    std::vector<double> yq(nq);
    for (size_t k = 0; k < nq; ++k) {
        size_t i = findInterval(x, n, xq[k]);
        double dx = x[i + 1] - x[i];
        if (dx == 0.0) {
            yq[k] = y[i];
        } else {
            double t = (xq[k] - x[i]) / dx;
            yq[k] = y[i] + t * (y[i + 1] - y[i]);
        }
    }
    return yq;
}

static std::vector<double> interpNearest(
    const double *x, const double *y, size_t n, const double *xq, size_t nq)
{
    std::vector<double> yq(nq);
    for (size_t k = 0; k < nq; ++k) {
        size_t i = findInterval(x, n, xq[k]);
        if (std::abs(xq[k] - x[i]) <= std::abs(xq[k] - x[i + 1]))
            yq[k] = y[i];
        else
            yq[k] = y[i + 1];
    }
    return yq;
}

static std::vector<double> interpSpline(
    const double *x, const double *y, size_t n, const double *xq, size_t nq)
{
    if (n < 3)
        return interpLinear(x, y, n, xq, nq);

    size_t nm1 = n - 1;

    std::vector<double> h(nm1);
    for (size_t i = 0; i < nm1; ++i)
        h[i] = x[i + 1] - x[i];

    size_t m = n - 2;
    std::vector<double> sigma(n, 0.0);

    if (m > 0) {
        std::vector<double> diag(m), upper(m), lower(m), rhs(m);

        for (size_t i = 0; i < m; ++i) {
            size_t j = i + 1;
            diag[i] = 2.0 * (h[j - 1] + h[j]);
            rhs[i] = 6.0 * ((y[j + 1] - y[j]) / h[j] - (y[j] - y[j - 1]) / h[j - 1]);
            if (i > 0)
                lower[i] = h[j - 1];
            if (i < m - 1)
                upper[i] = h[j];
        }

        for (size_t i = 1; i < m; ++i) {
            double w = lower[i] / diag[i - 1];
            diag[i] -= w * upper[i - 1];
            rhs[i] -= w * rhs[i - 1];
        }

        sigma[m] = rhs[m - 1] / diag[m - 1];
        for (int i = static_cast<int>(m) - 2; i >= 0; --i) {
            sigma[i + 1] = (rhs[i] - upper[i] * sigma[i + 2]) / diag[i];
        }
    }

    std::vector<double> yq(nq);
    for (size_t k = 0; k < nq; ++k) {
        size_t i = findInterval(x, n, xq[k]);
        double dx = xq[k] - x[i];
        double dx1 = x[i + 1] - xq[k];
        double hi = h[i];

        yq[k] = sigma[i] * dx1 * dx1 * dx1 / (6.0 * hi) + sigma[i + 1] * dx * dx * dx / (6.0 * hi)
                + (y[i] / hi - sigma[i] * hi / 6.0) * dx1
                + (y[i + 1] / hi - sigma[i + 1] * hi / 6.0) * dx;
    }
    return yq;
}

static std::vector<double> interpPchip(
    const double *x, const double *y, size_t n, const double *xq, size_t nq)
{
    if (n < 3)
        return interpLinear(x, y, n, xq, nq);

    size_t nm1 = n - 1;

    std::vector<double> h(nm1), delta(nm1);
    for (size_t i = 0; i < nm1; ++i) {
        h[i] = x[i + 1] - x[i];
        delta[i] = (y[i + 1] - y[i]) / h[i];
    }

    std::vector<double> d(n, 0.0);

    for (size_t i = 1; i < nm1; ++i) {
        if (delta[i - 1] * delta[i] <= 0.0) {
            d[i] = 0.0;
        } else {
            double w1 = 2.0 * h[i] + h[i - 1];
            double w2 = h[i] + 2.0 * h[i - 1];
            d[i] = (w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i]);
        }
    }

    d[0] = ((2.0 * h[0] + h[1]) * delta[0] - h[0] * delta[1]) / (h[0] + h[1]);
    if (d[0] * delta[0] < 0.0)
        d[0] = 0.0;
    else if (delta[0] * delta[1] < 0.0 && std::abs(d[0]) > std::abs(3.0 * delta[0]))
        d[0] = 3.0 * delta[0];

    d[nm1] = ((2.0 * h[nm1 - 1] + h[nm1 - 2]) * delta[nm1 - 1] - h[nm1 - 1] * delta[nm1 - 2])
             / (h[nm1 - 1] + h[nm1 - 2]);
    if (d[nm1] * delta[nm1 - 1] < 0.0)
        d[nm1] = 0.0;
    else if (delta[nm1 - 2] * delta[nm1 - 1] < 0.0
             && std::abs(d[nm1]) > std::abs(3.0 * delta[nm1 - 1]))
        d[nm1] = 3.0 * delta[nm1 - 1];

    std::vector<double> yq(nq);
    for (size_t k = 0; k < nq; ++k) {
        size_t i = findInterval(x, n, xq[k]);
        double t = (xq[k] - x[i]) / h[i];
        double t2 = t * t;
        double t3 = t2 * t;

        double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        double h10 = t3 - 2.0 * t2 + t;
        double h01 = -2.0 * t3 + 3.0 * t2;
        double h11 = t3 - t2;

        yq[k] = h00 * y[i] + h10 * h[i] * d[i] + h01 * y[i + 1] + h11 * h[i] * d[i + 1];
    }
    return yq;
}

void FitLibrary::registerInterpFunctions(Engine &engine)
{
    // --- interp1(x, y, xq) / interp1(x, y, xq, method) ---
    engine.registerFunction(
        "interp1", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 3)
                throw std::runtime_error("interp1 requires at least 3 arguments");

            auto &xv = args[0];
            auto &yv = args[1];
            auto &xqv = args[2];
            std::string method = "linear";
            if (args.size() >= 4 && args[3].isChar())
                method = args[3].toString();

            size_t n = xv.numel();
            size_t nq = xqv.numel();

            if (n != yv.numel())
                throw std::runtime_error("interp1: x and y must have same length");
            if (n < 2)
                throw std::runtime_error("interp1: need at least 2 data points");

            const double *x = xv.doubleData();
            const double *y = yv.doubleData();
            const double *xq = xqv.doubleData();

            std::vector<double> yq;
            if (method == "linear")
                yq = interpLinear(x, y, n, xq, nq);
            else if (method == "nearest")
                yq = interpNearest(x, y, n, xq, nq);
            else if (method == "spline")
                yq = interpSpline(x, y, n, xq, nq);
            else if (method == "pchip")
                yq = interpPchip(x, y, n, xq, nq);
            else
                throw std::runtime_error("interp1: unknown method '" + method + "'");

            bool isRow = xqv.dims().rows() == 1;
            auto r = isRow ? MValue::matrix(1, nq, MType::DOUBLE, alloc)
                           : MValue::matrix(nq, 1, MType::DOUBLE, alloc);
            for (size_t i = 0; i < nq; ++i)
                r.doubleDataMut()[i] = yq[i];
            {
                outs[0] = r;
                return;
            }
        });

    // --- spline(x, y, xq) ---
    engine.registerFunction(
        "spline", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 3)
                throw std::runtime_error("spline requires 3 arguments");

            auto &xv = args[0];
            auto &yv = args[1];
            auto &xqv = args[2];
            size_t n = xv.numel();
            size_t nq = xqv.numel();

            if (n != yv.numel())
                throw std::runtime_error("spline: x and y must have same length");
            if (n < 2)
                throw std::runtime_error("spline: need at least 2 data points");

            auto yq = interpSpline(xv.doubleData(), yv.doubleData(), n, xqv.doubleData(), nq);

            bool isRow = xqv.dims().rows() == 1;
            auto r = isRow ? MValue::matrix(1, nq, MType::DOUBLE, alloc)
                           : MValue::matrix(nq, 1, MType::DOUBLE, alloc);
            for (size_t i = 0; i < nq; ++i)
                r.doubleDataMut()[i] = yq[i];
            {
                outs[0] = r;
                return;
            }
        });

    // --- pchip(x, y, xq) ---
    engine.registerFunction(
        "pchip", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            if (args.size() < 3)
                throw std::runtime_error("pchip requires 3 arguments");

            auto &xv = args[0];
            auto &yv = args[1];
            auto &xqv = args[2];
            size_t n = xv.numel();
            size_t nq = xqv.numel();

            if (n != yv.numel())
                throw std::runtime_error("pchip: x and y must have same length");
            if (n < 2)
                throw std::runtime_error("pchip: need at least 2 data points");

            auto yq = interpPchip(xv.doubleData(), yv.doubleData(), n, xqv.doubleData(), nq);

            bool isRow = xqv.dims().rows() == 1;
            auto r = isRow ? MValue::matrix(1, nq, MType::DOUBLE, alloc)
                           : MValue::matrix(nq, 1, MType::DOUBLE, alloc);
            for (size_t i = 0; i < nq; ++i)
                r.doubleDataMut()[i] = yq[i];
            {
                outs[0] = r;
                return;
            }
        });

    // --- polyfit(x, y, n) ---
    engine.registerFunction("polyfit",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 3)
                                    throw std::runtime_error("polyfit requires 3 arguments");

                                const double *x = args[0].doubleData();
                                const double *y = args[1].doubleData();
                                size_t m = args[0].numel();
                                int deg = static_cast<int>(args[2].toScalar());
                                int np = deg + 1;

                                if (static_cast<size_t>(np) > m)
                                    throw std::runtime_error("polyfit: not enough data points");

                                std::vector<double> A(m * np);
                                for (size_t i = 0; i < m; ++i) {
                                    for (int j = 0; j < np; ++j) {
                                        A[j * m + i] = std::pow(x[i], deg - j);
                                    }
                                }

                                std::vector<double> ATA(np * np, 0.0);
                                for (int r = 0; r < np; ++r)
                                    for (int c = 0; c < np; ++c)
                                        for (size_t i = 0; i < m; ++i)
                                            ATA[c * np + r] += A[r * m + i] * A[c * m + i];

                                std::vector<double> ATy(np, 0.0);
                                for (int r = 0; r < np; ++r)
                                    for (size_t i = 0; i < m; ++i)
                                        ATy[r] += A[r * m + i] * y[i];

                                std::vector<double> aug(np * (np + 1));
                                for (int r = 0; r < np; ++r) {
                                    for (int c = 0; c < np; ++c)
                                        aug[r * (np + 1) + c] = ATA[c * np + r];
                                    aug[r * (np + 1) + np] = ATy[r];
                                }

                                for (int k = 0; k < np; ++k) {
                                    int maxRow = k;
                                    double maxVal = std::abs(aug[k * (np + 1) + k]);
                                    for (int r = k + 1; r < np; ++r) {
                                        double v = std::abs(aug[r * (np + 1) + k]);
                                        if (v > maxVal) {
                                            maxVal = v;
                                            maxRow = r;
                                        }
                                    }
                                    if (maxRow != k) {
                                        for (int c = 0; c <= np; ++c)
                                            std::swap(aug[k * (np + 1) + c],
                                                      aug[maxRow * (np + 1) + c]);
                                    }

                                    double pivot = aug[k * (np + 1) + k];
                                    if (std::abs(pivot) < 1e-15)
                                        throw std::runtime_error("polyfit: singular matrix");

                                    for (int c = k; c <= np; ++c)
                                        aug[k * (np + 1) + c] /= pivot;
                                    for (int r = 0; r < np; ++r) {
                                        if (r == k)
                                            continue;
                                        double f = aug[r * (np + 1) + k];
                                        for (int c = k; c <= np; ++c)
                                            aug[r * (np + 1) + c] -= f * aug[k * (np + 1) + c];
                                    }
                                }

                                auto p = MValue::matrix(1, np, MType::DOUBLE, alloc);
                                for (int j = 0; j < np; ++j)
                                    p.doubleDataMut()[j] = aug[j * (np + 1) + np];
                                {
                                    outs[0] = p;
                                    return;
                                }
                            });

    // --- polyval(p, x) ---
    engine.registerFunction("polyval",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("polyval requires 2 arguments");

                                const double *p = args[0].doubleData();
                                size_t np = args[0].numel();
                                auto &xv = args[1];
                                size_t nx = xv.numel();
                                const double *x = xv.doubleData();

                                auto r = createLike(xv, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < nx; ++i) {
                                    double val = p[0];
                                    for (size_t j = 1; j < np; ++j)
                                        val = val * x[i] + p[j];
                                    r.doubleDataMut()[i] = val;
                                }
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    // --- trapz(y) / trapz(x, y) ---
    engine.registerFunction("trapz",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.empty())
                                    throw std::runtime_error("trapz requires at least 1 argument");

                                const double *x = nullptr;
                                const double *y = nullptr;
                                size_t n = 0;

                                if (args.size() == 1) {
                                    y = args[0].doubleData();
                                    n = args[0].numel();
                                    double s = 0.0;
                                    for (size_t i = 1; i < n; ++i)
                                        s += 0.5 * (y[i - 1] + y[i]);
                                    {
                                        outs[0] = MValue::scalar(s, alloc);
                                        return;
                                    }
                                }

                                x = args[0].doubleData();
                                y = args[1].doubleData();
                                n = args[0].numel();
                                if (args[1].numel() != n)
                                    throw std::runtime_error(
                                        "trapz: x and y must have same length");

                                double s = 0.0;
                                for (size_t i = 1; i < n; ++i)
                                    s += 0.5 * (y[i - 1] + y[i]) * (x[i] - x[i - 1]);
                                {
                                    outs[0] = MValue::scalar(s, alloc);
                                    return;
                                }
                            });
}

} // namespace numkit::m