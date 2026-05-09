// libs/stats/src/library.cpp
//
// Registration hub for the Statistics Toolbox builtins.
// Namespace layout (NAMESPACE_DESIGN.md §5, §9.3):
//   moments/   → stats.descriptive.<fn>  (skewness, kurtosis, ...)
//   nan_aware/ → stats.nan.<fn>          (nansum, nanmean, ...)
// Each function is also aliased into `compat.<fn>` so MATLAB-style
// scripts can call them flat after `import compat.*`.

#include <numkit/stats/library.hpp>

#include <numkit/core/types.hpp>

namespace numkit::stats::detail {
// descriptive/descriptive.cpp (Phase 7b — moved from libs/builtin)
void var_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void std_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void median_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void quantile_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void prctile_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void mode_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void cov_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void corrcoef_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
// descriptive/descriptive_extras.cpp (B2)
void bounds_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void iqr_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void maxk_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void mink_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void rmse_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void mape_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void range_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void mad_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void geomean_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void harmmean_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void moment_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void trimmean_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void isoutlier_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void rmoutliers_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void fillmissing_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void rmmissing_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void standardizeMissing_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void corr_reg              (Span<const Value>, size_t, Span<Value>, CallContext &);
void detrend_reg           (Span<const Value>, size_t, Span<Value>, CallContext &);
void partialcorr_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void ecdf_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void datastats_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void ksdensity_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void prepareCurveData_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void prepareSurfaceData_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void ecdfhist_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void normalize_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void rescale_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void zscore_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
// moments/moments.cpp
void skewness_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void kurtosis_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
// nan_aware/nan_aware.cpp
void nansum_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void nanmean_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void nanmedian_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void nanmax_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void nanmin_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void nanvar_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void nanstd_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
// distributions/normal.cpp
void normpdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void normcdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void norminv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void normrnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void normstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/chi2.cpp
void chi2pdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void chi2cdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void chi2inv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void chi2rnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void chi2stat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/students_t.cpp
void tpdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void tcdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void tinv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void trnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void tstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/fisher_f.cpp
void fpdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void fcdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void finv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void frnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void fstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/beta.cpp
void betapdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void betacdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void betainv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void betarnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void betastat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/gamma_dist.cpp
void gampdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gamcdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gaminv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gamrnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gamstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/exponential.cpp
void exppdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void expcdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void expinv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void exprnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void expstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/extreme_value.cpp
void evpdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void evcdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void evinv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void evrnd_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void evstat_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/gev.cpp
void gevpdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gevcdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gevinv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gevrnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gevstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/gp.cpp
void gppdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void gpcdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void gpinv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void gprnd_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void gpstat_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/nakagami.cpp
void nakapdf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void nakacdf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void nakainv_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void nakarnd_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void nakastat_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/rician.cpp
void ricepdf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ricecdf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void riceinv_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ricernd_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ricestat_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/ncx2.cpp
void ncx2pdf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ncx2cdf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ncx2inv_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ncx2rnd_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ncx2stat_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/uniform.cpp
void unifpdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void unifcdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void unifinv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void unifrnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void unifstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/lognormal.cpp
void lognpdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void logncdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void logninv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void lognrnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void lognstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/weibull.cpp
void wblpdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void wblcdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void wblinv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void wblrnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void wblstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/rayleigh.cpp
void raylpdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void raylcdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void raylinv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void raylrnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void raylstat_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/poisson.cpp
void poisspdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void poisscdf_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void poissinv_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void poissrnd_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void poisstat_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/binomial.cpp
void binopdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void binocdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void binoinv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void binornd_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void binostat_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/unid.cpp
void unidpdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void unidcdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void unidinv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void unidrnd_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void unidstat_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/geometric.cpp
void geopdf_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void geocdf_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void geoinv_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void geornd_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void geostat_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/negbin.cpp
void nbinpdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void nbincdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void nbininv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void nbinrnd_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void nbinstat_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// distributions/hypergeom.cpp
void hygepdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void hygecdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void hygeinv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void hygernd_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void hygestat_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// moving/moving.cpp
void movmean_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void movsum_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void movmin_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void movmax_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void movprod_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void movmedian_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void movvar_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void movstd_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void movmad_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void smoothdata_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void hampel_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// cluster/distance.cpp
void pdist_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void pdist2_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void squareform_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void mahal_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);

// cluster/kmeans.cpp
void kmeans_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// cluster/kmedoids.cpp
void kmedoids_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void dbscan_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// cluster/linkage.cpp
void linkage_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void cluster_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void clusterdata_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void cophenet_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// cluster/silhouette.cpp
void silhouette_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// cluster/knnsearch.cpp
void knnsearch_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rangesearch_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void inconsistent_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

// dim/pca.cpp
void pca_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void pcacov_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void pcares_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// test/hypothesis.cpp
void ttest_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void ttest2_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void ztest_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void vartest_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void vartest2_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void kstest_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void kstest2_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void lillietest_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void jbtest_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void signtest_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void signrank_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ranksum_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void runstest_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void vartestn_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void chi2gof_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void fishertest_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

// resample/resample.cpp
void randsample_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void datasample_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void bootstrp_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void jackknife_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void combnk_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// fit/fit.cpp
void normfit_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void poissfit_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void expfit_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void unifit_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void normlike_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void explike_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void lognlike_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void gamlike_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void betalike_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void wbllike_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void evlike_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lognfit_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void binofit_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void raylfit_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gevlike_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void gplike_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// mvdist/mvdist.cpp
void mvnpdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void mnpdf_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void mvtpdf_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// anova/anova.cpp
void anova1_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void kruskalwallis_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void dummyvar_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);

// regress/regress.cpp
void regress_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void lscov_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void ridge_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// lda/lda.cpp
void classify_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// spline/spline.cpp
void aveknt_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void augknt_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void brk2knt_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void knt2brk_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ppmak_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fnval_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fnder_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fnint_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void csapi_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fnbrk_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fncmb_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// qmc/qmc.cpp
void haltonset_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void net_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::stats::detail

namespace numkit {

void StatsLibrary::install(Engine &engine)
{
    // Local helper: stats is a MATLAB-mirror library, every function
    // gets registered under stats.<sub>.<name> AND aliased into compat.<name>.
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("stats.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    reg("descriptive", "var",       &stats::detail::var_reg);
    reg("descriptive", "std",       &stats::detail::std_reg);
    reg("descriptive", "median",    &stats::detail::median_reg);
    reg("descriptive", "quantile",  &stats::detail::quantile_reg);
    reg("descriptive", "prctile",   &stats::detail::prctile_reg);
    reg("descriptive", "mode",      &stats::detail::mode_reg);
    reg("descriptive", "cov",       &stats::detail::cov_reg);
    reg("descriptive", "corrcoef",  &stats::detail::corrcoef_reg);
    reg("descriptive", "skewness",  &stats::detail::skewness_reg);
    reg("descriptive", "kurtosis",  &stats::detail::kurtosis_reg);
    reg("descriptive", "bounds",    &stats::detail::bounds_reg);
    reg("descriptive", "iqr",       &stats::detail::iqr_reg);
    reg("descriptive", "maxk",      &stats::detail::maxk_reg);
    reg("descriptive", "mink",      &stats::detail::mink_reg);
    reg("descriptive", "rmse",      &stats::detail::rmse_reg);
    reg("descriptive", "mape",      &stats::detail::mape_reg);
    reg("descriptive", "range",     &stats::detail::range_reg);
    reg("descriptive", "mad",       &stats::detail::mad_reg);
    reg("descriptive", "geomean",   &stats::detail::geomean_reg);
    reg("descriptive", "harmmean",  &stats::detail::harmmean_reg);
    reg("descriptive", "moment",    &stats::detail::moment_reg);
    reg("descriptive", "trimmean",  &stats::detail::trimmean_reg);
    reg("descriptive", "isoutlier",          &stats::detail::isoutlier_reg);
    reg("descriptive", "rmoutliers",         &stats::detail::rmoutliers_reg);
    reg("descriptive", "fillmissing",        &stats::detail::fillmissing_reg);
    reg("descriptive", "rmmissing",          &stats::detail::rmmissing_reg);
    reg("descriptive", "standardizeMissing", &stats::detail::standardizeMissing_reg);
    reg("descriptive", "corr",               &stats::detail::corr_reg);
    reg("descriptive", "detrend",            &stats::detail::detrend_reg);
    reg("descriptive", "partialcorr",        &stats::detail::partialcorr_reg);
    reg("descriptive", "ecdf",      &stats::detail::ecdf_reg);
    reg("descriptive", "datastats", &stats::detail::datastats_reg);
    reg("descriptive", "ksdensity", &stats::detail::ksdensity_reg);
    reg("descriptive", "prepareCurveData",   &stats::detail::prepareCurveData_reg);
    reg("descriptive", "prepareSurfaceData", &stats::detail::prepareSurfaceData_reg);
    reg("descriptive", "ecdfhist",  &stats::detail::ecdfhist_reg);
    reg("descriptive", "normalize", &stats::detail::normalize_reg);
    reg("descriptive", "rescale",   &stats::detail::rescale_reg);
    reg("descriptive", "zscore",    &stats::detail::zscore_reg);

    reg("dist", "normpdf",  &stats::detail::normpdf_reg);
    reg("dist", "normcdf",  &stats::detail::normcdf_reg);
    reg("dist", "norminv",  &stats::detail::norminv_reg);
    reg("dist", "normrnd",  &stats::detail::normrnd_reg);
    reg("dist", "normstat", &stats::detail::normstat_reg);

    reg("dist", "chi2pdf",  &stats::detail::chi2pdf_reg);
    reg("dist", "chi2cdf",  &stats::detail::chi2cdf_reg);
    reg("dist", "chi2inv",  &stats::detail::chi2inv_reg);
    reg("dist", "chi2rnd",  &stats::detail::chi2rnd_reg);
    reg("dist", "chi2stat", &stats::detail::chi2stat_reg);

    reg("dist", "tpdf",     &stats::detail::tpdf_reg);
    reg("dist", "tcdf",     &stats::detail::tcdf_reg);
    reg("dist", "tinv",     &stats::detail::tinv_reg);
    reg("dist", "trnd",     &stats::detail::trnd_reg);
    reg("dist", "tstat",    &stats::detail::tstat_reg);

    reg("dist", "fpdf",     &stats::detail::fpdf_reg);
    reg("dist", "fcdf",     &stats::detail::fcdf_reg);
    reg("dist", "finv",     &stats::detail::finv_reg);
    reg("dist", "frnd",     &stats::detail::frnd_reg);
    reg("dist", "fstat",    &stats::detail::fstat_reg);

    reg("dist", "betapdf",  &stats::detail::betapdf_reg);
    reg("dist", "betacdf",  &stats::detail::betacdf_reg);
    reg("dist", "betainv",  &stats::detail::betainv_reg);
    reg("dist", "betarnd",  &stats::detail::betarnd_reg);
    reg("dist", "betastat", &stats::detail::betastat_reg);

    reg("dist", "gampdf",   &stats::detail::gampdf_reg);
    reg("dist", "gamcdf",   &stats::detail::gamcdf_reg);
    reg("dist", "gaminv",   &stats::detail::gaminv_reg);
    reg("dist", "gamrnd",   &stats::detail::gamrnd_reg);
    reg("dist", "gamstat",  &stats::detail::gamstat_reg);

    reg("dist", "exppdf",   &stats::detail::exppdf_reg);
    reg("dist", "expcdf",   &stats::detail::expcdf_reg);
    reg("dist", "expinv",   &stats::detail::expinv_reg);
    reg("dist", "exprnd",   &stats::detail::exprnd_reg);
    reg("dist", "expstat",  &stats::detail::expstat_reg);

    reg("dist", "evpdf",    &stats::detail::evpdf_reg);
    reg("dist", "evcdf",    &stats::detail::evcdf_reg);
    reg("dist", "evinv",    &stats::detail::evinv_reg);
    reg("dist", "evrnd",    &stats::detail::evrnd_reg);
    reg("dist", "evstat",   &stats::detail::evstat_reg);

    reg("dist", "gevpdf",   &stats::detail::gevpdf_reg);
    reg("dist", "gevcdf",   &stats::detail::gevcdf_reg);
    reg("dist", "gevinv",   &stats::detail::gevinv_reg);
    reg("dist", "gevrnd",   &stats::detail::gevrnd_reg);
    reg("dist", "gevstat",  &stats::detail::gevstat_reg);

    reg("dist", "gppdf",    &stats::detail::gppdf_reg);
    reg("dist", "gpcdf",    &stats::detail::gpcdf_reg);
    reg("dist", "gpinv",    &stats::detail::gpinv_reg);
    reg("dist", "gprnd",    &stats::detail::gprnd_reg);
    reg("dist", "gpstat",   &stats::detail::gpstat_reg);

    reg("dist", "nakapdf",  &stats::detail::nakapdf_reg);
    reg("dist", "nakacdf",  &stats::detail::nakacdf_reg);
    reg("dist", "nakainv",  &stats::detail::nakainv_reg);
    reg("dist", "nakarnd",  &stats::detail::nakarnd_reg);
    reg("dist", "nakastat", &stats::detail::nakastat_reg);

    reg("dist", "ricepdf",  &stats::detail::ricepdf_reg);
    reg("dist", "ricecdf",  &stats::detail::ricecdf_reg);
    reg("dist", "riceinv",  &stats::detail::riceinv_reg);
    reg("dist", "ricernd",  &stats::detail::ricernd_reg);
    reg("dist", "ricestat", &stats::detail::ricestat_reg);

    reg("dist", "ncx2pdf",  &stats::detail::ncx2pdf_reg);
    reg("dist", "ncx2cdf",  &stats::detail::ncx2cdf_reg);
    reg("dist", "ncx2inv",  &stats::detail::ncx2inv_reg);
    reg("dist", "ncx2rnd",  &stats::detail::ncx2rnd_reg);
    reg("dist", "ncx2stat", &stats::detail::ncx2stat_reg);

    reg("dist", "unifpdf",  &stats::detail::unifpdf_reg);
    reg("dist", "unifcdf",  &stats::detail::unifcdf_reg);
    reg("dist", "unifinv",  &stats::detail::unifinv_reg);
    reg("dist", "unifrnd",  &stats::detail::unifrnd_reg);
    reg("dist", "unifstat", &stats::detail::unifstat_reg);

    reg("dist", "lognpdf",  &stats::detail::lognpdf_reg);
    reg("dist", "logncdf",  &stats::detail::logncdf_reg);
    reg("dist", "logninv",  &stats::detail::logninv_reg);
    reg("dist", "lognrnd",  &stats::detail::lognrnd_reg);
    reg("dist", "lognstat", &stats::detail::lognstat_reg);

    reg("dist", "wblpdf",   &stats::detail::wblpdf_reg);
    reg("dist", "wblcdf",   &stats::detail::wblcdf_reg);
    reg("dist", "wblinv",   &stats::detail::wblinv_reg);
    reg("dist", "wblrnd",   &stats::detail::wblrnd_reg);
    reg("dist", "wblstat",  &stats::detail::wblstat_reg);

    reg("dist", "raylpdf",  &stats::detail::raylpdf_reg);
    reg("dist", "raylcdf",  &stats::detail::raylcdf_reg);
    reg("dist", "raylinv",  &stats::detail::raylinv_reg);
    reg("dist", "raylrnd",  &stats::detail::raylrnd_reg);
    reg("dist", "raylstat", &stats::detail::raylstat_reg);

    reg("dist", "poisspdf", &stats::detail::poisspdf_reg);
    reg("dist", "poisscdf", &stats::detail::poisscdf_reg);
    reg("dist", "poissinv", &stats::detail::poissinv_reg);
    reg("dist", "poissrnd", &stats::detail::poissrnd_reg);
    reg("dist", "poisstat", &stats::detail::poisstat_reg);

    reg("dist", "binopdf",  &stats::detail::binopdf_reg);
    reg("dist", "binocdf",  &stats::detail::binocdf_reg);
    reg("dist", "binoinv",  &stats::detail::binoinv_reg);
    reg("dist", "binornd",  &stats::detail::binornd_reg);
    reg("dist", "binostat", &stats::detail::binostat_reg);

    reg("dist", "unidpdf",  &stats::detail::unidpdf_reg);
    reg("dist", "unidcdf",  &stats::detail::unidcdf_reg);
    reg("dist", "unidinv",  &stats::detail::unidinv_reg);
    reg("dist", "unidrnd",  &stats::detail::unidrnd_reg);
    reg("dist", "unidstat", &stats::detail::unidstat_reg);

    reg("dist", "geopdf",   &stats::detail::geopdf_reg);
    reg("dist", "geocdf",   &stats::detail::geocdf_reg);
    reg("dist", "geoinv",   &stats::detail::geoinv_reg);
    reg("dist", "geornd",   &stats::detail::geornd_reg);
    reg("dist", "geostat",  &stats::detail::geostat_reg);

    reg("dist", "nbinpdf",  &stats::detail::nbinpdf_reg);
    reg("dist", "nbincdf",  &stats::detail::nbincdf_reg);
    reg("dist", "nbininv",  &stats::detail::nbininv_reg);
    reg("dist", "nbinrnd",  &stats::detail::nbinrnd_reg);
    reg("dist", "nbinstat", &stats::detail::nbinstat_reg);

    reg("dist", "hygepdf",  &stats::detail::hygepdf_reg);
    reg("dist", "hygecdf",  &stats::detail::hygecdf_reg);
    reg("dist", "hygeinv",  &stats::detail::hygeinv_reg);
    reg("dist", "hygernd",  &stats::detail::hygernd_reg);
    reg("dist", "hygestat", &stats::detail::hygestat_reg);

    reg("nan", "nansum",    &stats::detail::nansum_reg);
    reg("nan", "nanmean",   &stats::detail::nanmean_reg);
    reg("nan", "nanmedian", &stats::detail::nanmedian_reg);
    reg("nan", "nanmax",    &stats::detail::nanmax_reg);
    reg("nan", "nanmin",    &stats::detail::nanmin_reg);
    reg("nan", "nanvar",    &stats::detail::nanvar_reg);
    reg("nan", "nanstd",    &stats::detail::nanstd_reg);

    reg("moving", "movmean",    &stats::detail::movmean_reg);
    reg("moving", "movsum",     &stats::detail::movsum_reg);
    reg("moving", "movmin",     &stats::detail::movmin_reg);
    reg("moving", "movmax",     &stats::detail::movmax_reg);
    reg("moving", "movprod",    &stats::detail::movprod_reg);
    reg("moving", "movmedian",  &stats::detail::movmedian_reg);
    reg("moving", "movvar",     &stats::detail::movvar_reg);
    reg("moving", "movstd",     &stats::detail::movstd_reg);
    reg("moving", "movmad",     &stats::detail::movmad_reg);
    reg("moving", "smoothdata", &stats::detail::smoothdata_reg);
    reg("moving", "hampel",     &stats::detail::hampel_reg);

    reg("cluster", "pdist",      &stats::detail::pdist_reg);
    reg("cluster", "pdist2",     &stats::detail::pdist2_reg);
    reg("cluster", "squareform", &stats::detail::squareform_reg);
    reg("cluster", "mahal",      &stats::detail::mahal_reg);
    reg("cluster", "kmeans",     &stats::detail::kmeans_reg);
    reg("cluster", "kmedoids",   &stats::detail::kmedoids_reg);
    reg("cluster", "dbscan",     &stats::detail::dbscan_reg);
    reg("cluster", "linkage",    &stats::detail::linkage_reg);
    reg("cluster", "cluster",    &stats::detail::cluster_reg);
    reg("cluster", "clusterdata",&stats::detail::clusterdata_reg);
    reg("cluster", "cophenet",   &stats::detail::cophenet_reg);
    reg("cluster", "inconsistent", &stats::detail::inconsistent_reg);
    reg("cluster", "silhouette", &stats::detail::silhouette_reg);
    reg("cluster", "knnsearch",  &stats::detail::knnsearch_reg);
    reg("cluster", "rangesearch",&stats::detail::rangesearch_reg);

    reg("dim", "pca",     &stats::detail::pca_reg);
    reg("dim", "pcacov",  &stats::detail::pcacov_reg);
    reg("dim", "pcares",  &stats::detail::pcares_reg);

    reg("test", "ttest",    &stats::detail::ttest_reg);
    reg("test", "ttest2",   &stats::detail::ttest2_reg);
    reg("test", "ztest",    &stats::detail::ztest_reg);
    reg("test", "vartest",  &stats::detail::vartest_reg);
    reg("test", "vartest2", &stats::detail::vartest2_reg);
    reg("test", "kstest",     &stats::detail::kstest_reg);
    reg("test", "kstest2",    &stats::detail::kstest2_reg);
    reg("test", "lillietest", &stats::detail::lillietest_reg);
    reg("test", "jbtest",   &stats::detail::jbtest_reg);
    reg("test", "signtest", &stats::detail::signtest_reg);
    reg("test", "signrank", &stats::detail::signrank_reg);
    reg("test", "ranksum",  &stats::detail::ranksum_reg);
    reg("test", "runstest", &stats::detail::runstest_reg);
    reg("test", "vartestn", &stats::detail::vartestn_reg);
    reg("test", "chi2gof",    &stats::detail::chi2gof_reg);
    reg("test", "fishertest", &stats::detail::fishertest_reg);

    reg("resample", "randsample", &stats::detail::randsample_reg);
    reg("resample", "datasample", &stats::detail::datasample_reg);
    reg("resample", "bootstrp",   &stats::detail::bootstrp_reg);
    reg("resample", "jackknife",  &stats::detail::jackknife_reg);
    reg("resample", "combnk",     &stats::detail::combnk_reg);

    reg("fit", "normfit",  &stats::detail::normfit_reg);
    reg("fit", "poissfit", &stats::detail::poissfit_reg);
    reg("fit", "expfit",   &stats::detail::expfit_reg);
    reg("fit", "unifit",   &stats::detail::unifit_reg);
    reg("fit", "normlike", &stats::detail::normlike_reg);
    reg("fit", "explike",  &stats::detail::explike_reg);
    reg("fit", "lognlike", &stats::detail::lognlike_reg);
    reg("fit", "gamlike",  &stats::detail::gamlike_reg);
    reg("fit", "betalike", &stats::detail::betalike_reg);
    reg("fit", "wbllike",  &stats::detail::wbllike_reg);
    reg("fit", "evlike",   &stats::detail::evlike_reg);
    reg("fit", "lognfit",  &stats::detail::lognfit_reg);
    reg("fit", "binofit",  &stats::detail::binofit_reg);
    reg("fit", "raylfit",  &stats::detail::raylfit_reg);
    reg("fit", "gevlike",  &stats::detail::gevlike_reg);
    reg("fit", "gplike",   &stats::detail::gplike_reg);

    reg("mvdist", "mvnpdf", &stats::detail::mvnpdf_reg);
    reg("mvdist", "mnpdf",  &stats::detail::mnpdf_reg);
    reg("mvdist", "mvtpdf", &stats::detail::mvtpdf_reg);

    reg("anova", "anova1",        &stats::detail::anova1_reg);
    reg("anova", "kruskalwallis", &stats::detail::kruskalwallis_reg);
    reg("anova", "dummyvar",      &stats::detail::dummyvar_reg);

    reg("regress", "regress", &stats::detail::regress_reg);
    reg("regress", "lscov",   &stats::detail::lscov_reg);
    reg("regress", "ridge",   &stats::detail::ridge_reg);

    reg("lda", "classify", &stats::detail::classify_reg);

    reg("descriptive", "aveknt",  &stats::detail::aveknt_reg);
    reg("descriptive", "augknt",  &stats::detail::augknt_reg);
    reg("descriptive", "brk2knt", &stats::detail::brk2knt_reg);
    reg("descriptive", "knt2brk", &stats::detail::knt2brk_reg);
    reg("descriptive", "ppmak",   &stats::detail::ppmak_reg);
    reg("descriptive", "fnval",   &stats::detail::fnval_reg);
    reg("descriptive", "fnder",   &stats::detail::fnder_reg);
    reg("descriptive", "fnint",   &stats::detail::fnint_reg);
    reg("descriptive", "csapi",   &stats::detail::csapi_reg);
    reg("descriptive", "fnbrk",   &stats::detail::fnbrk_reg);
    reg("descriptive", "fncmb",   &stats::detail::fncmb_reg);

    reg("qmc", "haltonset", &stats::detail::haltonset_reg);
    reg("qmc", "net",       &stats::detail::net_reg);
}

} // namespace numkit
