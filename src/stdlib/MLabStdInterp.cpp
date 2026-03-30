#include "MLabStdLibrary.hpp"
#include "MLabStdHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace mlab {

// ============================================================
// Internal: binary search — find index i such that xData[i] <= xq < xData[i+1]
// Returns clamped index in [0, n-2]
// ============================================================
static size_t findInterval(const double *xData, size_t n, double xq)
{
    if (xq <= xData[0])
        return 0;
    if (xq >= xData[n - 1])
        return n - 2;
    // std::upper_bound returns iterator to first element > xq
    auto it = std::upper_bound(xData, xData + n, xq);
    size_t idx = static_cast<size_t>(it - xData);
    if (idx == 0)
        return 0;
    return idx - 1;
}

// ============================================================
// Internal: linear interpolation at query points
// ============================================================
static std::vector<double> interpLinear(const double *x, const double *y, size_t n,
                                        const double *xq, size_t nq)
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

// ============================================================
// Internal: nearest-neighbor interpolation
// ============================================================
static std::vector<double> interpNearest(const double *x, const double *y, size_t n,
                                         const double *xq, size_t nq)
{
    std::vector<double> yq(nq);
    for (size_t k = 0; k < nq; ++k) {
        size_t i = findInterval(x, n, xq[k]);
        // Pick nearest of x[i] and x[i+1]
        if (std::abs(xq[k] - x[i]) <= std::abs(xq[k] - x[i + 1]))
            yq[k] = y[i];
        else
            yq[k] = y[i + 1];
    }
    return yq;
}

// ============================================================
// Internal: natural cubic spline
// Solves tridiagonal system for second derivatives,
// then evaluates at query points.
// ============================================================
static std::vector<double> interpSpline(const double *x, const double *y, size_t n,
                                        const double *xq, size_t nq)
{
    if (n < 3)
        return interpLinear(x, y, n, xq, nq); // fallback

    size_t nm1 = n - 1;

    // h[i] = x[i+1] - x[i]
    std::vector<double> h(nm1);
    for (size_t i = 0; i < nm1; ++i)
        h[i] = x[i + 1] - x[i];

    // Solve tridiagonal system for sigma (second derivatives / 6)
    // Natural spline: sigma[0] = sigma[n-1] = 0
    // Interior equations:
    //   h[i-1]*sigma[i-1] + 2*(h[i-1]+h[i])*sigma[i] + h[i]*sigma[i+1]
    //     = 6*((y[i+1]-y[i])/h[i] - (y[i]-y[i-1])/h[i-1])
    size_t m = n - 2; // number of interior points
    std::vector<double> sigma(n, 0.0);

    if (m > 0) {
        std::vector<double> diag(m), upper(m), lower(m), rhs(m);

        for (size_t i = 0; i < m; ++i) {
            size_t j = i + 1; // index in original arrays
            diag[i] = 2.0 * (h[j - 1] + h[j]);
            rhs[i] = 6.0 * ((y[j + 1] - y[j]) / h[j] - (y[j] - y[j - 1]) / h[j - 1]);
            if (i > 0)
                lower[i] = h[j - 1];
            if (i < m - 1)
                upper[i] = h[j];
        }

        // Thomas algorithm (forward sweep)
        for (size_t i = 1; i < m; ++i) {
            double w = lower[i] / diag[i - 1];
            diag[i] -= w * upper[i - 1];
            rhs[i] -= w * rhs[i - 1];
        }

        // Back substitution
        sigma[m] = rhs[m - 1] / diag[m - 1]; // sigma[m] = interior index m-1+1
        for (int i = static_cast<int>(m) - 2; i >= 0; --i) {
            sigma[i + 1] = (rhs[i] - upper[i] * sigma[i + 2]) / diag[i];
        }
    }

    // Evaluate spline at query points
    std::vector<double> yq(nq);
    for (size_t k = 0; k < nq; ++k) {
        size_t i = findInterval(x, n, xq[k]);
        double dx = xq[k] - x[i];
        double dx1 = x[i + 1] - xq[k];
        double hi = h[i];

        yq[k] = sigma[i] * dx1 * dx1 * dx1 / (6.0 * hi)
               + sigma[i + 1] * dx * dx * dx / (6.0 * hi)
               + (y[i] / hi - sigma[i] * hi / 6.0) * dx1
               + (y[i + 1] / hi - sigma[i + 1] * hi / 6.0) * dx;
    }
    return yq;
}

// ============================================================
// Internal: PCHIP (Piecewise Cubic Hermite Interpolating Polynomial)
// Monotone-preserving cubic interpolation (Fritsch-Carlson method)
// ============================================================
static std::vector<double> interpPchip(const double *x, const double *y, size_t n,
                                       const double *xq, size_t nq)
{
    if (n < 3)
        return interpLinear(x, y, n, xq, nq);

    size_t nm1 = n - 1;

    // Compute slopes delta[i] = (y[i+1]-y[i])/(x[i+1]-x[i])
    std::vector<double> h(nm1), delta(nm1);
    for (size_t i = 0; i < nm1; ++i) {
        h[i] = x[i + 1] - x[i];
        delta[i] = (y[i + 1] - y[i]) / h[i];
    }

    // Compute derivatives d[i] at each node (Fritsch-Carlson)
    std::vector<double> d(n, 0.0);

    // Interior points
    for (size_t i = 1; i < nm1; ++i) {
        if (delta[i - 1] * delta[i] <= 0.0) {
            // Different signs or zero — set derivative to zero (monotonicity)
            d[i] = 0.0;
        } else {
            // Weighted harmonic mean
            double w1 = 2.0 * h[i] + h[i - 1];
            double w2 = h[i] + 2.0 * h[i - 1];
            d[i] = (w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i]);
        }
    }

    // Endpoints: one-sided shape-preserving formula
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

    // Evaluate cubic Hermite at query points
    std::vector<double> yq(nq);
    for (size_t k = 0; k < nq; ++k) {
        size_t i = findInterval(x, n, xq[k]);
        double t = (xq[k] - x[i]) / h[i];
        double t2 = t * t;
        double t3 = t2 * t;

        // Hermite basis functions
        double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
        double h10 = t3 - 2.0 * t2 + t;
        double h01 = -2.0 * t3 + 3.0 * t2;
        double h11 = t3 - t2;

        yq[k] = h00 * y[i] + h10 * h[i] * d[i] + h01 * y[i + 1] + h11 * h[i] * d[i + 1];
    }
    return yq;
}

// ============================================================
// Register all interpolation functions
// ============================================================
void StdLibrary::registerInterpFunctions(Engine &engine)
{
    // --- interp1(x, y, xq) / interp1(x, y, xq, method) ---
    engine.registerFunction("interp1",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
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
                                return {r};
                            });

    // --- spline(x, y, xq) — shortcut for interp1(..., 'spline') ---
    engine.registerFunction("spline",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
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

                                auto yq = interpSpline(xv.doubleData(), yv.doubleData(), n,
                                                       xqv.doubleData(), nq);

                                bool isRow = xqv.dims().rows() == 1;
                                auto r = isRow ? MValue::matrix(1, nq, MType::DOUBLE, alloc)
                                               : MValue::matrix(nq, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < nq; ++i)
                                    r.doubleDataMut()[i] = yq[i];
                                return {r};
                            });

    // --- pchip(x, y, xq) — shortcut for interp1(..., 'pchip') ---
    engine.registerFunction("pchip",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
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

                                auto yq = interpPchip(xv.doubleData(), yv.doubleData(), n,
                                                      xqv.doubleData(), nq);

                                bool isRow = xqv.dims().rows() == 1;
                                auto r = isRow ? MValue::matrix(1, nq, MType::DOUBLE, alloc)
                                               : MValue::matrix(nq, 1, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < nq; ++i)
                                    r.doubleDataMut()[i] = yq[i];
                                return {r};
                            });

    // --- polyfit(x, y, n) — polynomial least-squares fit ---
    // Uses normal equations: (A'A) p = A'y
    engine.registerFunction("polyfit",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 3)
                                    throw std::runtime_error("polyfit requires 3 arguments");

                                const double *x = args[0].doubleData();
                                const double *y = args[1].doubleData();
                                size_t m = args[0].numel();
                                int deg = static_cast<int>(args[2].toScalar());
                                int np = deg + 1; // number of coefficients

                                if (static_cast<size_t>(np) > m)
                                    throw std::runtime_error("polyfit: not enough data points");

                                // Build Vandermonde matrix A (m x np) in column-major
                                // A[i][j] = x[i]^(deg-j)  (MATLAB convention: highest power first)
                                std::vector<double> A(m * np);
                                for (size_t i = 0; i < m; ++i) {
                                    double xpow = 1.0;
                                    for (int j = deg; j >= 0; --j) {
                                        A[j * m + i] = xpow;
                                        xpow *= x[i];
                                    }
                                }
                                // Now A[j*m+i] = x[i]^(deg-j)
                                // Wait — we want p(1) * x^deg + p(2) * x^(deg-1) + ... + p(deg+1)
                                // So column j should be x^(deg-j)
                                // With the loop above: j=0 → xpow starts at 1 → x^0... that's wrong.
                                // Fix: build powers correctly
                                for (size_t i = 0; i < m; ++i) {
                                    for (int j = 0; j < np; ++j) {
                                        // Column j has x^(deg - j)
                                        A[j * m + i] = std::pow(x[i], deg - j);
                                    }
                                }

                                // Compute A'A (np x np)
                                std::vector<double> ATA(np * np, 0.0);
                                for (int r = 0; r < np; ++r)
                                    for (int c = 0; c < np; ++c)
                                        for (size_t i = 0; i < m; ++i)
                                            ATA[c * np + r] += A[r * m + i] * A[c * m + i];

                                // Compute A'y (np x 1)
                                std::vector<double> ATy(np, 0.0);
                                for (int r = 0; r < np; ++r)
                                    for (size_t i = 0; i < m; ++i)
                                        ATy[r] += A[r * m + i] * y[i];

                                // Solve ATA * p = ATy using Gaussian elimination with partial pivoting
                                // Augmented matrix [ATA | ATy]
                                std::vector<double> aug(np * (np + 1));
                                for (int r = 0; r < np; ++r) {
                                    for (int c = 0; c < np; ++c)
                                        aug[r * (np + 1) + c] = ATA[c * np + r];
                                    aug[r * (np + 1) + np] = ATy[r];
                                }

                                for (int k = 0; k < np; ++k) {
                                    // Partial pivoting
                                    int maxRow = k;
                                    double maxVal = std::abs(aug[k * (np + 1) + k]);
                                    for (int r = k + 1; r < np; ++r) {
                                        double v = std::abs(aug[r * (np + 1) + k]);
                                        if (v > maxVal) { maxVal = v; maxRow = r; }
                                    }
                                    if (maxRow != k) {
                                        for (int c = 0; c <= np; ++c)
                                            std::swap(aug[k * (np + 1) + c], aug[maxRow * (np + 1) + c]);
                                    }

                                    double pivot = aug[k * (np + 1) + k];
                                    if (std::abs(pivot) < 1e-15)
                                        throw std::runtime_error("polyfit: singular matrix");

                                    for (int c = k; c <= np; ++c)
                                        aug[k * (np + 1) + c] /= pivot;
                                    for (int r = 0; r < np; ++r) {
                                        if (r == k) continue;
                                        double f = aug[r * (np + 1) + k];
                                        for (int c = k; c <= np; ++c)
                                            aug[r * (np + 1) + c] -= f * aug[k * (np + 1) + c];
                                    }
                                }

                                // Extract solution
                                auto p = MValue::matrix(1, np, MType::DOUBLE, alloc);
                                for (int j = 0; j < np; ++j)
                                    p.doubleDataMut()[j] = aug[j * (np + 1) + np];
                                return {p};
                            });

    // --- polyval(p, x) — evaluate polynomial using Horner's scheme ---
    engine.registerFunction("polyval",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("polyval requires 2 arguments");

                                const double *p = args[0].doubleData();
                                size_t np = args[0].numel();
                                auto &xv = args[1];
                                size_t nx = xv.numel();
                                const double *x = xv.doubleData();

                                auto r = MValue::matrix(xv.dims().rows(), xv.dims().cols(),
                                                        MType::DOUBLE, alloc);
                                for (size_t i = 0; i < nx; ++i) {
                                    // Horner: p[0]*x^(np-1) + p[1]*x^(np-2) + ... + p[np-1]
                                    double val = p[0];
                                    for (size_t j = 1; j < np; ++j)
                                        val = val * x[i] + p[j];
                                    r.doubleDataMut()[i] = val;
                                }
                                return {r};
                            });

    // --- trapz(y) / trapz(x, y) — trapezoidal numerical integration ---
    engine.registerFunction("trapz",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (args.empty())
                                    throw std::runtime_error("trapz requires at least 1 argument");

                                const double *x = nullptr;
                                const double *y = nullptr;
                                size_t n = 0;

                                if (args.size() == 1) {
                                    // trapz(y) — unit spacing
                                    y = args[0].doubleData();
                                    n = args[0].numel();
                                    double s = 0.0;
                                    for (size_t i = 1; i < n; ++i)
                                        s += 0.5 * (y[i - 1] + y[i]);
                                    return {MValue::scalar(s, alloc)};
                                }

                                // trapz(x, y)
                                x = args[0].doubleData();
                                y = args[1].doubleData();
                                n = args[0].numel();
                                if (args[1].numel() != n)
                                    throw std::runtime_error("trapz: x and y must have same length");

                                double s = 0.0;
                                for (size_t i = 1; i < n; ++i)
                                    s += 0.5 * (y[i - 1] + y[i]) * (x[i] - x[i - 1]);
                                return {MValue::scalar(s, alloc)};
                            });
}

} // namespace mlab
