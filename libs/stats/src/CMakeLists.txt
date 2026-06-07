set(STATSLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/descriptive.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/descriptive_extras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/normalize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/tiedrank.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/corrcov.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/tabulate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/grp2idx.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/cholcov.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/crosstab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/grpstats.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/nearcorr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/partial_canon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/moments/moments.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/moments/moments_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/nan_aware/nan_aware.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/moving/moving.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/normal.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/normal_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/chi2.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/chi2_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/students_t.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/students_t_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/fisher_f.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/fisher_f_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/beta.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/beta_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/gamma_dist.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/gamma_dist_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/multivariate.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/multivariate_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/dist_fits.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/dist_fits_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/exponential.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/exponential_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/extreme_value.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/extreme_value_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/gev.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/gev_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/gp.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/gp_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/uniform.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/uniform_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/lognormal.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/lognormal_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/weibull.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/weibull_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/rayleigh.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/rayleigh_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/poisson.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/poisson_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/binomial.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/binomial_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/unid.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/unid_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/geometric.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/geometric_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/negbin.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/negbin_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/hypergeom.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/hypergeom_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/nakagami.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/nakagami_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/rician.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/rician_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/ncx2.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/ncx2_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/distance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/kmeans.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/kmedoids.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/linkage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/silhouette.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/knnsearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dim/pca.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/hypothesis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/ad_dw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resample/resample.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fit/fit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mvdist/mvdist.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/mvdist/mvdist_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/anova/anova.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/anova/multcompare.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/anova/multcompare_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/spline/spline.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spline/spline_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/qmc/qmc.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/qmc/qmc_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/regress/regress.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/regress/nlinfit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/regress/robust.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/regress/glm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/regress/lasso.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lda/lda.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/lda/lda_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/sampling/lhs.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/sampling/lhs_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/copulas/copulas.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/copulas/copulas_reg.cpp # register (CallContext)
)

# Backend-split functions: one definition per TU, picked at configure
# time via NUMKIT_WITH_SIMD. nan_reductions kernels feed nansum / nanmean
# in nan_aware.cpp.
if(NUMKIT_WITH_SIMD)
    list(APPEND STATSLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/nan_aware/backends/nan_reductions_simd.cpp)
else()
    list(APPEND STATSLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/nan_aware/backends/nan_reductions_portable.cpp)
endif()

set(STATSLIB_SOURCES ${STATSLIB_SOURCES} PARENT_SCOPE)
