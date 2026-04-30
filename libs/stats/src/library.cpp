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
