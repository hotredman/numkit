// toolboxes/stats/include/numkit/stats/library.hpp
//
// Statistics builtins. Currently houses moments (skewness, kurtosis) and
// nan-aware reductions (nansum, nanmean, nanvar, nanstd, nanmedian,
// nanmax, nanmin). The common descriptive statistics
// (var/std/median/mode/quantile/prctile/cov/corrcoef) live in
// toolboxes/builtin under data_analysis/.

#pragma once

namespace numkit { class Engine; }  // fwd-decl — keep this public header core-free

namespace numkit {

class StatsLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
