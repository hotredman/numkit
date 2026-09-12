/// @file library.hpp
/// @ingroup group_stats
// toolboxes/stats/include/nk/stats/library.hpp
//
// Statistics builtins. Currently houses moments (skewness, kurtosis) and
// nan-aware reductions (nansum, nanmean, nanvar, nanstd, nanmedian,
// nanmax, nanmin). The common descriptive statistics
// (var/std/median/mode/quantile/prctile/cov/corrcoef) live in
// toolboxes/builtin under data_analysis/.

#pragma once

namespace nk {

/// @addtogroup group_stats
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace nk {

class StatsLibrary
{
public:
    static void install(Engine &engine);
};


/// @}
} // namespace nk
