/// @file wfilters.hpp
/// @ingroup group_wavelet
// toolboxes/wavelet/include/numkit/wavelet/filter/wfilters.hpp

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>
#include <vector>

namespace numkit::wavelet {

/// Filter bank for an orthogonal wavelet family.
///
/// Holds the four canonical filters used by 1-D / 2-D DWT:
///   - `Lo_D` — analysis lowpass.
///   - `Hi_D` — analysis highpass.
///   - `Lo_R` — synthesis lowpass.
///   - `Hi_R` — synthesis highpass.
///
/// Relations:
/// @f$ \text{Lo\_R}[k] = \text{Lo\_D}[N-1-k] @f$ (time reversal),
/// @f$ \text{Hi\_R}[k] = (-1)^k\,\text{Lo\_D}[k] @f$ (QMF), and
/// @f$ \text{Hi\_D}[k] = \text{Hi\_R}[N-1-k] @f$.
struct FilterBank {
    std::vector<double> Lo_D;
    std::vector<double> Hi_D;
    std::vector<double> Lo_R;
    std::vector<double> Hi_R;
};

/// Return the four filter banks for a named orthogonal wavelet.
///
/// Recognised families: `"haar"` / `"db1"`, `"db2".."db10"`,
/// `"sym2".."sym10"`, `"coif1".."coif5"`. (More can be added without
/// touching dwt/idwt — they only consume the four returned vectors.)
///
/// @param name  Wavelet family name.
/// @return      @ref FilterBank with all four arrays populated.
/// @throws      Error on unsupported name.
///
/// @see wfilters
FilterBank wavelet_filters(const std::string &name);

/// Fill @p out with the four filters of a biorthogonal (`bior*`) or
/// reverse-biorthogonal (`rbio*`) family. Unlike the orthogonal families
/// these have distinct analysis/synthesis pairs, so all four are tabulated
/// independently (see filter/biorfilt.cpp). Used by @ref wavelet_filters.
///
/// @param name  Candidate family name (e.g. `"bior2.2"`, `"rbio4.4"`).
/// @param out   Filled with the four filter vectors on success.
/// @return      `true` if @p name is a known bior/rbio family, else `false`.
bool bior_filterbank(const std::string &name, FilterBank &out);

/// Result of @ref wfilters (`[…] = wfilters(wname[, kind])`).
///
/// Which fields are populated depends on `kind`:
///   - `""` / `"all"`: all four set.
///   - `"d"`:  Lo_D, Hi_D set; Lo_R, Hi_R empty.
///   - `"r"`:  Lo_R, Hi_R set; Lo_D, Hi_D empty.
///   - `"l"`:  Lo_D, Lo_R set; Hi_D, Hi_R empty.
///   - `"h"`:  Hi_D, Hi_R set; Lo_D, Lo_R empty.
struct WFiltersResult {
    Value Lo_D;
    Value Hi_D;
    Value Lo_R;
    Value Hi_R;
};

/// Build the named wavelet's filters as numkit Value rows.
///
/// `wfilters(wname[, kind])`. The `kind` selector:
///
/// | kind | populated fields  | call form                 |
/// | ---- | ----------------- | ------------------------- |
/// | `""` | all four          | `[LoD, HiD, LoR, HiR]`    |
/// | `"d"`| Lo_D, Hi_D        | `[LoD, HiD]`              |
/// | `"r"`| Lo_R, Hi_R        | `[LoR, HiR]`              |
/// | `"l"`| Lo_D, Lo_R        | `[LoD, LoR]`              |
/// | `"h"`| Hi_D, Hi_R        | `[HiD, HiR]`              |
///
/// @param name  Wavelet family name (see @ref wavelet_filters).
/// @param kind  Selector — `""`, `"d"`, `"r"`, `"l"`, or `"h"`.
/// @param mr    Memory resource (nullptr → process default).
/// @return      @ref WFiltersResult with the requested subset populated.
/// @throws      Error on unknown wavelet name or unknown kind.
///
/// @code
/// auto r = wfilters("db4");          // all four filters
/// auto r2 = wfilters("db4", "d");    // analysis only — Lo_D, Hi_D
/// @endcode
///
/// @see wavelet_filters
WFiltersResult wfilters(const std::string &name, const std::string &kind = "",
                        std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
