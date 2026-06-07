// libs/signal/src/fit/fit_reg.cpp
//
// CallContext register half of fit/fit.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/stats/distributions/beta.hpp>
#include <numkit/stats/distributions/chi2.hpp>
#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/fit/fit.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "fit_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

static double parse_alpha_arg(Span<const Value> args, size_t pos, double def) {
    if (pos >= args.size() || args[pos].isEmpty()) return def;
    return args[pos].toScalar();
}

void normfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("normfit: requires X[, alpha[, censoring[, freq[, options]]]]",
                    0, 0, "normfit", "", "numkit:normfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    const Value &cens = (args.size() > 2) ? args[2] : Value::Empty;
    const Value &freq = (args.size() > 3) ? args[3] : Value::Empty;
    auto [mu, sd, muci, sdci] = normfit(args[0], alpha, cens, freq, ctx.engine->resource());
    outs[0] = std::move(mu);
    if (nargout > 1) outs[1] = std::move(sd);
    if (nargout > 2) outs[2] = std::move(muci);
    if (nargout > 3) outs[3] = std::move(sdci);
}

void poissfit_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("poissfit: requires X[, alpha]",
                    0, 0, "poissfit", "", "numkit:poissfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [lam, ci] = poissfit(args[0], alpha, ctx.engine->resource());
    outs[0] = std::move(lam);
    if (nargout > 1) outs[1] = std::move(ci);
}

void expfit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("expfit: requires X[, alpha[, censoring[, freq]]]",
                    0, 0, "expfit", "", "numkit:expfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    const Value &cens = (args.size() > 2) ? args[2] : Value::Empty;
    const Value &freq = (args.size() > 3) ? args[3] : Value::Empty;
    auto [mu, ci] = expfit(args[0], alpha, cens, freq, ctx.engine->resource());
    outs[0] = std::move(mu);
    if (nargout > 1) outs[1] = std::move(ci);
}

void unifit_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("unifit: requires X[, alpha]",
                    0, 0, "unifit", "", "numkit:unifit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [a, b, aci, bci] = unifit(args[0], alpha, ctx.engine->resource());
    outs[0] = std::move(a);
    if (nargout > 1) outs[1] = std::move(b);
    if (nargout > 2) outs[2] = std::move(aci);
    if (nargout > 3) outs[3] = std::move(bci);
}

void lognfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lognfit: requires X[, alpha[, censoring[, freq[, options]]]]",
                    0, 0, "lognfit", "", "numkit:lognfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    // 3rd arg = censoring (may be empty []), 4th = freq (may be empty),
    // 5th = options struct (silently ignored — we use fixed 200 / 1e-10).
    const Value &cens = (args.size() > 2) ? args[2] : Value::Empty;
    const Value &freq = (args.size() > 3) ? args[3] : Value::Empty;
    auto [parm, pci] = lognfit(args[0], alpha, cens, freq, ctx.engine->resource());
    outs[0] = std::move(parm);
    if (nargout > 1) outs[1] = std::move(pci);
}

void binofit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("binofit: requires (X, N[, alpha])",
                    0, 0, "binofit", "", "numkit:binofit:nargin");
    const double alpha = parse_alpha_arg(args, 2, 0.05);
    auto [phat, pci] = binofit(args[0], args[1], alpha, ctx.engine->resource());
    outs[0] = std::move(phat);
    if (nargout > 1) outs[1] = std::move(pci);
}

void raylfit_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("raylfit: requires X[, alpha]",
                    0, 0, "raylfit", "", "numkit:raylfit:nargin");
    const double alpha = parse_alpha_arg(args, 1, 0.05);
    auto [shat, sci] = raylfit(args[0], alpha, ctx.engine->resource());
    outs[0] = std::move(shat);
    if (nargout > 1) outs[1] = std::move(sci);
}

// ─── *like adapters ───────────────────────────────────────────────────

// Digamma ψ(z) for z > 0 — recurrence to z>=8 then asymptotic series.
static double digamma(double z)
{
    double r = 0.0;
    while (z < 8.0) { r -= 1.0 / z; z += 1.0; }
    const double inv  = 1.0 / z;
    const double inv2 = inv * inv;
    r += std::log(z) - 0.5 * inv;
    r -= inv2 * (1.0/12.0 - inv2 * (1.0/120.0 - inv2 * 1.0/252.0));
    return r;
}

// Fill a 2×2 inverse observed-Fisher matrix `p` (column-major,
// parameter order [p0, p1]) for a 2-parameter likelihood. Uses central
// differences (no in-tree trigamma); step h ≈ eps^(1/4) ≈ 1e-4 is the
// optimal balance between truncation O(h²) and roundoff O(eps/h²).
// Caller must have already verified that `nL` is finite — we do not
// re-validate inputs.
template <class Eval>
static void fill_fd_avar2(double *p, double p0, double p1,
                          double nL, Eval eval_nL)
{
    const double NaNd = std::numeric_limits<double>::quiet_NaN();
    if (!std::isfinite(nL)) {
        p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
        return;
    }
    const double h0 = std::max(1e-4, 1e-4 * std::abs(p0));
    const double h1 = std::max(1e-4, 1e-4 * std::abs(p1));
    const double f_p0 = eval_nL(p0 + h0, p1);
    const double f_m0 = eval_nL(p0 - h0, p1);
    const double f_p1 = eval_nL(p0, p1 + h1);
    const double f_m1 = eval_nL(p0, p1 - h1);
    const double f_pp = eval_nL(p0 + h0, p1 + h1);
    const double f_pm = eval_nL(p0 + h0, p1 - h1);
    const double f_mp = eval_nL(p0 - h0, p1 + h1);
    const double f_mm = eval_nL(p0 - h0, p1 - h1);
    const double I00 = (f_p0 - 2.0 * nL + f_m0) / (h0 * h0);
    const double I11 = (f_p1 - 2.0 * nL + f_m1) / (h1 * h1);
    const double I01 = (f_pp - f_pm - f_mp + f_mm) / (4.0 * h0 * h1);
    const double det = I00 * I11 - I01 * I01;
    if (det == 0.0 || !std::isfinite(det)) {
        p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
    } else {
        const double inv = 1.0 / det;
        p[0] =  I11 * inv;
        p[1] = -I01 * inv;
        p[2] = -I01 * inv;
        p[3] =  I00 * inv;
    }
}

// 3-parameter analogue of `fill_fd_avar2`. Fills a 3×3 column-major
// inverse observed-Fisher matrix at `(p0, p1, p2)`. 18 nL evaluations.
template <class Eval>
static void fill_fd_avar3(double *p, double p0, double p1, double p2,
                          double nL, Eval eval_nL)
{
    const double NaNd = std::numeric_limits<double>::quiet_NaN();
    auto setNaN = [&]() {
        for (int i = 0; i < 9; ++i) p[i] = NaNd;
    };
    if (!std::isfinite(nL)) { setNaN(); return; }

    const double h0 = std::max(1e-4, 1e-4 * std::abs(p0));
    const double h1 = std::max(1e-4, 1e-4 * std::abs(p1));
    const double h2 = std::max(1e-4, 1e-4 * std::abs(p2));
    auto e = [&](double q0, double q1, double q2) { return eval_nL(q0, q1, q2); };
    // Diagonal entries.
    const double H00 = (e(p0+h0,p1,p2) - 2.0*nL + e(p0-h0,p1,p2)) / (h0*h0);
    const double H11 = (e(p0,p1+h1,p2) - 2.0*nL + e(p0,p1-h1,p2)) / (h1*h1);
    const double H22 = (e(p0,p1,p2+h2) - 2.0*nL + e(p0,p1,p2-h2)) / (h2*h2);
    // Off-diagonals via 4-point stencil.
    auto cross = [&](int a, int b) {
        double da[3] = {0.0, 0.0, 0.0}, db[3] = {0.0, 0.0, 0.0};
        const double ha = (a == 0 ? h0 : (a == 1 ? h1 : h2));
        const double hb = (b == 0 ? h0 : (b == 1 ? h1 : h2));
        da[a] = ha; db[b] = hb;
        const double pp = e(p0+da[0]+db[0], p1+da[1]+db[1], p2+da[2]+db[2]);
        const double pm = e(p0+da[0]-db[0], p1+da[1]-db[1], p2+da[2]-db[2]);
        const double mp = e(p0-da[0]+db[0], p1-da[1]+db[1], p2-da[2]+db[2]);
        const double mm = e(p0-da[0]-db[0], p1-da[1]-db[1], p2-da[2]-db[2]);
        return (pp - pm - mp + mm) / (4.0 * ha * hb);
    };
    const double H01 = cross(0, 1);
    const double H02 = cross(0, 2);
    const double H12 = cross(1, 2);
    // 3×3 cofactor inversion. Symmetric: H10=H01, H20=H02, H21=H12.
    const double C00 = H11*H22 - H12*H12;
    const double C01 = -(H01*H22 - H12*H02);
    const double C02 = H01*H12 - H11*H02;
    const double C11 = H00*H22 - H02*H02;
    const double C12 = -(H00*H12 - H01*H02);
    const double C22 = H00*H11 - H01*H01;
    const double det = H00 * C00 + H01 * C01 + H02 * C02;
    if (det == 0.0 || !std::isfinite(det)) { setNaN(); return; }
    const double inv = 1.0 / det;
    // Column-major 3×3, parameter order [p0, p1, p2].
    p[0] = C00 * inv; p[1] = C01 * inv; p[2] = C02 * inv;  // col 0
    p[3] = C01 * inv; p[4] = C11 * inv; p[5] = C12 * inv;  // col 1
    p[6] = C02 * inv; p[7] = C12 * inv; p[8] = C22 * inv;  // col 2
}

static void like2_reg(const char *fn,
                      double (*impl)(double, double, const Value &, std::pmr::memory_resource *),
                      Span<const Value> args, Span<Value> outs,
                      CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error(std::string(fn) + ": requires (params[2], data)",
                    0, 0, fn, "", "numkit:like:nargin");
    const double p0 = args[0].elemAsDouble(0);
    const double p1 = args[0].elemAsDouble(1);
    const double nL = impl(p0, p1, args[1], ctx.engine->resource());
    outs[0] = Value::scalar(nL, ctx.engine->resource());
}

void normlike_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("normlike: requires (params=[mu sigma], data[, cens, freq])",
                    0, 0, "normlike", "", "numkit:normlike:nargin");
    const double mu    = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, ctx.engine->resource());
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    const double nL = normlike(mu, sigma, args[1], cens, freq, ctx.engine->resource());
    outs[0] = Value::scalar(nL, ctx.engine->resource());

    // Second output: aVar = inv(observed Fisher information).
    // Order [mu, sigma]; symmetric 2×2.
    if (nargout >= 2) {
        const Value &x = args[1];
        const size_t N = x.numel();
        const bool useC = cens.numel() > 0;
        const bool useF = freq.numel() > 0;
        // Observed information I (= positive Hessian of nL):
        //   uncensored row, weight w:
        //     I_μμ += w / σ²
        //     I_σσ += w · (-1/σ² + 3·d²/σ⁴)
        //     I_μσ += w · 2·d/σ³            (d = x-μ)
        //   right-censored row, weight w (h=φ(z)/S(z), h'=h(h-z)):
        //     I_μμ += w · h'/σ²
        //     I_σσ += w · (2z·h + z²·h')/σ²
        //     I_μσ += w · (z·h' + h)/σ²
        const double inv_s   = 1.0 / sigma;
        const double inv_s2  = inv_s * inv_s;
        const double inv_s3  = inv_s2 * inv_s;
        const double inv_s4  = inv_s2 * inv_s2;
        const double sqrt2pi_inv = 1.0 / std::sqrt(2.0 * 3.14159265358979323846);
        const double sqrt2_inv   = 1.0 / std::sqrt(2.0);
        double I00 = 0.0, I01 = 0.0, I11 = 0.0;
        bool nanSeen = false;
        for (size_t i = 0; i < N; ++i) {
            const double w = useF ? freq.elemAsDouble(i) : 1.0;
            if (w == 0.0) continue;
            const double xi = x.elemAsDouble(i);
            if (std::isnan(xi)) { nanSeen = true; break; }
            const double d = xi - mu;
            const double z = d * inv_s;
            const bool censored = useC && (cens.elemAsDouble(i) != 0.0);
            if (!censored) {
                I00 += w * inv_s2;
                I11 += w * (-inv_s2 + 3.0 * d * d * inv_s4);
                I01 += w * 2.0 * d * inv_s3;
            } else {
                const double phi = sqrt2pi_inv * std::exp(-0.5 * z * z);
                const double S   = 0.5 * std::erfc(z * sqrt2_inv);
                const double h   = phi / S;
                const double hp  = h * (h - z);
                I00 += w * hp * inv_s2;
                I11 += w * (2.0 * z * h + z * z * hp) * inv_s2;
                I01 += w * (z * hp + h) * inv_s2;
            }
        }
        const double NaNd = std::numeric_limits<double>::quiet_NaN();
        Value av = Value::matrix(2, 2, ValueType::DOUBLE, ctx.engine->resource());
        double *p = av.doubleDataMut();
        if (nanSeen || N == 0 || !(sigma > 0.0)) {
            p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
        } else {
            const double det = I00 * I11 - I01 * I01;
            if (det == 0.0 || !std::isfinite(det)) {
                p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
            } else {
                const double inv = 1.0 / det;
                // column-major 2×2: stored [a, b, c, d] = [(1,1), (2,1), (1,2), (2,2)]
                p[0] =  I11 * inv;
                p[1] = -I01 * inv;
                p[2] = -I01 * inv;
                p[3] =  I00 * inv;
            }
        }
        outs[1] = std::move(av);
    }
}

void lognlike_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("lognlike: requires (params=[mu sigma], data[, cens, freq])",
                    0, 0, "lognlike", "", "numkit:lognlike:nargin");
    auto *mr = ctx.engine->resource();
    const double mu    = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    Value av = Value::matrix(2, 2, ValueType::DOUBLE, mr);
    const double nL = lognlike_full(mu, sigma, args[1], cens, freq,
                                    nargout >= 2 ? av.doubleDataMut() : nullptr);
    outs[0] = Value::scalar(nL, mr);
    if (nargout >= 2) outs[1] = std::move(av);
}

void gamlike_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("gamlike: requires (params=[a b], data)",
                    0, 0, "gamlike", "", "numkit:gamlike:nargin");
    auto *mr = ctx.engine->resource();
    const double a  = args[0].elemAsDouble(0);
    const double b  = args[0].elemAsDouble(1);
    const Value &x  = args[1];
    const double nL = gamlike(a, b, x, mr);
    outs[0] = Value::scalar(nL, mr);

    // Second output: 2×2 inverse observed-Fisher info, parameter order
    // [a, b]. Computed via central-difference Hessian (no trigamma).
    if (nargout >= 2) {
        Value av = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        fill_fd_avar2(av.doubleDataMut(), a, b, nL,
                      [&](double aa, double bb) { return gamlike(aa, bb, x, mr); });
        outs[1] = std::move(av);
    }
}

void betalike_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("betalike: requires (params=[a b], data)",
                    0, 0, "betalike", "", "numkit:betalike:nargin");
    auto *mr = ctx.engine->resource();
    const double a  = args[0].elemAsDouble(0);
    const double b  = args[0].elemAsDouble(1);
    const Value &x  = args[1];
    const double nL = betalike(a, b, x, mr);
    outs[0] = Value::scalar(nL, mr);

    // Second output: 2×2 inverse Fisher info, parameter order [a, b].
    // MATLAB's betalike uses BHHH (outer-product-of-gradients) — the
    // sum of per-row score outer products — NOT the Hessian. Verified
    // by direct probe: at user-supplied params (away from MLE) the two
    // estimators differ; MATLAB / Octave both report the BHHH form.
    // Score per row:
    //   ∂log f/∂a = log x_i  - ψ(a) + ψ(a+b)
    //   ∂log f/∂b = log(1-x_i) - ψ(b) + ψ(a+b)
    if (nargout >= 2) {
        Value av = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        double *p = av.doubleDataMut();
        const double NaNd = std::numeric_limits<double>::quiet_NaN();
        const size_t N = x.numel();
        if (!std::isfinite(nL) || !(a > 0.0) || !(b > 0.0) || N == 0) {
            p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
        } else {
            const double Ca = -digamma(a) + digamma(a + b);
            const double Cb = -digamma(b) + digamma(a + b);
            double Iaa = 0.0, Ibb = 0.0, Iab = 0.0;
            bool bad = false;
            for (size_t i = 0; i < N; ++i) {
                const double xi = x.elemAsDouble(i);
                if (xi <= 0.0 || xi >= 1.0) { bad = true; break; }
                const double sa = std::log(xi)    + Ca;
                const double sb = std::log1p(-xi) + Cb;
                Iaa += sa * sa;
                Ibb += sb * sb;
                Iab += sa * sb;
            }
            const double det = Iaa * Ibb - Iab * Iab;
            if (bad || det == 0.0 || !std::isfinite(det)) {
                p[0] = NaNd; p[1] = NaNd; p[2] = NaNd; p[3] = NaNd;
            } else {
                const double inv = 1.0 / det;
                p[0] =  Ibb * inv;
                p[1] = -Iab * inv;
                p[2] = -Iab * inv;
                p[3] =  Iaa * inv;
            }
        }
        outs[1] = std::move(av);
    }
}

void wbllike_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("wbllike: requires (params=[scale shape], data[, cens, freq])",
                    0, 0, "wbllike", "", "numkit:wbllike:nargin");
    const double scale = args[0].elemAsDouble(0);
    const double shape = args[0].elemAsDouble(1);
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, ctx.engine->resource());
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    const double nL = wbllike_full(scale, shape, args[1], cens, freq, ctx.engine->resource());
    outs[0] = Value::scalar(nL, ctx.engine->resource());
    // AVAR (2-output form): not yet implemented; observed Fisher info
    // for Weibull has nontrivial mixed partials. Deferred.
}

void evlike_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("evlike: requires (params=[mu sigma], data[, cens, freq])",
                    0, 0, "evlike", "", "numkit:evlike:nargin");
    const double mu    = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, ctx.engine->resource());
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    const double nL = evlike_full(mu, sigma, args[1], cens, freq, ctx.engine->resource());
    outs[0] = Value::scalar(nL, ctx.engine->resource());
    // AVAR (2-output form): not yet implemented — observed Fisher info
    // for Gumbel-min has nontrivial cross-terms; deferred. See
    // for the partial-closure note.
}

void explike_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("explike: requires (mu, data[, cens, freq])",
                    0, 0, "explike", "", "numkit:explike:nargin");
    auto *mr = ctx.engine->resource();
    const double mu = args[0].toScalar();
    Value emptyVal = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    const Value &cens = (args.size() >= 3) ? args[2] : emptyVal;
    const Value &freq = (args.size() >= 4) ? args[3] : emptyVal;
    double avar = std::numeric_limits<double>::quiet_NaN();
    const double nL = explike_full(mu, args[1], cens, freq,
                                   nargout >= 2 ? &avar : nullptr);
    outs[0] = Value::scalar(nL, mr);
    if (nargout >= 2) outs[1] = Value::scalar(avar, mr);
}

void gevlike_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 3)
        throw Error("gevlike: requires (params=[k sigma mu], data)",
                    0, 0, "gevlike", "", "numkit:gevlike:nargin");
    auto *mr = ctx.engine->resource();
    const double k     = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    const double mu    = args[0].elemAsDouble(2);
    const Value &x     = args[1];
    const double nL = gevlike(k, sigma, mu, x, mr);
    outs[0] = Value::scalar(nL, mr);
    if (nargout >= 2) {
        Value ac = Value::matrix(3, 3, ValueType::DOUBLE, mr);
        fill_fd_avar3(ac.doubleDataMut(), k, sigma, mu, nL,
                      [&](double kk, double ss, double mm) {
                          return gevlike(kk, ss, mm, x, mr);
                      });
        outs[1] = std::move(ac);
    }
}

void gplike_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2 || args[0].numel() < 2)
        throw Error("gplike: requires (params=[k sigma], data)",
                    0, 0, "gplike", "", "numkit:gplike:nargin");
    auto *mr = ctx.engine->resource();
    const double k     = args[0].elemAsDouble(0);
    const double sigma = args[0].elemAsDouble(1);
    const Value &x     = args[1];
    const double nL = gplike(k, sigma, x, mr);
    outs[0] = Value::scalar(nL, mr);
    if (nargout >= 2) {
        Value ac = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        fill_fd_avar2(ac.doubleDataMut(), k, sigma, nL,
                      [&](double kk, double ss) { return gplike(kk, ss, x, mr); });
        outs[1] = std::move(ac);
    }
}

// ── mle (closed-form max-likelihood estimator) ─────────────────────
// Supported distribution families (closed-form MLE -- no optimization):
//   'normal':      [muhat, sigmahat], sigmahat uses N normalisation
//   'exponential': muhat = mean(x)
//   'poisson':     lambdahat = mean(x)
//   'lognormal':   [muhat, sigmahat] of log(x)
// Custom 'pdf'/'logpdf'/'nloglf' with 'start' x0 deferred -- needs
// Nelder-Mead simplex over a function-handle nLL.
void mle_reg(Span<const Value> args, size_t nargout,
             Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mle: requires (data[, options])",
                    0, 0, "mle", "", "numkit:mle:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];
    const std::size_t N = x.numel();
    if (N < 2)
        throw Error("mle: need at least 2 observations",
                    0, 0, "mle", "", "numkit:mle:tooSmall");

    std::string dist = "normal";
    bool custom_fn = false;
    double alpha = 0.05;   // 100·(1-alpha)% confidence for the 2nd output pci
    for (std::size_t i = 1; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("mle: option name must be a string",
                        0, 0, "mle", "", "numkit:mle:badOption");
        const std::string opt = args[i].toString();
        if (opt == "distribution" || opt == "Distribution")
            dist = args[i + 1].toString();
        else if (opt == "alpha" || opt == "Alpha") {
            alpha = args[i + 1].toScalar();
            if (!(alpha > 0.0 && alpha < 1.0))
                throw Error("mle: Alpha must be in (0, 1)",
                            0, 0, "mle", "", "numkit:mle:badOption");
        }
        else if (opt == "pdf" || opt == "PDF" ||
                 opt == "logpdf" || opt == "LogPDF" ||
                 opt == "nloglf" || opt == "NLogLF")
            custom_fn = true;
        else if (opt == "start" || opt == "Start") { /* paired with custom_fn */ }
        else
            throw Error("mle: unknown option '" + opt + "'",
                        0, 0, "mle", "", "numkit:mle:badOption");
    }

    // pci (2nd output): 2×k matrix, row 1 = lower bounds, row 2 = upper bounds,
    // one column per parameter — taken from the matching *fit CI (which already
    // matches MATLAB). emitNorm2: two-parameter (mu, sigma) layout.
    auto emitPci2 = [&](const Value &ci1, const Value &ci2) {
        Value pci = Value::matrix(2, 2, ValueType::DOUBLE, mr);
        double *p = pci.doubleDataMut();
        p[0] = ci1.elemAsDouble(0); p[1] = ci1.elemAsDouble(1);   // col 1
        p[2] = ci2.elemAsDouble(0); p[3] = ci2.elemAsDouble(1);   // col 2
        outs[1] = std::move(pci);
    };
    auto emitPci1 = [&](const Value &ci) {
        Value pci = Value::matrix(2, 1, ValueType::DOUBLE, mr);
        double *p = pci.doubleDataMut();
        p[0] = ci.elemAsDouble(0); p[1] = ci.elemAsDouble(1);
        outs[1] = std::move(pci);
    };
    if (custom_fn)
        throw Error("mle: custom 'pdf'/'logpdf'/'nloglf' fitting via "
                    "Nelder-Mead is deferred -- supported distributions "
                    "in this revision: 'normal', 'exponential', "
                    "'poisson', 'lognormal'",
                    0, 0, "mle", "", "numkit:mle:customFnNYI");

    const double *xd = x.doubleData();

    if (dist == "normal" || dist == "Normal" || dist == "norm") {
        double sum = 0.0;
        for (std::size_t i = 0; i < N; ++i) sum += xd[i];
        const double mean = sum / static_cast<double>(N);
        double ss = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
            const double d = xd[i] - mean;
            ss += d * d;
        }
        const double sigma = std::sqrt(ss / static_cast<double>(N));
        auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = mean;
        out.doubleDataMut()[1] = sigma;
        outs[0] = std::move(out);
        if (nargout >= 2) {
            auto [m, s, muci, sdci] = normfit(x, alpha, Value::Empty, Value::Empty, mr);
            (void)m; (void)s;
            emitPci2(muci, sdci);
        }
        return;
    }
    if (dist == "exponential" || dist == "Exponential" || dist == "exp") {
        double sum = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
            if (xd[i] < 0.0)
                throw Error("mle: exponential requires non-negative data",
                            0, 0, "mle", "", "numkit:mle:badData");
            sum += xd[i];
        }
        auto out = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = sum / static_cast<double>(N);
        outs[0] = std::move(out);
        if (nargout >= 2) {
            auto [m, ci] = expfit(x, alpha, Value::Empty, Value::Empty, mr);
            (void)m;
            emitPci1(ci);
        }
        return;
    }
    if (dist == "poisson" || dist == "Poisson" || dist == "poiss") {
        double sum = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
            if (xd[i] < 0.0 || xd[i] != std::floor(xd[i]))
                throw Error("mle: poisson requires non-negative integer data",
                            0, 0, "mle", "", "numkit:mle:badData");
            sum += xd[i];
        }
        auto out = Value::matrix(1, 1, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = sum / static_cast<double>(N);
        outs[0] = std::move(out);
        if (nargout >= 2) {
            auto [lam, ci] = poissfit(x, alpha, mr);
            (void)lam;
            emitPci1(ci);
        }
        return;
    }
    if (dist == "lognormal" || dist == "Lognormal" || dist == "logn") {
        double sum_log = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
            if (xd[i] <= 0.0)
                throw Error("mle: lognormal requires strictly positive data",
                            0, 0, "mle", "", "numkit:mle:badData");
            sum_log += std::log(xd[i]);
        }
        const double mu = sum_log / static_cast<double>(N);
        double ss = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
            const double d = std::log(xd[i]) - mu;
            ss += d * d;
        }
        const double sigma = std::sqrt(ss / static_cast<double>(N));
        auto out = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        out.doubleDataMut()[0] = mu;
        out.doubleDataMut()[1] = sigma;
        outs[0] = std::move(out);
        if (nargout >= 2) {
            // CI on (mu, sigma) of log(x) — matches MATLAB's lognormal pci.
            Value logx = Value::matrix(1, N, ValueType::DOUBLE, mr);
            double *lx = logx.doubleDataMut();
            for (std::size_t i = 0; i < N; ++i) lx[i] = std::log(xd[i]);
            auto [m, s, muci, sdci] = normfit(logx, alpha, Value::Empty, Value::Empty, mr);
            (void)m; (void)s;
            emitPci2(muci, sdci);
        }
        return;
    }
    throw Error("mle: distribution '" + dist + "' not supported "
                "(supported: normal, exponential, poisson, lognormal). "
                "Custom pdf/logpdf/nloglf via Nelder-Mead is deferred.",
                0, 0, "mle", "", "numkit:mle:dist");
}

// ── fitdist (returns probability-distribution struct) ──────────────
// MATLAB returns a probability-distribution OBJECT with class methods
// (.cdf, .pdf, .icdf etc). numkit doesn't ship full OOP for
// distributions; we return a struct with:
//   .DistributionName  — canonical name string
//   .ParameterValues   — 1xN row of fitted parameters
//   .ParameterNames    — cell array of parameter-name strings
//   .NumObservations   — sample size
// Wraps mle for the closed-form distributions; same set: normal,
// exponential, poisson, lognormal.
void fitdist_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fitdist: requires (data, 'DistributionName')",
                    0, 0, "fitdist", "", "numkit:fitdist:nargin");
    if (!args[1].isChar() && !args[1].isString())
        throw Error("fitdist: 2nd argument must be a distribution-name string",
                    0, 0, "fitdist", "", "numkit:fitdist:badName");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];
    const std::size_t N = x.numel();
    if (N < 2)
        throw Error("fitdist: need at least 2 observations",
                    0, 0, "fitdist", "", "numkit:fitdist:tooSmall");

    const std::string raw = args[1].toString();
    // Canonicalise.
    std::string name;
    name.reserve(raw.size());
    for (char c : raw) name += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string canonical;
    std::vector<std::string> param_names;
    if (name == "normal" || name == "norm") {
        canonical = "Normal";
        param_names = {"mu", "sigma"};
    } else if (name == "exponential" || name == "exp") {
        canonical = "Exponential";
        param_names = {"mu"};
    } else if (name == "poisson" || name == "poiss") {
        canonical = "Poisson";
        param_names = {"lambda"};
    } else if (name == "lognormal" || name == "logn") {
        canonical = "Lognormal";
        param_names = {"mu", "sigma"};
    } else {
        throw Error("fitdist: distribution '" + raw + "' not supported "
                    "(supported: 'Normal', 'Exponential', 'Poisson', "
                    "'Lognormal'). Other MATLAB distributions deferred.",
                    0, 0, "fitdist", "", "numkit:fitdist:dist");
    }

    // Compute parameters per MATLAB convention. NB: fitdist uses
    // sample std (N-1) for Normal/Lognormal, NOT MLE std (N).
    // mle() in this file uses N normalisation (true MLE); we override
    // here to match MATLAB's fitdist behaviour.
    const double *xd = x.doubleData();
    Value params;
    if (canonical == "Normal") {
        double sum = 0.0;
        for (std::size_t i = 0; i < N; ++i) sum += xd[i];
        const double mean = sum / static_cast<double>(N);
        double ss = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
            const double d = xd[i] - mean;
            ss += d * d;
        }
        const double sigma = std::sqrt(ss / static_cast<double>(N - 1));  // N-1
        params = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        params.doubleDataMut()[0] = mean;
        params.doubleDataMut()[1] = sigma;
    } else if (canonical == "Lognormal") {
        double sum_log = 0.0;
        for (std::size_t i = 0; i < N; ++i) sum_log += std::log(xd[i]);
        const double mu = sum_log / static_cast<double>(N);
        double ss = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
            const double d = std::log(xd[i]) - mu;
            ss += d * d;
        }
        const double sigma = std::sqrt(ss / static_cast<double>(N - 1));
        params = Value::matrix(1, 2, ValueType::DOUBLE, mr);
        params.doubleDataMut()[0] = mu;
        params.doubleDataMut()[1] = sigma;
    } else {
        // Exponential, Poisson: MLE matches sample mean either way.
        Value mle_args[3] = { x, Value::fromString("distribution", mr),
                              Value::fromString(canonical, mr) };
        Value mle_out[1];
        mle_reg(Span<const Value>(mle_args, 3), 1,
                Span<Value>(mle_out, 1), ctx);
        params = std::move(mle_out[0]);
    }

    // Build the result struct.
    Value pd = Value::structure(mr);
    pd.field("DistributionName")  = Value::fromString(canonical, mr);
    pd.field("ParameterValues")   = std::move(params);
    Value names_cell = Value::cell(1, param_names.size(), mr);
    for (std::size_t i = 0; i < param_names.size(); ++i)
        names_cell.cellAt(i) = Value::fromString(param_names[i], mr);
    pd.field("ParameterNames")    = std::move(names_cell);
    pd.field("NumObservations")   = Value::scalar(static_cast<double>(N), mr);
    outs[0] = std::move(pd);
}

} // namespace detail

} // namespace numkit::stats
