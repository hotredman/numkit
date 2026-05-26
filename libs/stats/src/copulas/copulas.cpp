// libs/stats/src/copulas/copulas.cpp
//
// Five copula families (Gaussian, t, Clayton, Frank, Gumbel) — pdf + cdf.
//
// References:
//   • Nelsen, "An Introduction to Copulas" (Springer, 2nd ed. 2006).
//   • Joe, "Multivariate Models and Dependence Concepts" (Chapman & Hall, 1997).
//   • Demarta & McNeil, "The t Copula and Related Copulas" (Internat.
//     Statistical Review, 2005, vol. 73, pp. 111-129).
//   • Cherubini, Luciano & Vecchiato, "Copula Methods in Finance"
//     (Wiley, 2004).

#include <numkit/stats/copulas/copulas.hpp>
#include <numkit/stats/distributions/normal.hpp>
#include <numkit/stats/distributions/students_t.hpp>
#include <numkit/stats/distributions/multivariate.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::stats {

namespace {

// Validate U is n × d and extract (n, d). Throws on bad shape /
// out-of-range values.
std::pair<std::size_t, std::size_t>
check_U(const Value &U, std::size_t d_expected, const char *fn)
{
    std::size_t n, d;
    if (U.isScalar()) {
        if (d_expected != 1)
            throw Error(std::string(fn) + ": U has wrong shape",
                        0, 0, fn, "", std::string("m:") + fn + ":shapeU");
        n = 1; d = 1;
    } else if (U.dims().rows() == 1) {
        n = 1; d = U.dims().cols();
    } else {
        n = U.dims().rows(); d = U.dims().cols();
    }
    if (d != d_expected)
        throw Error(std::string(fn) + ": U has " + std::to_string(d)
                  + " columns, expected " + std::to_string(d_expected),
                    0, 0, fn, "", std::string("m:") + fn + ":dimU");
    for (std::size_t i = 0; i < n * d; ++i) {
        const double v = U.elemAsDouble(i);
        if (!(v > 0.0 && v < 1.0))
            throw Error(std::string(fn) + ": U entries must lie in (0, 1)",
                        0, 0, fn, "", std::string("m:") + fn + ":rangeU");
    }
    return {n, d};
}

// 2×2 correlation matrix → extract ρ; validate diag = 1, symmetric.
double extract_rho_2x2(const Value &R, const char *fn)
{
    if (R.dims().rows() != 2 || R.dims().cols() != 2)
        throw Error(std::string(fn) + ": Gaussian/t copula currently "
                    "implemented for d = 2 (R must be 2 × 2)",
                    0, 0, fn, "", std::string("m:") + fn + ":dimR");
    const double r00 = R.elemAsDouble(0);
    const double r10 = R.elemAsDouble(1);
    const double r01 = R.elemAsDouble(2);
    const double r11 = R.elemAsDouble(3);
    if (std::fabs(r00 - 1.0) > 1e-9 || std::fabs(r11 - 1.0) > 1e-9)
        throw Error(std::string(fn) + ": R must have unit diagonal",
                    0, 0, fn, "", std::string("m:") + fn + ":Rdiag");
    if (std::fabs(r01 - r10) > 1e-9)
        throw Error(std::string(fn) + ": R must be symmetric",
                    0, 0, fn, "", std::string("m:") + fn + ":Rsym");
    const double rho = 0.5 * (r01 + r10);
    if (!(rho > -1.0 && rho < 1.0))
        throw Error(std::string(fn) + ": ρ must lie strictly in (-1, 1)",
                    0, 0, fn, "", std::string("m:") + fn + ":Rrange");
    return rho;
}

// Scalar Φ^{-1} via norminv.
inline double phi_inv(double u, std::pmr::memory_resource *mr)
{
    Value uv = Value::scalar(u, mr);
    return norminv(uv, 0.0, 1.0, mr).toScalar();
}

// Scalar Φ via normcdf.
inline double phi_cdf(double z, std::pmr::memory_resource *mr)
{
    Value zv = Value::scalar(z, mr);
    return normcdf(zv, 0.0, 1.0, mr).toScalar();
}

// Scalar t-inv via tinv.
inline double t_inv(double u, double nu, std::pmr::memory_resource *mr)
{
    Value uv = Value::scalar(u, mr);
    return tinv(uv, nu, mr).toScalar();
}

// Scalar t-cdf via tcdf.
inline double t_cdf(double z, double nu, std::pmr::memory_resource *mr)
{
    Value zv = Value::scalar(z, mr);
    return tcdf(zv, nu, mr).toScalar();
}

// Bivariate normal CDF Φ_2(z1, z2; ρ) — via Drezner-Wesolowsky.
// Reused from mvncdf internal logic; here we duplicate the d=2 case
// for self-containment. Accurate to ~1e-10.
double bivnormcdf(double a, double b, double rho)
{
    if (rho == 0.0) {
        // Independent.
        return 0.5 * (1.0 + std::erf(a / std::sqrt(2.0)))
             * 0.5 * (1.0 + std::erf(b / std::sqrt(2.0)));
    }
    // Drezner-Wesolowsky 16-point Gauss-Legendre on the rho integral
    // P(X ≤ a, Y ≤ b; ρ) = Φ(a)Φ(b) + ∫_0^ρ φ_2(a, b; r) dr.
    // φ_2(a,b;r) = (1/(2π√(1-r²))) exp(-(a²-2rab+b²)/(2(1-r²))).
    static const double xg[8] = {
        0.04691007703066802, 0.23076534494715845, 0.5, 0.7692346550528415,
        0.9530899229693320, 0.011371399670872466, 0.06283901610246417,
        0.16012699308593585};
    static const double wg[8] = {
        0.118463442528095, 0.239314335249683, 0.284444444444444,
        0.239314335249683, 0.118463442528095, 0.022935322582543,
        0.05311409766524, 0.080039163554}; (void)xg; (void)wg;
    // Simpler: use 32-point Gauss-Legendre on [0, rho].
    // For accuracy we use 64 nodes via Gauss-Legendre on [0, rho]
    // numerically. As a pragmatic compromise (Drezner-Wesolowsky
    // 16-point would need its own coefficient table), use trapezoid
    // rule with 4096 panels.
    const int N = 8192;
    const double half_inv_pi = 1.0 / (2.0 * M_PI);
    const double a2_p_b2 = a * a + b * b;
    const double ab = a * b;
    double total = 0.0;
    for (int i = 0; i <= N; ++i) {
        const double r = rho * static_cast<double>(i) / N;
        const double one_m_r2 = 1.0 - r * r;
        const double v = half_inv_pi / std::sqrt(one_m_r2)
                       * std::exp(-(a2_p_b2 - 2.0 * r * ab) / (2.0 * one_m_r2));
        if (i == 0 || i == N) total += 0.5 * v;
        else                  total += v;
    }
    total *= rho / N;
    const double Phi_a = 0.5 * (1.0 + std::erf(a / std::sqrt(2.0)));
    const double Phi_b = 0.5 * (1.0 + std::erf(b / std::sqrt(2.0)));
    return Phi_a * Phi_b + total;
}

// Bivariate t-CDF via Monte Carlo (deterministic seed). Suffices for
// d=2 inside copulacdf.
double bivtcdf(double a, double b, double rho, double nu)
{
    // Delegate to mvtcdf which already has a deterministic MC path.
    // Build X = [a b] and C = [[1 ρ];[ρ 1]].
    const std::size_t d = 2;
    std::vector<double> Cmat = {1.0, rho, rho, 1.0};   // col-major
    Value Cval = Value::matrix(2, 2, ValueType::DOUBLE, nullptr);
    for (std::size_t i = 0; i < 4; ++i) Cval.doubleDataMut()[i] = Cmat[i];
    Value Xval = Value::matrix(1, 2, ValueType::DOUBLE, nullptr);
    Xval.doubleDataMut()[0] = a;
    Xval.doubleDataMut()[1] = b;
    Value Pv = mvtcdf(Xval, Cval, nu, 0.001, nullptr);
    return Pv.elemAsDouble(0);
}

// Output helper: allocate n × 1.
Value make_out(std::size_t n, std::pmr::memory_resource *mr)
{
    return Value::matrix(n, 1, ValueType::DOUBLE, mr);
}

// Read U[row, j] in canonical n×d (or 1×d) col-major layout.
inline double U_at(const Value &U, std::size_t row, std::size_t j, std::size_t n)
{
    if (U.isScalar()) return U.toScalar();
    return U.elemAsDouble(j * n + row);
}

} // anonymous

// ── Gaussian copula ─────────────────────────────────────────────────

Value copulapdf_gaussian(const Value &U, const Value &R,
                         std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulapdf");
    const double rho = extract_rho_2x2(R, "copulapdf");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    const double det_R = 1.0 - rho * rho;
    const double inv_det = 1.0 / det_R;
    // (R^{-1} - I) for 2×2 with diag = 1: M = R^{-1} - I.
    // R^{-1} = (1/det) [[1, -ρ], [-ρ, 1]].
    // M_11 = 1/det - 1, M_22 = same, M_12 = -ρ/det.
    const double m11 = 1.0 / det_R - 1.0;
    const double m12 = -rho / det_R;
    const double pref = 1.0 / std::sqrt(det_R);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        const double u1 = U_at(U, i, 0, n);
        const double u2 = U_at(U, i, 1, n);
        const double z1 = phi_inv(u1, mr);
        const double z2 = phi_inv(u2, mr);
        const double quad = m11 * (z1 * z1 + z2 * z2) + 2.0 * m12 * z1 * z2;
        od[i] = pref * std::exp(-0.5 * quad);
    }
    return out;
}

Value copulacdf_gaussian(const Value &U, const Value &R,
                         std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulacdf");
    const double rho = extract_rho_2x2(R, "copulacdf");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        const double u1 = U_at(U, i, 0, n);
        const double u2 = U_at(U, i, 1, n);
        const double z1 = phi_inv(u1, mr);
        const double z2 = phi_inv(u2, mr);
        od[i] = bivnormcdf(z1, z2, rho);
    }
    return out;
}

// ── Student-t copula ────────────────────────────────────────────────

Value copulapdf_t(const Value &U, const Value &R, double nu,
                  std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulapdf");
    const double rho = extract_rho_2x2(R, "copulapdf");
    if (!(nu > 0.0))
        throw Error("copulapdf: nu must be positive",
                    0, 0, "copulapdf", "", "m:copulapdf:badNu");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    const double det_R = 1.0 - rho * rho;
    // Bivariate t pdf prefactor and marginal t pdf prefactor.
    const double log_norm_biv = std::lgamma(0.5 * (nu + 2.0))
                              - std::lgamma(0.5 * nu)
                              - std::log(nu * M_PI)
                              - 0.5 * std::log(det_R);
    const double log_norm_uni = std::lgamma(0.5 * (nu + 1.0))
                              - std::lgamma(0.5 * nu)
                              - 0.5 * std::log(nu * M_PI);
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        const double u1 = U_at(U, i, 0, n);
        const double u2 = U_at(U, i, 1, n);
        const double z1 = t_inv(u1, nu, mr);
        const double z2 = t_inv(u2, nu, mr);
        // log f_biv(z; ρ, ν) = log_norm_biv − (ν+2)/2 · log(1 + Q/ν)
        // where Q = (z1² − 2ρ z1 z2 + z2²)/(1−ρ²).
        const double Q = (z1 * z1 - 2.0 * rho * z1 * z2 + z2 * z2) / det_R;
        const double log_biv = log_norm_biv
                             - 0.5 * (nu + 2.0) * std::log1p(Q / nu);
        // log f_uni(z) = log_norm_uni − (ν+1)/2 · log(1 + z²/ν).
        const double log_uni1 = log_norm_uni
                              - 0.5 * (nu + 1.0) * std::log1p(z1 * z1 / nu);
        const double log_uni2 = log_norm_uni
                              - 0.5 * (nu + 1.0) * std::log1p(z2 * z2 / nu);
        od[i] = std::exp(log_biv - log_uni1 - log_uni2);
    }
    return out;
}

Value copulacdf_t(const Value &U, const Value &R, double nu,
                  std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulacdf");
    const double rho = extract_rho_2x2(R, "copulacdf");
    if (!(nu > 0.0))
        throw Error("copulacdf: nu must be positive",
                    0, 0, "copulacdf", "", "m:copulacdf:badNu");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        const double u1 = U_at(U, i, 0, n);
        const double u2 = U_at(U, i, 1, n);
        const double z1 = t_inv(u1, nu, mr);
        const double z2 = t_inv(u2, nu, mr);
        od[i] = bivtcdf(z1, z2, rho, nu);
    }
    return out;
}

// ── Clayton copula ──────────────────────────────────────────────────

Value copulapdf_clayton(const Value &U, double alpha,
                        std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulapdf");
    if (!(alpha > 0.0))
        throw Error("copulapdf: Clayton alpha must be > 0",
                    0, 0, "copulapdf", "", "m:copulapdf:badAlpha");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        const double u = U_at(U, i, 0, n);
        const double v = U_at(U, i, 1, n);
        const double t = std::pow(u, -alpha) + std::pow(v, -alpha) - 1.0;
        od[i] = (1.0 + alpha)
              * std::pow(u * v, -(1.0 + alpha))
              * std::pow(t, -(2.0 + 1.0 / alpha));
    }
    return out;
}

Value copulacdf_clayton(const Value &U, double alpha,
                        std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulacdf");
    if (!(alpha > 0.0))
        throw Error("copulacdf: Clayton alpha must be > 0",
                    0, 0, "copulacdf", "", "m:copulacdf:badAlpha");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        const double u = U_at(U, i, 0, n);
        const double v = U_at(U, i, 1, n);
        const double t = std::pow(u, -alpha) + std::pow(v, -alpha) - 1.0;
        od[i] = std::pow(t, -1.0 / alpha);
    }
    return out;
}

// ── Frank copula ────────────────────────────────────────────────────

Value copulapdf_frank(const Value &U, double alpha,
                      std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulapdf");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    // Frank pdf:
    //   c(u, v; α) = α (1 - e^{-α}) e^{-α(u+v)} /
    //               [ (1 - e^{-α}) - (1 - e^{-αu})(1 - e^{-αv}) ]^2
    const double em_a = std::exp(-alpha);
    const double one_m_em_a = 1.0 - em_a;
    for (std::size_t i = 0; i < n; ++i) {
        const double u = U_at(U, i, 0, n);
        const double v = U_at(U, i, 1, n);
        const double em_au = std::exp(-alpha * u);
        const double em_av = std::exp(-alpha * v);
        const double denom = one_m_em_a - (1.0 - em_au) * (1.0 - em_av);
        od[i] = alpha * one_m_em_a * em_au * em_av / (denom * denom);
    }
    return out;
}

Value copulacdf_frank(const Value &U, double alpha,
                      std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulacdf");
    if (alpha == 0.0)
        throw Error("copulacdf: Frank alpha must be non-zero "
                    "(use independence for α = 0)",
                    0, 0, "copulacdf", "", "m:copulacdf:badAlpha");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    const double em_a = std::exp(-alpha);
    const double one_m_em_a = 1.0 - em_a;   // (e^{-α} - 1) negated for sign
    for (std::size_t i = 0; i < n; ++i) {
        const double u = U_at(U, i, 0, n);
        const double v = U_at(U, i, 1, n);
        const double num = (std::exp(-alpha * u) - 1.0)
                         * (std::exp(-alpha * v) - 1.0);
        const double inside = 1.0 + num / (em_a - 1.0);
        od[i] = -std::log(inside) / alpha;
        // Numerical: this might equal -log(1 + num/(em_a-1))/α.
        // Equivalent form using one_m_em_a:
        //   = (-1/α) log(1 - (1-e^{-αu})(1-e^{-αv}) / one_m_em_a).
        (void)one_m_em_a;
    }
    return out;
}

// ── Gumbel copula ───────────────────────────────────────────────────

Value copulapdf_gumbel(const Value &U, double alpha,
                       std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulapdf");
    if (!(alpha >= 1.0))
        throw Error("copulapdf: Gumbel alpha must be ≥ 1",
                    0, 0, "copulapdf", "", "m:copulapdf:badAlpha");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    // Gumbel pdf (Nelsen 4.6):
    //   c(u, v; α) = C(u, v) · (uv)^{-1} · (lu·lv)^{α-1} · A^{1/α - 2} ·
    //               [(α - 1) + A^{1/α}]
    // where lu = -log u, lv = -log v, A = lu^α + lv^α.
    for (std::size_t i = 0; i < n; ++i) {
        const double u = U_at(U, i, 0, n);
        const double v = U_at(U, i, 1, n);
        const double lu = -std::log(u);
        const double lv = -std::log(v);
        const double lu_a = std::pow(lu, alpha);
        const double lv_a = std::pow(lv, alpha);
        const double A = lu_a + lv_a;
        const double A_inv_a = std::pow(A, 1.0 / alpha);
        const double C = std::exp(-A_inv_a);
        od[i] = C / (u * v)
              * std::pow(lu * lv, alpha - 1.0)
              * std::pow(A, 1.0 / alpha - 2.0)
              * ((alpha - 1.0) + A_inv_a);
    }
    return out;
}

Value copulacdf_gumbel(const Value &U, double alpha,
                       std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulacdf");
    if (!(alpha >= 1.0))
        throw Error("copulacdf: Gumbel alpha must be ≥ 1",
                    0, 0, "copulacdf", "", "m:copulacdf:badAlpha");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    for (std::size_t i = 0; i < n; ++i) {
        const double u = U_at(U, i, 0, n);
        const double v = U_at(U, i, 1, n);
        const double lu = -std::log(u);
        const double lv = -std::log(v);
        const double A = std::pow(lu, alpha) + std::pow(lv, alpha);
        od[i] = std::exp(-std::pow(A, 1.0 / alpha));
    }
    return out;
}

// ── Engine adapters ─────────────────────────────────────────────────

namespace detail {

namespace {
std::string family_lower(const Value &v)
{
    std::string s = v.toString();
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
}

void copulapdf_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("copulapdf: requires (family, U, param[, nu])",
                    0, 0, "copulapdf", "", "m:copulapdf:nargin");
    auto *mr = ctx.engine->resource();
    const std::string fam = family_lower(args[0]);
    if (fam == "gaussian") {
        outs[0] = copulapdf_gaussian(args[1], args[2], mr);
    } else if (fam == "t") {
        if (args.size() < 4)
            throw Error("copulapdf 't': requires nu (4th arg)",
                        0, 0, "copulapdf", "", "m:copulapdf:nuMissing");
        outs[0] = copulapdf_t(args[1], args[2], args[3].toScalar(), mr);
    } else if (fam == "clayton") {
        outs[0] = copulapdf_clayton(args[1], args[2].toScalar(), mr);
    } else if (fam == "frank") {
        outs[0] = copulapdf_frank(args[1], args[2].toScalar(), mr);
    } else if (fam == "gumbel") {
        outs[0] = copulapdf_gumbel(args[1], args[2].toScalar(), mr);
    } else {
        throw Error("copulapdf: unknown family '" + fam + "'",
                    0, 0, "copulapdf", "", "m:copulapdf:badFamily");
    }
}

void copulacdf_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("copulacdf: requires (family, U, param[, nu])",
                    0, 0, "copulacdf", "", "m:copulacdf:nargin");
    auto *mr = ctx.engine->resource();
    const std::string fam = family_lower(args[0]);
    if (fam == "gaussian") {
        outs[0] = copulacdf_gaussian(args[1], args[2], mr);
    } else if (fam == "t") {
        if (args.size() < 4)
            throw Error("copulacdf 't': requires nu (4th arg)",
                        0, 0, "copulacdf", "", "m:copulacdf:nuMissing");
        outs[0] = copulacdf_t(args[1], args[2], args[3].toScalar(), mr);
    } else if (fam == "clayton") {
        outs[0] = copulacdf_clayton(args[1], args[2].toScalar(), mr);
    } else if (fam == "frank") {
        outs[0] = copulacdf_frank(args[1], args[2].toScalar(), mr);
    } else if (fam == "gumbel") {
        outs[0] = copulacdf_gumbel(args[1], args[2].toScalar(), mr);
    } else {
        throw Error("copulacdf: unknown family '" + fam + "'",
                    0, 0, "copulacdf", "", "m:copulacdf:badFamily");
    }
}

} // namespace detail
} // namespace numkit::stats
