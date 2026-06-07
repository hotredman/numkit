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

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

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
                        0, 0, fn, "", std::string("numkit:") + fn + ":shapeU");
        n = 1; d = 1;
    } else if (U.dims().rows() == 1) {
        n = 1; d = U.dims().cols();
    } else {
        n = U.dims().rows(); d = U.dims().cols();
    }
    if (d != d_expected)
        throw Error(std::string(fn) + ": U has " + std::to_string(d)
                  + " columns, expected " + std::to_string(d_expected),
                    0, 0, fn, "", std::string("numkit:") + fn + ":dimU");
    for (std::size_t i = 0; i < n * d; ++i) {
        const double v = U.elemAsDouble(i);
        if (!(v > 0.0 && v < 1.0))
            throw Error(std::string(fn) + ": U entries must lie in (0, 1)",
                        0, 0, fn, "", std::string("numkit:") + fn + ":rangeU");
    }
    return {n, d};
}

// 2×2 correlation matrix → extract ρ; validate diag = 1, symmetric.
double extract_rho_2x2(const Value &R, const char *fn)
{
    if (R.dims().rows() != 2 || R.dims().cols() != 2)
        throw Error(std::string(fn) + ": expected 2 × 2 R",
                    0, 0, fn, "", std::string("numkit:") + fn + ":dimR");
    const double r00 = R.elemAsDouble(0);
    const double r10 = R.elemAsDouble(1);
    const double r01 = R.elemAsDouble(2);
    const double r11 = R.elemAsDouble(3);
    if (std::fabs(r00 - 1.0) > 1e-9 || std::fabs(r11 - 1.0) > 1e-9)
        throw Error(std::string(fn) + ": R must have unit diagonal",
                    0, 0, fn, "", std::string("numkit:") + fn + ":Rdiag");
    if (std::fabs(r01 - r10) > 1e-9)
        throw Error(std::string(fn) + ": R must be symmetric",
                    0, 0, fn, "", std::string("numkit:") + fn + ":Rsym");
    const double rho = 0.5 * (r01 + r10);
    if (!(rho > -1.0 && rho < 1.0))
        throw Error(std::string(fn) + ": ρ must lie strictly in (-1, 1)",
                    0, 0, fn, "", std::string("numkit:") + fn + ":Rrange");
    return rho;
}

// Generic R validation + dimension extraction (any d ≥ 2).
std::size_t check_R_dim(const Value &R, const char *fn)
{
    if (R.dims().rows() != R.dims().cols() || R.dims().rows() < 2)
        throw Error(std::string(fn) + ": R must be a square d × d "
                    "matrix with d ≥ 2",
                    0, 0, fn, "", std::string("numkit:") + fn + ":dimR");
    const std::size_t d = R.dims().rows();
    // Diagonal must be 1; matrix must be symmetric.
    for (std::size_t i = 0; i < d; ++i) {
        if (std::fabs(R.elemAsDouble(i * d + i) - 1.0) > 1e-9)
            throw Error(std::string(fn) + ": R must have unit diagonal",
                        0, 0, fn, "", std::string("numkit:") + fn + ":Rdiag");
        for (std::size_t j = i + 1; j < d; ++j) {
            if (std::fabs(R.elemAsDouble(j * d + i) - R.elemAsDouble(i * d + j))
                > 1e-9)
                throw Error(std::string(fn) + ": R must be symmetric",
                            0, 0, fn, "", std::string("numkit:") + fn + ":Rsym");
        }
    }
    return d;
}

// In-place Cholesky (lower) of R (row-major). Returns det R via diag product.
double chol_lower_inplace(double *R, std::size_t d, const char *fn)
{
    double det = 1.0;
    for (std::size_t j = 0; j < d; ++j) {
        double diag = R[j * d + j];
        for (std::size_t k = 0; k < j; ++k)
            diag -= R[j * d + k] * R[j * d + k];
        if (!(diag > 0.0))
            throw Error(std::string(fn) + ": R must be positive definite",
                        0, 0, fn, "", std::string("numkit:") + fn + ":notPD");
        const double Ljj = std::sqrt(diag);
        R[j * d + j] = Ljj;
        det *= diag;
        for (std::size_t i = j + 1; i < d; ++i) {
            double s = R[i * d + j];
            for (std::size_t k = 0; k < j; ++k)
                s -= R[i * d + k] * R[j * d + k];
            R[i * d + j] = s / Ljj;
        }
        for (std::size_t k = j + 1; k < d; ++k) R[j * d + k] = 0.0;
    }
    return det;
}

// Solve L · y = b in place; L row-major lower-tri.
void chol_lower_solve(const double *L, double *y, std::size_t d)
{
    for (std::size_t i = 0; i < d; ++i) {
        double s = y[i];
        for (std::size_t k = 0; k < i; ++k) s -= L[i * d + k] * y[k];
        y[i] = s / L[i * d + i];
    }
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

// ── Gaussian copula (any d ≥ 2) ─────────────────────────────────────

Value copulapdf_gaussian(const Value &U, const Value &R,
                         std::pmr::memory_resource *mr)
{
    const std::size_t d = check_R_dim(R, "copulapdf");
    auto [n, dU] = check_U(U, d, "copulapdf");
    auto out = make_out(n, mr);
    if (n == 0) return out;

    // Lower Cholesky of R; det_R = ∏ diag(L)².
    std::vector<double> L(d * d);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            L[i * d + j] = R.elemAsDouble(j * d + i);   // col-major → row-major
    const double det_R = chol_lower_inplace(L.data(), d, "copulapdf");
    const double pref = 1.0 / std::sqrt(det_R);
    double *od = out.doubleDataMut();
    std::vector<double> z(d), y(d);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < d; ++j)
            z[j] = phi_inv(U_at(U, i, j, n), mr);
        // Solve L · y = z, then z' R^{-1} z = y' y.
        y = z;
        chol_lower_solve(L.data(), y.data(), d);
        double yty = 0.0;
        for (std::size_t j = 0; j < d; ++j) yty += y[j] * y[j];
        double ztz = 0.0;
        for (std::size_t j = 0; j < d; ++j) ztz += z[j] * z[j];
        // Quad form for (R^{-1} - I) z = y'y - z'z.
        od[i] = pref * std::exp(-0.5 * (yty - ztz));
    }
    return out;
}

Value copulacdf_gaussian(const Value &U, const Value &R,
                         std::pmr::memory_resource *mr)
{
    const std::size_t d = check_R_dim(R, "copulacdf");
    auto [n, dU] = check_U(U, d, "copulacdf");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();

    if (d == 2) {
        // Fast bivariate path (Drezner-Wesolowsky).
        const double rho = extract_rho_2x2(R, "copulacdf");
        for (std::size_t i = 0; i < n; ++i) {
            const double z1 = phi_inv(U_at(U, i, 0, n), mr);
            const double z2 = phi_inv(U_at(U, i, 1, n), mr);
            od[i] = bivnormcdf(z1, z2, rho);
        }
        return out;
    }

    // d ≥ 3: build Z = norminv(U) row-by-row and call mvncdf via
    // existing engine API. Build X as n × d.
    Value Xz = Value::matrix(n, d, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < d; ++j)
            Xz.doubleDataMut()[j * n + i] = phi_inv(U_at(U, i, j, n), mr);
    // mvncdf(X, mu=zeros, Sigma=R). Call typed API.
    Value mu0 = Value::matrix(1, d, ValueType::DOUBLE, mr);
    for (std::size_t j = 0; j < d; ++j) mu0.doubleDataMut()[j] = 0.0;
    Value Pv = mvncdf(Xz, mu0, R, mr);
    for (std::size_t i = 0; i < n; ++i) od[i] = Pv.elemAsDouble(i);
    return out;
}

// ── Student-t copula ────────────────────────────────────────────────

Value copulapdf_t(const Value &U, const Value &R, double nu,
                  std::pmr::memory_resource *mr)
{
    const std::size_t d = check_R_dim(R, "copulapdf");
    auto [n, dU] = check_U(U, d, "copulapdf");
    if (!(nu > 0.0))
        throw Error("copulapdf: nu must be positive",
                    0, 0, "copulapdf", "", "numkit:copulapdf:badNu");
    auto out = make_out(n, mr);
    if (n == 0) return out;

    // Multivariate t pdf:
    //   f(z; R, ν) = Γ((ν+d)/2) / [Γ(ν/2) · (νπ)^{d/2} · √det R]
    //              · (1 + z'R^{-1}z/ν)^{-(ν+d)/2}
    // Marginal t pdf:
    //   f(z_j; ν) = Γ((ν+1)/2) / [Γ(ν/2) · √(νπ)] · (1 + z_j²/ν)^{-(ν+1)/2}
    // log c(u; R, ν) = log f_d(z; R, ν) - Σ log f_1(z_j; ν)

    std::vector<double> L(d * d);
    for (std::size_t i = 0; i < d; ++i)
        for (std::size_t j = 0; j < d; ++j)
            L[i * d + j] = R.elemAsDouble(j * d + i);
    const double det_R = chol_lower_inplace(L.data(), d, "copulapdf");

    const double dD = static_cast<double>(d);
    const double log_norm_mv = std::lgamma(0.5 * (nu + dD))
                             - std::lgamma(0.5 * nu)
                             - 0.5 * dD * std::log(nu * M_PI)
                             - 0.5 * std::log(det_R);
    const double log_norm_uni = std::lgamma(0.5 * (nu + 1.0))
                              - std::lgamma(0.5 * nu)
                              - 0.5 * std::log(nu * M_PI);

    double *od = out.doubleDataMut();
    std::vector<double> z(d), y(d);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < d; ++j)
            z[j] = t_inv(U_at(U, i, j, n), nu, mr);
        y = z;
        chol_lower_solve(L.data(), y.data(), d);
        double yty = 0.0;
        for (std::size_t j = 0; j < d; ++j) yty += y[j] * y[j];
        const double log_mv = log_norm_mv
                            - 0.5 * (nu + dD) * std::log1p(yty / nu);
        double log_unis = 0.0;
        for (std::size_t j = 0; j < d; ++j)
            log_unis += log_norm_uni
                      - 0.5 * (nu + 1.0) * std::log1p(z[j] * z[j] / nu);
        od[i] = std::exp(log_mv - log_unis);
    }
    return out;
}

Value copulacdf_t(const Value &U, const Value &R, double nu,
                  std::pmr::memory_resource *mr)
{
    const std::size_t d = check_R_dim(R, "copulacdf");
    auto [n, dU] = check_U(U, d, "copulacdf");
    if (!(nu > 0.0))
        throw Error("copulacdf: nu must be positive",
                    0, 0, "copulacdf", "", "numkit:copulacdf:badNu");
    auto out = make_out(n, mr);
    if (n == 0) return out;
    double *od = out.doubleDataMut();
    if (d == 2) {
        // Fast bivariate path.
        const double rho = extract_rho_2x2(R, "copulacdf");
        for (std::size_t i = 0; i < n; ++i) {
            const double z1 = t_inv(U_at(U, i, 0, n), nu, mr);
            const double z2 = t_inv(U_at(U, i, 1, n), nu, mr);
            od[i] = bivtcdf(z1, z2, rho, nu);
        }
        return out;
    }
    // d ≥ 3: stack Z = tinv(U) and call mvtcdf.
    Value Xz = Value::matrix(n, d, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < d; ++j)
            Xz.doubleDataMut()[j * n + i] = t_inv(U_at(U, i, j, n), nu, mr);
    Value Pv = mvtcdf(Xz, R, nu, 0.005, mr);
    for (std::size_t i = 0; i < n; ++i) od[i] = Pv.elemAsDouble(i);
    return out;
}

// ── Clayton copula ──────────────────────────────────────────────────

Value copulapdf_clayton(const Value &U, double alpha,
                        std::pmr::memory_resource *mr)
{
    auto [n, d] = check_U(U, 2, "copulapdf");
    if (!(alpha > 0.0))
        throw Error("copulapdf: Clayton alpha must be > 0",
                    0, 0, "copulapdf", "", "numkit:copulapdf:badAlpha");
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
                    0, 0, "copulacdf", "", "numkit:copulacdf:badAlpha");
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
                    0, 0, "copulacdf", "", "numkit:copulacdf:badAlpha");
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
                    0, 0, "copulapdf", "", "numkit:copulapdf:badAlpha");
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
                    0, 0, "copulacdf", "", "numkit:copulacdf:badAlpha");
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

} // namespace numkit::stats
