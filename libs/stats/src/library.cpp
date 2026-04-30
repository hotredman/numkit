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

    reg("nan", "nansum",    &stats::detail::nansum_reg);
    reg("nan", "nanmean",   &stats::detail::nanmean_reg);
    reg("nan", "nanmedian", &stats::detail::nanmedian_reg);
    reg("nan", "nanmax",    &stats::detail::nanmax_reg);
    reg("nan", "nanmin",    &stats::detail::nanmin_reg);
    reg("nan", "nanvar",    &stats::detail::nanvar_reg);
    reg("nan", "nanstd",    &stats::detail::nanstd_reg);
}

} // namespace numkit
