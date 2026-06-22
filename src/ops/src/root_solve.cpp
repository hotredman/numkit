// ops/src/root_solve.cpp
//
// Iterative solver kernels (Brent root / bracket / Brent-min / Nelder-Mead),
// moved verbatim from toolboxes/optim's fzero.cpp so the numerical core lives in
// the kernel layer (ops, core-free). The toolbox keeps the Value-API wrappers;
// the engine adapters stay in bundle. See root_solve.hpp.

#include <numkit/ops/root_solve.hpp>

#include <numkit/ops/callback_eval.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace numkit::ops {

std::pair<double, double> findBracket(FnHandle fn, double x0, std::pmr::memory_resource *mr)
{
    constexpr int kMaxExpansions = 60;
    double        step           = (x0 == 0.0) ? 0.02 : std::abs(x0) * 0.02;
    if (step == 0.0) step = 0.02;
    double a  = x0, b = x0;
    double fa = evalScalar(fn, a, mr);
    if (fa == 0.0) return {a, a};
    double fb = fa;
    for (int i = 0; i < kMaxExpansions; ++i) {
        const double s = step * std::pow(2.0, i);
        const double aPrev = a, fAprev = fa;
        a  = x0 - s;
        b  = x0 + s;
        fa = evalScalar(fn, a, mr);
        if (fa == 0.0) return {a, a};
        fb = evalScalar(fn, b, mr);
        if (fb == 0.0) return {b, b};
        if ((fa < 0) != (fb < 0)) return {a, b};
        if ((fAprev < 0) != (fa < 0)) return {a, aPrev};
    }
    throw Error("fzero: failed to find a bracket containing a sign change near x0", 0, 0, "fzero",
                "", "numkit:fzero:noBracket");
}

double brent(FnHandle fn, double a, double b, std::pmr::memory_resource *mr)
{
    constexpr int    kMaxIter = 200;
    constexpr double kEps     = 1e-15;

    double fa = evalScalar(fn, a, mr);
    double fb = evalScalar(fn, b, mr);
    if (fa == 0.0) return a;
    if (fb == 0.0) return b;
    if ((fa < 0) == (fb < 0))
        throw Error("fzero: f(a) and f(b) must have opposite signs "
                    "(no sign change in the supplied interval)",
                    0, 0, "fzero", "", "numkit:fzero:noSignChange");

    double c = a, fc = fa, d = b - a, e = d;
    for (int it = 0; it < kMaxIter; ++it) {
        if ((fb < 0) == (fc < 0)) {
            c = a; fc = fa; d = b - a; e = d;
        }
        if (std::abs(fc) < std::abs(fb)) {
            a = b; b = c; c = a;
            fa = fb; fb = fc; fc = fa;
        }
        const double tol1 = 2.0 * kEps * std::abs(b) + 0.5 * 1e-15;
        const double xm   = 0.5 * (c - b);
        if (std::abs(xm) <= tol1 || fb == 0.0) return b;

        if (std::abs(e) >= tol1 && std::abs(fa) > std::abs(fb)) {
            const double s = fb / fa;
            double       p, q;
            if (a == c) {
                p = 2.0 * xm * s;
                q = 1.0 - s;
            } else {
                const double r  = fb / fc;
                const double sa = fa / fc;
                p = s * (2.0 * xm * sa * (sa - r) - (b - a) * (r - 1.0));
                q = (sa - 1.0) * (r - 1.0) * (s - 1.0);
            }
            if (p > 0) q = -q;
            p = std::abs(p);
            const double min1 = 3.0 * xm * q - std::abs(tol1 * q);
            const double min2 = std::abs(e * q);
            if (2.0 * p < std::min(min1, min2)) {
                e = d;
                d = p / q;
            } else {
                d = xm; e = d;
            }
        } else {
            d = xm; e = d;
        }
        a = b; fa = fb;
        if (std::abs(d) > tol1)
            b += d;
        else
            b += (xm > 0 ? std::abs(tol1) : -std::abs(tol1));
        fb = evalScalar(fn, b, mr);
    }
    throw Error("fzero: failed to converge within iteration limit", 0, 0, "fzero", "",
                "numkit:fzero:noConverge");
}

double brentMin(FnHandle fn, double a, double b, double tol, std::pmr::memory_resource *mr)
{
    constexpr int    kMaxIter = 200;
    const double     GOLD     = 0.5 * (3.0 - std::sqrt(5.0));  // ~= 0.381966
    constexpr double kEps     = 1e-10;

    double x  = a + GOLD * (b - a);
    double w  = x, v = x;
    double fx = evalScalar(fn, x, mr);
    double fw = fx, fv = fx;
    double d  = 0.0, e = 0.0;

    for (int it = 0; it < kMaxIter; ++it) {
        const double m  = 0.5 * (a + b);
        const double t1 = tol * std::abs(x) + kEps;
        const double t2 = 2.0 * t1;
        if (std::abs(x - m) <= (t2 - 0.5 * (b - a))) return x;

        bool gold = true;
        if (std::abs(e) > t1) {
            // Try parabolic fit through (v, w, x).
            const double r1 = (x - w) * (fx - fv);
            double       q  = (x - v) * (fx - fw);
            double       p  = (x - v) * q - (x - w) * r1;
            q               = 2.0 * (q - r1);
            if (q > 0) p = -p;
            q                  = std::abs(q);
            const double etemp = e;
            e                  = d;
            if (std::abs(p) < std::abs(0.5 * q * etemp) && p > q * (a - x) && p < q * (b - x)) {
                d             = p / q;
                const double u = x + d;
                if ((u - a) < t2 || (b - u) < t2) d = (m >= x ? t1 : -t1);
                gold = false;
            }
        }
        if (gold) {
            e = (x >= m ? a - x : b - x);
            d = GOLD * e;
        }
        const double u  = (std::abs(d) >= t1) ? x + d : x + (d >= 0 ? t1 : -t1);
        const double fu = evalScalar(fn, u, mr);

        if (fu <= fx) {
            if (u >= x) a = x; else b = x;
            v = w; fv = fw;
            w = x; fw = fx;
            x = u; fx = fu;
        } else {
            if (u < x) a = u; else b = u;
            if (fu <= fw || w == x) {
                v = w; fv = fw;
                w = u; fw = fu;
            } else if (fu <= fv || v == x || v == w) {
                v = u; fv = fu;
            }
        }
    }
    return x;  // best so far
}

ScratchVec<double> nelderMead(FnHandle fn, const double *x0, std::size_t n, double tol,
                              std::pmr::memory_resource *mr)
{
    constexpr int    kMaxIter = 500;
    constexpr double ALPHA    = 1.0;  // reflection
    constexpr double GAMMA    = 2.0;  // expansion
    constexpr double RHO      = 0.5;  // contraction
    constexpr double SIGMA    = 0.5;  // shrink

    auto evalAt = [&](const double *x) -> double { return evalVecToScalar(fn, x, n, mr); };

    // Buffers (all on the per-call arena passed in via mr).
    ScratchVec<double> sx((n + 1) * n, mr);
    ScratchVec<double> fv(n + 1, mr);

    // Initial simplex: x0 + 0.05*e_i (or 0.00025 if x0_i == 0).
    for (std::size_t j = 0; j < n; ++j) sx[j] = x0[j];
    fv[0] = evalAt(&sx[0]);
    for (std::size_t i = 1; i <= n; ++i) {
        for (std::size_t j = 0; j < n; ++j) sx[i * n + j] = x0[j];
        const double xi    = x0[i - 1];
        sx[i * n + (i - 1)] = (xi != 0.0 ? 1.05 * xi : 0.00025);
        fv[i]               = evalAt(&sx[i * n]);
    }

    ScratchVec<std::size_t> ord(n + 1, mr);
    ScratchVec<double>      newSx((n + 1) * n, mr);
    ScratchVec<double>      newFv(n + 1, mr);
    ScratchVec<double>      centroid(n, mr);
    ScratchVec<double>      xr(n, mr);
    ScratchVec<double>      xe(n, mr);
    ScratchVec<double>      xc(n, mr);

    for (int it = 0; it < kMaxIter; ++it) {
        // Sort vertices by fv ascending.
        for (std::size_t i = 0; i <= n; ++i) ord[i] = i;
        std::sort(ord.begin(), ord.end(), [&](std::size_t a, std::size_t b) { return fv[a] < fv[b]; });

        for (std::size_t i = 0; i <= n; ++i) {
            const double *src = &sx[ord[i] * n];
            for (std::size_t j = 0; j < n; ++j) newSx[i * n + j] = src[j];
            newFv[i] = fv[ord[i]];
        }
        std::copy(newSx.begin(), newSx.end(), sx.begin());
        std::copy(newFv.begin(), newFv.end(), fv.begin());

        // MATLAB convergence requires BOTH the function-value spread (TolFun) AND
        // the simplex size relative to the best vertex (TolX) within tol.
        const double fspread = fv[n] - fv[0];
        double       xspread = 0.0;
        for (std::size_t i = 1; i <= n; ++i)
            for (std::size_t j = 0; j < n; ++j)
                xspread = std::max(xspread, std::abs(sx[i * n + j] - sx[j]));
        if (fspread <= tol && xspread <= tol) break;

        for (std::size_t j = 0; j < n; ++j) centroid[j] = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = 0; j < n; ++j) centroid[j] += sx[i * n + j];
        for (std::size_t j = 0; j < n; ++j) centroid[j] /= static_cast<double>(n);

        for (std::size_t j = 0; j < n; ++j)
            xr[j] = centroid[j] + ALPHA * (centroid[j] - sx[n * n + j]);
        const double fxr = evalAt(xr.data());

        if (fxr < fv[0]) {
            for (std::size_t j = 0; j < n; ++j) xe[j] = centroid[j] + GAMMA * (xr[j] - centroid[j]);
            const double fxe = evalAt(xe.data());
            if (fxe < fxr) {
                for (std::size_t j = 0; j < n; ++j) sx[n * n + j] = xe[j];
                fv[n] = fxe;
            } else {
                for (std::size_t j = 0; j < n; ++j) sx[n * n + j] = xr[j];
                fv[n] = fxr;
            }
        } else if (fxr < fv[n - 1]) {
            for (std::size_t j = 0; j < n; ++j) sx[n * n + j] = xr[j];
            fv[n] = fxr;
        } else {
            const bool    outside = fxr < fv[n];
            const double *base    = outside ? xr.data() : &sx[n * n];
            for (std::size_t j = 0; j < n; ++j) xc[j] = centroid[j] + RHO * (base[j] - centroid[j]);
            const double fxc      = evalAt(xc.data());
            const double fcompare = outside ? fxr : fv[n];
            if (fxc <= fcompare) {
                for (std::size_t j = 0; j < n; ++j) sx[n * n + j] = xc[j];
                fv[n] = fxc;
            } else {
                for (std::size_t i = 1; i <= n; ++i) {
                    for (std::size_t j = 0; j < n; ++j)
                        sx[i * n + j] = sx[j] + SIGMA * (sx[i * n + j] - sx[j]);
                    fv[i] = evalAt(&sx[i * n]);
                }
            }
        }
    }

    ScratchVec<double> best(n, mr);
    for (std::size_t j = 0; j < n; ++j) best[j] = sx[j];
    return best;
}

} // namespace numkit::ops
