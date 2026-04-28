// libs/stats/src/MStatsLibrary.cpp
//
// Registration hub for the Statistics Toolbox builtins. Mirrors the
// MSignalLibrary pattern.

#include <numkit/m/stats/MStatsLibrary.hpp>

#include <numkit/m/core/MTypes.hpp>

namespace numkit::m::stats::detail {
// moments/moments.cpp
void skewness_reg (Span<const MValue>, size_t, Span<MValue>, CallContext &);
void kurtosis_reg (Span<const MValue>, size_t, Span<MValue>, CallContext &);
// nan_aware/nan_aware.cpp
void nansum_reg   (Span<const MValue>, size_t, Span<MValue>, CallContext &);
void nanmean_reg  (Span<const MValue>, size_t, Span<MValue>, CallContext &);
void nanmedian_reg(Span<const MValue>, size_t, Span<MValue>, CallContext &);
void nanmax_reg   (Span<const MValue>, size_t, Span<MValue>, CallContext &);
void nanmin_reg   (Span<const MValue>, size_t, Span<MValue>, CallContext &);
void nanvar_reg   (Span<const MValue>, size_t, Span<MValue>, CallContext &);
void nanstd_reg   (Span<const MValue>, size_t, Span<MValue>, CallContext &);
} // namespace numkit::m::stats::detail

namespace numkit::m {

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

} // namespace numkit::m
