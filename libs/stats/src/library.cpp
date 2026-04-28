// libs/stats/src/library.cpp
//
// Registration hub for the Statistics Toolbox builtins. Mirrors the
// MSignalLibrary pattern.

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
    engine.registerFunction("skewness",  &stats::detail::skewness_reg);
    engine.registerFunction("kurtosis",  &stats::detail::kurtosis_reg);
    engine.registerFunction("nansum",    &stats::detail::nansum_reg);
    engine.registerFunction("nanmean",   &stats::detail::nanmean_reg);
    engine.registerFunction("nanmedian", &stats::detail::nanmedian_reg);
    engine.registerFunction("nanmax",    &stats::detail::nanmax_reg);
    engine.registerFunction("nanmin",    &stats::detail::nanmin_reg);
    engine.registerFunction("nanvar",    &stats::detail::nanvar_reg);
    engine.registerFunction("nanstd",    &stats::detail::nanstd_reg);
}

} // namespace numkit
