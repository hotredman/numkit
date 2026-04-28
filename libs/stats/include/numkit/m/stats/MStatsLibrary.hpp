// libs/stats/include/numkit/m/stats/MStatsLibrary.hpp
//
// Statistics Toolbox builtins. Mirrors MATLAB's documentation root
// `/help/stats/`. Currently houses moments (skewness, kurtosis) and
// nan-aware reductions (nansum, nanmean, nanvar, nanstd, nanmedian,
// nanmax, nanmin). Descriptive statistics that base MATLAB ships
// (var/std/median/mode/quantile/prctile/cov/corrcoef) live in
// libs/builtin under data_analysis/.

#pragma once

#include <numkit/m/core/MEngine.hpp>

namespace numkit::m {

class StatsLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit::m
