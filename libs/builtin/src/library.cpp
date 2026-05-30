#include <numkit/builtin/library.hpp>
#include <numkit/builtin/language/operators/binary_ops.hpp>
#include <numkit/builtin/language/types/types.hpp>
#include <numkit/builtin/math/arithmetic/rounding.hpp>

#include <numkit/core/build_info.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value_type.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <thread>
#include <iomanip>
#include <random>
#include <regex>
#include <set>
#include <sstream>

namespace numkit::builtin::detail {
// Forward declarations for Phase 6c public-API-backed adapters.
// Each is defined in the corresponding source file under the section
// path indicated in the comment header.

// math/elementary/
void sqrt_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void abs_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cos_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acos_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atan2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sinh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cosh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tanh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asinh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acosh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atanh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sind_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cosd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asind_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acosd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void atan2d_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sinpi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cospi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void csc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sech_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void csch_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void coth_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void secd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cscd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cotd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acsc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asech_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acsch_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acoth_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void asecd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acscd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void acotd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cart2pol_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pol2cart_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cart2sph_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sph2cart_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void exp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log10_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void floor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ceil_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void round_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fix_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mod_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rem_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sign_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void subplus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void max_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void min_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sum_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void prod_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mean_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// var/std/median/quantile/prctile/mode/cov/corrcoef + skewness/kurtosis
// all live in libs/stats now (Phase 7b — Statistics Toolbox content
// per MATLAB taxonomy). Their registrations are in StatsLibrary::install.
void primes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isprime_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void factor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void perms_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void factorial_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nchoosek_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gradient_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cumtrapz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// interpolation/interp.cpp + math/elementary/polynomials.cpp
//   + math/integration/integration.cpp (trapz)
void interp1_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void interp2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void interp3_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void interpn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void inpolygon_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void convhull_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyarea_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void boundary_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void delaunay_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histcounts2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void griddata_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void griddatan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void matchpairs_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void findgroups_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void splitapply_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void groupcounts_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void groupsummary_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void grouptransform_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void groupfilter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void colperm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void symrcm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void spline_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pchip_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void makima_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mkpp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ppval_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void unmkpp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyfit_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyval_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void trapz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// fzero / fminbnd / fminsearch moved to libs/optim (see OptimLibrary::install)
void integral_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void roots_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyder_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyint_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void poly_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyvalm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polydiv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void residue_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void residuez_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void padecoef_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tf2zp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void zp2tf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// nansum / nanmean / nanmax / nanmin / nanvar / nanstd / nanmedian
// moved to libs/stats (see StatsLibrary::install)
void linspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void logspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// math/random/rng.cpp
void rand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void randn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void randi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void randperm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rng_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// manip.cpp
void repmat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fliplr_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void flipud_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rot90_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void circshift_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void tril_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void triu_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void flip_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void repelem_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sub2ind_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ind2sub_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void paddata_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void trimdata_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void resize_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// nd_manip.cpp
void permute_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ipermute_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void squeeze_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void blkdiag_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void shiftdim_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// math/elementary/ (Phase 7 floating-point additions)
void hypot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nthroot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void expm1_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log1p_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pow2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realpow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void reallog_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realsqrt_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gamma_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gammaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfinv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfcinv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfcx_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void beta_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void betaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void expint_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void psi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gammainc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void betainc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void legendre_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void besselj_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bessely_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void besseli_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void besselk_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void besselh_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ellipke_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void airy_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gammaincinv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void betaincinv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ellipj_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// int_math.cpp
void gcd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void lcm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitxor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitshift_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitcmp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitset_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitget_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// discrete.cpp
void unique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismember_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void union_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intersect_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setdiff_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setxor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void allunique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void numunique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismembertol_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uniquetol_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histcounts_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void discretize_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// accum.cpp
void accumarray_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void deg2rad_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rad2deg_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapToPi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapTo2Pi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapTo180_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapTo360_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// complex.cpp
void real_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void imag_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void conj_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void complex_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void angle_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// strings.cpp
void num2str_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int2str_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void validatestring_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void str2num_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void str2double_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void string_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void char_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strcmp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strcmpi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void upper_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void lower_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strtrim_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strsplit_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strcat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strlength_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strrep_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void contains_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extractAfter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extractBefore_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extractBetween_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void eraseBetween_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void insertAfter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void insertBefore_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void replaceBetween_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void startsWith_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void endsWith_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strncmp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strncmpi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strfind_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void blanks_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void newline_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strings_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void compose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strjust_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extract_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void split_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void join_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void deblank_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mat2str_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strjoin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strtok_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void append_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void count_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erase_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void replace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void reverse_reg(Span<const Value> args, size_t, Span<Value>, CallContext&);
void splitlines_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pad_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void strip_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void matches_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void convertCharsToStrings_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void convertStringsToChars_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstringscalar_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstrprop_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isletter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extractAfter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extractBefore_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void extractBetween_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void insertAfter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void insertBefore_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void eraseBetween_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void replaceBetween_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void dec2bin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void dec2hex_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bin2dec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hex2dec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void dec2base_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void base2dec_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rats_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexpi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexprep_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexptranslate_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// types.cpp
void double_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void single_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int8_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int16_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int32_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void int64_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint8_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint16_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint32_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uint64_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void logical_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cast_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void swapbytes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void typecast_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isnumeric_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void islogical_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ischar_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstring_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void iscell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isstruct_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isempty_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isscalar_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isreal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isinteger_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isfloat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issingle_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issparse_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isnan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isinf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isfinite_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isvector_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isrow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void iscolumn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismatrix_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issorted_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void issortedrows_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isuniform_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismissing_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void anymissing_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void standardizeMissing_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// issymmetric / ishermitian / isbanded / isdiag / istril / istriu /
// bandwidth _reg adapters → libs/linalg (predicates.cpp)
// vecnorm_reg, rref_reg, rcond_reg, planerot_reg, ldl_reg,
// lsqminnorm_reg, lsqnonneg_reg, balance_reg all migrated to libs/linalg.
void flintmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intmin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realmax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realmin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void allfinite_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void anynan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// `class` is registered via lambda in registerWorkspaceBuiltins() —
// no forward decl needed for a class_reg free function.

// format.cpp
void sprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// print.cpp
void disp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// File I/O + workspace save/load + CSV moved to libs/io (Phase 7c) —
// registrations live in IoLibrary::install. fopen/fclose/fread/fwrite/
// fgetl/fgets/feof/ferror/ftell/fseek/frewind, csvread/csvwrite,
// save/load are no longer registered from this TU.

// scan.cpp (still in libs/builtin under language/strings/)
void fscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void textscan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// env.cpp (still in libs/builtin under language/commands/)
void setenv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void getenv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// diagnostics.cpp
void error_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void warning_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void lastwarn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void MException_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rethrow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void throw_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void assert_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// datatypes/{cell,struct}/
void struct_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fieldnames_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isfield_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rmfield_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cellfun_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void num2cell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cell2mat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void iscellstr_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cellstr_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mat2cell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void structfun_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void getfield_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setfield_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void orderfields_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void struct2cell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cell2struct_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// matrix.cpp
void zeros_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ones_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void true_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void false_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void inf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void colon_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sparse_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void eye_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void magic_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void toeplitz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hankel_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void vander_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void compan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pascal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hilb_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void invhilb_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wilkinson_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void hadamard_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rosser_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// inv_reg / linsolve_reg → libs/linalg (properties.cpp, solvers.cpp)
// pageinv_reg → libs/linalg (page_ops.cpp)
// trace_reg, det_reg → libs/linalg (properties.cpp)
// chol_reg, lu_reg, qr_reg, svd_reg → libs/linalg (decompositions.cpp)
void topkrows_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// rank_reg, cond_reg, normest_reg → libs/linalg (properties.cpp)
// pinv_reg, orth_reg, null_reg → libs/linalg (pseudo_subspace.cpp)
// eig_reg, expm_reg, logm_reg, sqrtm_reg, schur_reg, hess_reg,
// sylvester_reg → libs/linalg (eig.cpp, matrix_functions.cpp)
// subspace_reg → libs/linalg (pseudo_subspace.cpp)
// norm_reg → libs/linalg (norms.cpp)
void size_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void length_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void numel_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ndims_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void reshape_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void transpose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pagetranspose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pagectranspose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void peaks_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sphere_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cylinder_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ellipsoid_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pagemtimes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void diag_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sort_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sortrows_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void find_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nnz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nonzeros_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void horzcat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void vertcat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void meshgrid_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ndgrid_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// kron_reg → libs/linalg (vector_ops.cpp)
void cumsum_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cumprod_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cummax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cummin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void diff_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void any_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void all_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void xor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// cross_reg, dot_reg → libs/linalg (vector_ops.cpp)

// Pack 11: operator-named function adapters (binary + unary).
// Defined in language/operators/{binary,unary}_ops.cpp.
void plus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void minus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void times_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mtimes_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rdivide_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mrdivide_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mldivide_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ldivide_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void power_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mpower_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void eq_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ne_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void lt_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void le_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gt_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ge_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void and_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void or_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uminus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uplus_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void not_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ctranspose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
} // namespace numkit::builtin::detail

namespace numkit {

// ── Warning helper for unsupported features ──────────────────
static void warnNotSupported(CallContext &ctx, const std::string &feature)
{
    ctx.engine->outputText("Warning: '" + feature + "' is not yet supported.\n");
}

void BuiltinLibrary::install(Engine &engine)
{
    registerBinaryOps(engine);
    registerUnaryOps(engine);
    registerTypeFunctions(engine);
    registerCellStructFunctions(engine);
    registerStringFunctions(engine);
    registerComplexFunctions(engine);

    registerWorkspaceBuiltins(engine);

    // ── Phase 6c: math/elementary/ public-API-backed built-ins ─────────────
    engine.registerFunction("sqrt",     &builtin::detail::sqrt_reg);
    engine.registerFunction("abs",      &builtin::detail::abs_reg);
    engine.registerFunction("sin",      &builtin::detail::sin_reg);
    engine.registerFunction("cos",      &builtin::detail::cos_reg);
    engine.registerFunction("tan",      &builtin::detail::tan_reg);
    engine.registerFunction("asin",     &builtin::detail::asin_reg);
    engine.registerFunction("acos",     &builtin::detail::acos_reg);
    engine.registerFunction("atan",     &builtin::detail::atan_reg);
    engine.registerFunction("atan2",    &builtin::detail::atan2_reg);
    engine.registerFunction("sinh",     &builtin::detail::sinh_reg);
    engine.registerFunction("cosh",     &builtin::detail::cosh_reg);
    engine.registerFunction("tanh",     &builtin::detail::tanh_reg);
    engine.registerFunction("asinh",    &builtin::detail::asinh_reg);
    engine.registerFunction("acosh",    &builtin::detail::acosh_reg);
    engine.registerFunction("atanh",    &builtin::detail::atanh_reg);
    engine.registerFunction("sind",     &builtin::detail::sind_reg);
    engine.registerFunction("cosd",     &builtin::detail::cosd_reg);
    engine.registerFunction("tand",     &builtin::detail::tand_reg);
    engine.registerFunction("asind",    &builtin::detail::asind_reg);
    engine.registerFunction("acosd",    &builtin::detail::acosd_reg);
    engine.registerFunction("atand",    &builtin::detail::atand_reg);
    engine.registerFunction("atan2d",   &builtin::detail::atan2d_reg);
    engine.registerFunction("sinpi",    &builtin::detail::sinpi_reg);
    engine.registerFunction("cospi",    &builtin::detail::cospi_reg);
    engine.registerFunction("sec",      &builtin::detail::sec_reg);
    engine.registerFunction("csc",      &builtin::detail::csc_reg);
    engine.registerFunction("cot",      &builtin::detail::cot_reg);
    engine.registerFunction("sech",     &builtin::detail::sech_reg);
    engine.registerFunction("csch",     &builtin::detail::csch_reg);
    engine.registerFunction("coth",     &builtin::detail::coth_reg);
    engine.registerFunction("secd",     &builtin::detail::secd_reg);
    engine.registerFunction("cscd",     &builtin::detail::cscd_reg);
    engine.registerFunction("cotd",     &builtin::detail::cotd_reg);
    engine.registerFunction("asec",     &builtin::detail::asec_reg);
    engine.registerFunction("acsc",     &builtin::detail::acsc_reg);
    engine.registerFunction("acot",     &builtin::detail::acot_reg);
    engine.registerFunction("asech",    &builtin::detail::asech_reg);
    engine.registerFunction("acsch",    &builtin::detail::acsch_reg);
    engine.registerFunction("acoth",    &builtin::detail::acoth_reg);
    engine.registerFunction("asecd",    &builtin::detail::asecd_reg);
    engine.registerFunction("acscd",    &builtin::detail::acscd_reg);
    engine.registerFunction("acotd",    &builtin::detail::acotd_reg);
    engine.registerFunction("cart2pol", &builtin::detail::cart2pol_reg);
    engine.registerFunction("pol2cart", &builtin::detail::pol2cart_reg);
    engine.registerFunction("cart2sph", &builtin::detail::cart2sph_reg);
    engine.registerFunction("sph2cart", &builtin::detail::sph2cart_reg);
    engine.registerFunction("exp",      &builtin::detail::exp_reg);
    engine.registerFunction("log",      &builtin::detail::log_reg);
    engine.registerFunction("log2",     &builtin::detail::log2_reg);
    engine.registerFunction("log10",    &builtin::detail::log10_reg);
    engine.registerFunction("floor",    &builtin::detail::floor_reg);
    engine.registerFunction("ceil",     &builtin::detail::ceil_reg);
    engine.registerFunction("round",    &builtin::detail::round_reg);
    engine.registerFunction("fix",      &builtin::detail::fix_reg);
    engine.registerFunction("mod",      &builtin::detail::mod_reg);
    engine.registerFunction("rem",      &builtin::detail::rem_reg);
    engine.registerFunction("sign",     &builtin::detail::sign_reg);
    engine.registerFunction("subplus",  &builtin::detail::subplus_reg);
    engine.registerFunction("max",      &builtin::detail::max_reg);
    engine.registerFunction("min",      &builtin::detail::min_reg);
    engine.registerFunction("sum",      &builtin::detail::sum_reg);
    engine.registerFunction("prod",     &builtin::detail::prod_reg);
    engine.registerFunction("mean",     &builtin::detail::mean_reg);

    // var/std/median/quantile/prctile/mode/cov/corrcoef + skewness/kurtosis
    // registered by StatsLibrary::install() (Phase 7b — Statistics Toolbox).
    engine.registerFunction("primes",   &builtin::detail::primes_reg);
    engine.registerFunction("isprime",  &builtin::detail::isprime_reg);
    engine.registerFunction("factor",   &builtin::detail::factor_reg);
    engine.registerFunction("perms",     &builtin::detail::perms_reg);
    engine.registerFunction("factorial", &builtin::detail::factorial_reg);
    engine.registerFunction("nchoosek",  &builtin::detail::nchoosek_reg);
    engine.registerFunction("gradient",  &builtin::detail::gradient_reg);
    engine.registerFunction("cumtrapz",  &builtin::detail::cumtrapz_reg);
    engine.registerFunction("interp1",   &builtin::detail::interp1_reg);
    engine.registerFunction("interp2",   &builtin::detail::interp2_reg);
    engine.registerFunction("interp3",   &builtin::detail::interp3_reg);
    engine.registerFunction("interpn",   &builtin::detail::interpn_reg);
    engine.registerFunction("inpolygon", &builtin::detail::inpolygon_reg);
    engine.registerFunction("convhull",  &builtin::detail::convhull_reg);
    engine.registerFunction("polyarea",  &builtin::detail::polyarea_reg);
    engine.registerFunction("boundary",  &builtin::detail::boundary_reg);
    engine.registerFunction("delaunay",  &builtin::detail::delaunay_reg);
    engine.registerFunction("histcounts2", &builtin::detail::histcounts2_reg);
    engine.registerFunction("griddata",  &builtin::detail::griddata_reg);
    engine.registerFunction("griddatan", &builtin::detail::griddatan_reg);
    engine.registerFunction("matchpairs", &builtin::detail::matchpairs_reg);
    engine.registerFunction("findgroups",  &builtin::detail::findgroups_reg);
    engine.registerFunction("splitapply",  &builtin::detail::splitapply_reg);
    engine.registerFunction("groupcounts", &builtin::detail::groupcounts_reg);
    engine.registerFunction("groupsummary", &builtin::detail::groupsummary_reg);
    engine.registerFunction("grouptransform", &builtin::detail::grouptransform_reg);
    engine.registerFunction("groupfilter", &builtin::detail::groupfilter_reg);
    engine.registerFunction("colperm", &builtin::detail::colperm_reg);
    engine.registerFunction("symrcm", &builtin::detail::symrcm_reg);
    engine.registerFunction("spline",    &builtin::detail::spline_reg);
    engine.registerFunction("pchip",     &builtin::detail::pchip_reg);
    engine.registerFunction("makima",    &builtin::detail::makima_reg);
    engine.registerFunction("mkpp",      &builtin::detail::mkpp_reg);
    engine.registerFunction("ppval",     &builtin::detail::ppval_reg);
    engine.registerFunction("unmkpp",    &builtin::detail::unmkpp_reg);
    engine.registerFunction("polyfit",   &builtin::detail::polyfit_reg);
    engine.registerFunction("polyval",   &builtin::detail::polyval_reg);
    engine.registerFunction("trapz",     &builtin::detail::trapz_reg);
    // fzero / fminbnd / fminsearch registered by OptimLibrary::install()
    // (libs/optim) with cross-domain top-level promotion to keep MATLAB-base UX.

    // optimset / optimget — option struct utility, implemented as
    // lambdas (no public API needed).
    engine.registerFunction("optimset",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            // optimset(name1, val1, name2, val2, ...) → struct with those
            // fields. Defaults are filled in for any keys not supplied so
            // callers can rely on their presence.
            auto *mr = ctx.engine->resource();
            auto s = Value::structure(mr);
            // MATLAB-typical default values.
            s.field("Display")     = Value::fromString("notify", mr);
            s.field("MaxFunEvals") = Value::scalar(1000.0, mr);
            s.field("MaxIter")     = Value::scalar(500.0, mr);
            s.field("TolFun")      = Value::scalar(1e-6, mr);
            s.field("TolX")        = Value::scalar(1e-6, mr);
            // Apply user overrides.
            for (size_t i = 0; i + 1 < args.size(); i += 2) {
                if (!args[i].isChar() && !args[i].isString())
                    throw std::runtime_error(
                        "optimset: option name must be a string");
                s.field(args[i].toString()) = args[i + 1];
            }
            outs[0] = std::move(s);
        });

    engine.registerFunction("optimget",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("optimget requires (options, name[, default])");
            const Value &opts = args[0];
            const std::string name = args[1].toString();
            if (opts.isStruct() && opts.hasField(name)) {
                outs[0] = opts.field(name);
                return;
            }
            // Fallback to user-supplied default; otherwise [].
            if (args.size() >= 3) outs[0] = args[2];
            else outs[0] = Value::empty();
        });
    engine.registerFunction("integral",  &builtin::detail::integral_reg);
    engine.registerFunction("roots",     &builtin::detail::roots_reg);
    engine.registerFunction("polyder",   &builtin::detail::polyder_reg);
    engine.registerFunction("polyint",   &builtin::detail::polyint_reg);
    engine.registerFunction("poly",      &builtin::detail::poly_reg);
    engine.registerFunction("polyvalm",  &builtin::detail::polyvalm_reg);
    engine.registerFunction("polydiv",   &builtin::detail::polydiv_reg);
    engine.registerFunction("residue",   &builtin::detail::residue_reg);
    engine.registerFunction("residuez",  &builtin::detail::residuez_reg);
    engine.registerFunction("padecoef",  &builtin::detail::padecoef_reg);
    engine.registerFunction("tf2zp",     &builtin::detail::tf2zp_reg);
    engine.registerFunction("zp2tf",     &builtin::detail::zp2tf_reg);

    // ── Phase 2 NaN-aware reductions ───────────────────────────────
    // nan* family registered by StatsLibrary::install()

    engine.registerFunction("linspace", &builtin::detail::linspace_reg);
    engine.registerFunction("logspace", &builtin::detail::logspace_reg);
    engine.registerFunction("rand",     &builtin::detail::rand_reg);
    engine.registerFunction("randn",    &builtin::detail::randn_reg);
    engine.registerFunction("randi",    &builtin::detail::randi_reg);
    engine.registerFunction("randperm", &builtin::detail::randperm_reg);
    engine.registerFunction("rng",      &builtin::detail::rng_reg);

    // ── Phase 5 array manipulation ─────────────────────────────────
    engine.registerFunction("repmat",    &builtin::detail::repmat_reg);
    engine.registerFunction("fliplr",    &builtin::detail::fliplr_reg);
    engine.registerFunction("flipud",    &builtin::detail::flipud_reg);
    engine.registerFunction("rot90",     &builtin::detail::rot90_reg);
    engine.registerFunction("circshift", &builtin::detail::circshift_reg);
    engine.registerFunction("tril",      &builtin::detail::tril_reg);
    engine.registerFunction("triu",      &builtin::detail::triu_reg);
    engine.registerFunction("flip",      &builtin::detail::flip_reg);
    engine.registerFunction("repelem",   &builtin::detail::repelem_reg);
    engine.registerFunction("sub2ind",   &builtin::detail::sub2ind_reg);
    engine.registerFunction("ind2sub",   &builtin::detail::ind2sub_reg);
    engine.registerFunction("paddata",   &builtin::detail::paddata_reg);
    engine.registerFunction("trimdata",  &builtin::detail::trimdata_reg);
    engine.registerFunction("resize",    &builtin::detail::resize_reg);

    // ── Pack 33: idivide + bsxfun ─────────────────────────────────────
    //
    // idivide(A, B[, opt]) — integer-style division with rounding mode
    // opt ∈ {'fix' (default), 'floor', 'ceil', 'round'}. Per MATLAB,
    // at least one of A/B must be of an integer class (idivide on two
    // doubles is rejected). The other operand can be a same-class
    // integer or a scalar double; mixed integer classes / non-scalar
    // doubles are rejected. Result type matches the integer operand.
    // See BUGS.md #29.
    engine.registerFunction("idivide",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("idivide requires (A, B[, opt])");
            std::string opt = "fix";
            if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
                opt = args[2].toString();
                for (auto &c : opt)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            auto *mr = ctx.engine->resource();

            const ValueType t0 = args[0].type();
            const ValueType t1 = args[1].type();
            const bool int0 = isIntegerType(t0);
            const bool int1 = isIntegerType(t1);
            const bool dbl0 = (t0 == ValueType::DOUBLE);
            const bool dbl1 = (t1 == ValueType::DOUBLE);

            if (!int0 && !int1)
                throw std::runtime_error(
                    "At least one argument must belong to an integer class.");

            ValueType resultType;
            if (int0 && int1) {
                if (t0 != t1)
                    throw std::runtime_error(
                        "Integers can only be combined with integers of the same "
                        "class, or scalar doubles.");
                resultType = t0;
            } else if (int0) {
                if (!dbl1 || !args[1].isScalar())
                    throw std::runtime_error(
                        "Integers can only be combined with integers of the same "
                        "class, or scalar doubles.");
                resultType = t0;
            } else {
                if (!dbl0 || !args[0].isScalar())
                    throw std::runtime_error(
                        "Integers can only be combined with integers of the same "
                        "class, or scalar doubles.");
                resultType = t1;
            }

            // Compose: round(A ./ B). rdivide on integer arrays returns
            // double, so the rounding helpers see DOUBLE input.
            const Value a = builtin::toDouble(args[0], mr);
            const Value b = builtin::toDouble(args[1], mr);
            Value q = builtin::rdivide(a, b, mr);
            if (opt == "fix" || opt.empty()) q = builtin::fix(q, mr);
            else if (opt == "floor")          q = builtin::floor(q, mr);
            else if (opt == "ceil")           q = builtin::ceil(q, mr);
            else if (opt == "round")          q = builtin::round(q, mr);
            else
                throw std::runtime_error(
                    "idivide: opt must be 'fix', 'floor', 'ceil', or 'round'");

            outs[0] = builtin::cast(q, mtypeName(resultType), mr);
        });

    // bsxfun(fn, A, B) — apply fn elementwise to (A, B). numkit's
    // built-in elementwise ops already broadcast, so the wrapper just
    // forwards to the function handle.
    engine.registerFunction("bsxfun",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 3)
                throw std::runtime_error("bsxfun requires (fn, A, B)");
            if (!args[0].isFuncHandle())
                throw std::runtime_error("bsxfun: first argument must be a function handle");
            const Value callArgs[2] = { args[1], args[2] };
            outs[0] = ctx.engine->callFunctionHandle(
                args[0], Span<const Value>(callArgs, 2), ctx.env);
        });

    // ── Pack 34: function-handle introspection ────────────────────────
    //
    // functions(@h) returns a small struct describing the handle.
    // numkit handles only carry a name, so the {function, type, file}
    // fields are the natural set; advanced MATLAB metadata
    // (workspace, parentage) is not available.
    engine.registerFunction("functions",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isFuncHandle())
                throw std::runtime_error("functions: argument must be a function handle");
            auto *mr = ctx.engine->resource();
            auto s = Value::structure(mr);
            const std::string name = args[0].funcHandleName();
            s.field("function") = Value::fromString(name, mr);
            s.field("type")     = Value::fromString("simple", mr);
            s.field("file")     = Value::fromString("", mr);
            outs[0] = std::move(s);
        });

    // localfunctions() — MATLAB returns a cell of handles to local
    // functions defined in the current m-file. Without per-file
    // function-table introspection we return the empty cell, which
    // matches MATLAB when called outside a function file.
    engine.registerFunction("localfunctions",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::cell(0, 1, ctx.engine->resource());
        });

    // ── Pack 35: convertContainedStringsToChars ───────────────────────
    //
    // Recursive descent: char/numeric stays put; string scalar →
    // char row; cell → cell of recursively-converted entries; struct
    // → struct with each field converted. Mirrors MATLAB's behaviour
    // for the typical script use case (sanitising mixed cell/struct
    // payloads before passing to a char-only API).
    {
        struct Walker {
            std::pmr::memory_resource *mr;
            Value walk(const Value &v) {
                if (v.isString()) {
                    if (v.numel() <= 1)
                        return Value::fromString(v.toString(), mr);
                    auto c = Value::cell(v.numel(), 1, mr);
                    for (size_t i = 0; i < v.numel(); ++i)
                        c.cellAt(i) = Value::fromString(v.stringElem(i), mr);
                    return c;
                }
                if (v.isCell()) {
                    const auto &d = v.dims();
                    auto c = d.is3D()
                                ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                                : Value::cell(d.rows(), d.cols(), mr);
                    for (size_t i = 0; i < v.numel(); ++i)
                        c.cellAt(i) = walk(v.cellAt(i));
                    return c;
                }
                if (v.isStruct() && !v.isStructArray()) {
                    auto s = Value::structure(mr);
                    for (auto &kv : v.structFields())
                        s.field(kv.first) = walk(kv.second);
                    return s;
                }
                return v;
            }
        };
        engine.registerFunction("convertContainedStringsToChars",
            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                if (args.empty())
                    throw std::runtime_error(
                        "convertContainedStringsToChars: requires 1 argument");
                Walker w{ctx.engine->resource()};
                outs[0] = w.walk(args[0]);
            });
    }

    // ── Phase 6 N-D manipulation ──────────────────────────────────
    engine.registerFunction("permute",  &builtin::detail::permute_reg);
    engine.registerFunction("ipermute", &builtin::detail::ipermute_reg);
    engine.registerFunction("squeeze",  &builtin::detail::squeeze_reg);
    engine.registerFunction("cat",      &builtin::detail::cat_reg);
    engine.registerFunction("blkdiag",  &builtin::detail::blkdiag_reg);
    engine.registerFunction("shiftdim", &builtin::detail::shiftdim_reg);

    // ── Phase 7 numeric utilities ─────────────────────────────────
    engine.registerFunction("hypot",    &builtin::detail::hypot_reg);
    engine.registerFunction("nthroot",  &builtin::detail::nthroot_reg);
    engine.registerFunction("expm1",    &builtin::detail::expm1_reg);
    engine.registerFunction("log1p",    &builtin::detail::log1p_reg);
    engine.registerFunction("pow2",     &builtin::detail::pow2_reg);
    engine.registerFunction("realpow",  &builtin::detail::realpow_reg);
    engine.registerFunction("reallog",  &builtin::detail::reallog_reg);
    engine.registerFunction("realsqrt", &builtin::detail::realsqrt_reg);
    engine.registerFunction("gamma",    &builtin::detail::gamma_reg);
    engine.registerFunction("gammaln",  &builtin::detail::gammaln_reg);
    engine.registerFunction("erf",      &builtin::detail::erf_reg);
    engine.registerFunction("erfc",     &builtin::detail::erfc_reg);
    engine.registerFunction("erfinv",   &builtin::detail::erfinv_reg);
    engine.registerFunction("erfcinv",  &builtin::detail::erfcinv_reg);
    engine.registerFunction("erfcx",    &builtin::detail::erfcx_reg);
    engine.registerFunction("beta",     &builtin::detail::beta_reg);
    engine.registerFunction("betaln",   &builtin::detail::betaln_reg);
    engine.registerFunction("expint",   &builtin::detail::expint_reg);
    engine.registerFunction("psi",      &builtin::detail::psi_reg);
    engine.registerFunction("gammainc", &builtin::detail::gammainc_reg);
    engine.registerFunction("betainc",  &builtin::detail::betainc_reg);
    engine.registerFunction("legendre", &builtin::detail::legendre_reg);
    engine.registerFunction("besselj",  &builtin::detail::besselj_reg);
    engine.registerFunction("bessely",  &builtin::detail::bessely_reg);
    engine.registerFunction("besseli",  &builtin::detail::besseli_reg);
    engine.registerFunction("besselk",  &builtin::detail::besselk_reg);
    engine.registerFunction("besselh",  &builtin::detail::besselh_reg);
    engine.registerFunction("ellipke",  &builtin::detail::ellipke_reg);
    engine.registerFunction("airy",     &builtin::detail::airy_reg);
    engine.registerFunction("gammaincinv", &builtin::detail::gammaincinv_reg);
    engine.registerFunction("betaincinv",  &builtin::detail::betaincinv_reg);
    engine.registerFunction("ellipj",   &builtin::detail::ellipj_reg);
    engine.registerFunction("gcd",      &builtin::detail::gcd_reg);
    engine.registerFunction("lcm",      &builtin::detail::lcm_reg);
    engine.registerFunction("bitand",   &builtin::detail::bitand_reg);
    engine.registerFunction("bitor",    &builtin::detail::bitor_reg);
    engine.registerFunction("bitxor",   &builtin::detail::bitxor_reg);
    engine.registerFunction("bitshift", &builtin::detail::bitshift_reg);
    engine.registerFunction("bitcmp",   &builtin::detail::bitcmp_reg);
    engine.registerFunction("bitset",   &builtin::detail::bitset_reg);
    engine.registerFunction("bitget",   &builtin::detail::bitget_reg);

    // ── Phase 8 set / search ops ──────────────────────────────────
    engine.registerFunction("unique",     &builtin::detail::unique_reg);
    engine.registerFunction("ismember",   &builtin::detail::ismember_reg);
    engine.registerFunction("union",      &builtin::detail::union_reg);
    engine.registerFunction("intersect",  &builtin::detail::intersect_reg);
    engine.registerFunction("setdiff",    &builtin::detail::setdiff_reg);
    engine.registerFunction("setxor",     &builtin::detail::setxor_reg);
    engine.registerFunction("allunique",  &builtin::detail::allunique_reg);
    engine.registerFunction("numunique",  &builtin::detail::numunique_reg);
    engine.registerFunction("ismembertol",&builtin::detail::ismembertol_reg);
    engine.registerFunction("uniquetol",  &builtin::detail::uniquetol_reg);
    engine.registerFunction("histcounts", &builtin::detail::histcounts_reg);
    engine.registerFunction("histc", &builtin::detail::histc_reg);
    engine.registerFunction("discretize", &builtin::detail::discretize_reg);
    engine.registerFunction("accumarray", &builtin::detail::accumarray_reg);
    engine.registerFunction("deg2rad",  &builtin::detail::deg2rad_reg);
    engine.registerFunction("rad2deg",  &builtin::detail::rad2deg_reg);
    engine.registerFunction("wrapToPi",  &builtin::detail::wrapToPi_reg);
    engine.registerFunction("wrapTo2Pi", &builtin::detail::wrapTo2Pi_reg);
    engine.registerFunction("wrapTo180", &builtin::detail::wrapTo180_reg);
    engine.registerFunction("wrapTo360", &builtin::detail::wrapTo360_reg);

    // ── Phase 6c: matrix.cpp public-API-backed built-ins ───────────
    engine.registerFunction("zeros",     &builtin::detail::zeros_reg);
    engine.registerFunction("ones",      &builtin::detail::ones_reg);
    engine.registerFunction("true",      &builtin::detail::true_reg);
    engine.registerFunction("false",     &builtin::detail::false_reg);
    engine.registerFunction("nan",       &builtin::detail::nan_reg);
    engine.registerFunction("NaN",       &builtin::detail::nan_reg);
    engine.registerFunction("inf",       &builtin::detail::inf_reg);
    engine.registerFunction("Inf",       &builtin::detail::inf_reg);
    engine.registerFunction("colon",     &builtin::detail::colon_reg);
    engine.registerFunction("sparse",    &builtin::detail::sparse_reg);
    engine.registerFunction("eye",       &builtin::detail::eye_reg);
    engine.registerFunction("magic",     &builtin::detail::magic_reg);
    engine.registerFunction("toeplitz",  &builtin::detail::toeplitz_reg);
    engine.registerFunction("hankel",    &builtin::detail::hankel_reg);
    engine.registerFunction("vander",    &builtin::detail::vander_reg);
    engine.registerFunction("compan",    &builtin::detail::compan_reg);
    engine.registerFunction("pascal",    &builtin::detail::pascal_reg);
    engine.registerFunction("hilb",      &builtin::detail::hilb_reg);
    engine.registerFunction("invhilb",   &builtin::detail::invhilb_reg);
    engine.registerFunction("wilkinson", &builtin::detail::wilkinson_reg);
    engine.registerFunction("hadamard",  &builtin::detail::hadamard_reg);
    engine.registerFunction("rosser",    &builtin::detail::rosser_reg);
    // inv registered by LinalgLibrary::install (libs/linalg).
    // linsolve / pageinv registered by LinalgLibrary::install (libs/linalg).
    // trace / det registered by LinalgLibrary::install (libs/linalg).
    engine.registerFunction("topkrows",  &builtin::detail::topkrows_reg);
    // chol / lu / qr / svd registered by LinalgLibrary::install (libs/linalg).
    // rank / cond / normest registered by LinalgLibrary::install.
    // pinv / orth / null / subspace registered by LinalgLibrary::install.
    // eig / expm / logm / sqrtm / schur / hess registered by
    // LinalgLibrary::install (libs/linalg).
    // norm registered by LinalgLibrary::install (libs/linalg).
    // sylvester registered by LinalgLibrary::install (libs/linalg).

    // Linalg basics also exposed under `compat.*` so user code that
    // qualifies them as compat.norm / compat.inv (e.g. when porting
    // from a project that namespaces all calls) works without
    // surprises. Functions in the global namespace are already
    // accessible bare; this just adds explicit aliases in compat.
    // compat.norm registered by LinalgLibrary::install (libs/linalg).
    // compat.{inv,trace,det,rank,cond,normest} registered by LinalgLibrary::install.
    // compat.{pinv,chol,lu,qr,svd,orth,null,subspace} registered by
    // LinalgLibrary::install (libs/linalg).
    // compat.linsolve registered by LinalgLibrary::install (libs/linalg).
    // compat.{eig,expm,logm,sqrtm,schur,hess,sylvester} registered by
    // LinalgLibrary::install (libs/linalg).
    engine.registerFunction("size",      &builtin::detail::size_reg);
    engine.registerFunction("length",    &builtin::detail::length_reg);
    engine.registerFunction("numel",     &builtin::detail::numel_reg);
    engine.registerFunction("ndims",     &builtin::detail::ndims_reg);
    engine.registerFunction("reshape",   &builtin::detail::reshape_reg);
    engine.registerFunction("transpose", &builtin::detail::transpose_reg);
    engine.registerFunction("pagetranspose",  &builtin::detail::pagetranspose_reg);
    engine.registerFunction("pagectranspose", &builtin::detail::pagectranspose_reg);
    engine.registerFunction("peaks",          &builtin::detail::peaks_reg);
    engine.registerFunction("sphere",         &builtin::detail::sphere_reg);
    engine.registerFunction("cylinder",       &builtin::detail::cylinder_reg);
    engine.registerFunction("ellipsoid",      &builtin::detail::ellipsoid_reg);

    // ── Pack 11: operator-named functions ────────────────────────
    engine.registerFunction("plus",       &builtin::detail::plus_reg);
    engine.registerFunction("minus",      &builtin::detail::minus_reg);
    engine.registerFunction("times",      &builtin::detail::times_reg);
    engine.registerFunction("mtimes",     &builtin::detail::mtimes_reg);
    engine.registerFunction("rdivide",    &builtin::detail::rdivide_reg);
    engine.registerFunction("mrdivide",   &builtin::detail::mrdivide_reg);
    engine.registerFunction("mldivide",   &builtin::detail::mldivide_reg);
    engine.registerFunction("ldivide",    &builtin::detail::ldivide_reg);
    engine.registerFunction("power",      &builtin::detail::power_reg);
    engine.registerFunction("mpower",     &builtin::detail::mpower_reg);
    engine.registerFunction("eq",         &builtin::detail::eq_reg);
    engine.registerFunction("ne",         &builtin::detail::ne_reg);
    engine.registerFunction("lt",         &builtin::detail::lt_reg);
    engine.registerFunction("le",         &builtin::detail::le_reg);
    engine.registerFunction("gt",         &builtin::detail::gt_reg);
    engine.registerFunction("ge",         &builtin::detail::ge_reg);
    engine.registerFunction("and",        &builtin::detail::and_reg);
    engine.registerFunction("or",         &builtin::detail::or_reg);
    engine.registerFunction("uminus",     &builtin::detail::uminus_reg);
    engine.registerFunction("uplus",      &builtin::detail::uplus_reg);
    engine.registerFunction("not",        &builtin::detail::not_reg);
    engine.registerFunction("ctranspose", &builtin::detail::ctranspose_reg);
    engine.registerFunction("pagemtimes",&builtin::detail::pagemtimes_reg);
    engine.registerFunction("diag",      &builtin::detail::diag_reg);
    engine.registerFunction("sort",      &builtin::detail::sort_reg);
    engine.registerFunction("sortrows",  &builtin::detail::sortrows_reg);
    engine.registerFunction("find",      &builtin::detail::find_reg);
    engine.registerFunction("nnz",       &builtin::detail::nnz_reg);
    engine.registerFunction("nonzeros",  &builtin::detail::nonzeros_reg);
    engine.registerFunction("horzcat",   &builtin::detail::horzcat_reg);
    engine.registerFunction("vertcat",   &builtin::detail::vertcat_reg);
    engine.registerFunction("meshgrid",  &builtin::detail::meshgrid_reg);
    engine.registerFunction("ndgrid",    &builtin::detail::ndgrid_reg);
    // kron / cross / dot registered by LinalgLibrary::install (libs/linalg).
    engine.registerFunction("cumsum",    &builtin::detail::cumsum_reg);
    engine.registerFunction("cumprod",   &builtin::detail::cumprod_reg);
    engine.registerFunction("cummax",    &builtin::detail::cummax_reg);
    engine.registerFunction("cummin",    &builtin::detail::cummin_reg);
    engine.registerFunction("diff",      &builtin::detail::diff_reg);
    engine.registerFunction("any",       &builtin::detail::any_reg);
    engine.registerFunction("all",       &builtin::detail::all_reg);
    engine.registerFunction("xor",       &builtin::detail::xor_reg);

    // ── Phase 6c: math/elementary/complex.cpp public-API-backed built-ins ──────────
    engine.registerFunction("real",    &builtin::detail::real_reg);
    engine.registerFunction("imag",    &builtin::detail::imag_reg);
    engine.registerFunction("conj",    &builtin::detail::conj_reg);
    engine.registerFunction("complex", &builtin::detail::complex_reg);
    engine.registerFunction("angle",   &builtin::detail::angle_reg);

    // ── Phase 6c: strings.cpp public-API-backed built-ins ──────────
    engine.registerFunction("num2str",    &builtin::detail::num2str_reg);
    engine.registerFunction("int2str",    &builtin::detail::int2str_reg);
    engine.registerFunction("validatestring", &builtin::detail::validatestring_reg);
    engine.registerFunction("str2num",    &builtin::detail::str2num_reg);
    engine.registerFunction("str2double", &builtin::detail::str2double_reg);
    engine.registerFunction("string",     &builtin::detail::string_reg);
    engine.registerFunction("char",       &builtin::detail::char_reg);
    engine.registerFunction("strcmp",     &builtin::detail::strcmp_reg);
    engine.registerFunction("strcmpi",    &builtin::detail::strcmpi_reg);
    engine.registerFunction("upper",      &builtin::detail::upper_reg);
    engine.registerFunction("lower",      &builtin::detail::lower_reg);
    engine.registerFunction("strtrim",    &builtin::detail::strtrim_reg);
    engine.registerFunction("strsplit",   &builtin::detail::strsplit_reg);
    engine.registerFunction("strcat",     &builtin::detail::strcat_reg);
    engine.registerFunction("strlength",  &builtin::detail::strlength_reg);
    engine.registerFunction("strrep",     &builtin::detail::strrep_reg);
    engine.registerFunction("contains",   &builtin::detail::contains_reg);
    engine.registerFunction("startsWith", &builtin::detail::startsWith_reg);
    engine.registerFunction("endsWith",   &builtin::detail::endsWith_reg);
    // Lowercase aliases for the string-between family (canonical
    // MATLAB names are camelCase; numkit ships both for convenience).
    engine.registerFunction("extractafter",   &builtin::detail::extractAfter_reg);
    engine.registerFunction("extractbefore",  &builtin::detail::extractBefore_reg);
    engine.registerFunction("extractbetween", &builtin::detail::extractBetween_reg);
    engine.registerFunction("erasebetween",   &builtin::detail::eraseBetween_reg);
    engine.registerFunction("insertafter",    &builtin::detail::insertAfter_reg);
    engine.registerFunction("insertbefore",   &builtin::detail::insertBefore_reg);
    engine.registerFunction("replacebetween", &builtin::detail::replaceBetween_reg);
    engine.registerFunction("strncmp",    &builtin::detail::strncmp_reg);
    engine.registerFunction("strncmpi",   &builtin::detail::strncmpi_reg);
    engine.registerFunction("strfind",    &builtin::detail::strfind_reg);
    engine.registerFunction("blanks",     &builtin::detail::blanks_reg);
    engine.registerFunction("newline",    &builtin::detail::newline_reg);
    engine.registerFunction("strings",    &builtin::detail::strings_reg);
    engine.registerFunction("compose",    &builtin::detail::compose_reg);
    engine.registerFunction("strjust",    &builtin::detail::strjust_reg);
    engine.registerFunction("extract",    &builtin::detail::extract_reg);
    engine.registerFunction("split",      &builtin::detail::split_reg);
    engine.registerFunction("join",       &builtin::detail::join_reg);
    engine.registerFunction("deblank",    &builtin::detail::deblank_reg);
    engine.registerFunction("mat2str",    &builtin::detail::mat2str_reg);
    engine.registerFunction("strjoin",    &builtin::detail::strjoin_reg);
    engine.registerFunction("strtok",     &builtin::detail::strtok_reg);
    engine.registerFunction("append",     &builtin::detail::append_reg);
    engine.registerFunction("count",      &builtin::detail::count_reg);
    engine.registerFunction("erase",      &builtin::detail::erase_reg);
    engine.registerFunction("replace",    &builtin::detail::replace_reg);
    engine.registerFunction("reverse",    &builtin::detail::reverse_reg);
    engine.registerFunction("splitlines", &builtin::detail::splitlines_reg);
    engine.registerFunction("pad",        &builtin::detail::pad_reg);
    engine.registerFunction("strip",      &builtin::detail::strip_reg);
    engine.registerFunction("matches",    &builtin::detail::matches_reg);
    engine.registerFunction("convertCharsToStrings",
                                          &builtin::detail::convertCharsToStrings_reg);
    engine.registerFunction("convertStringsToChars",
                                          &builtin::detail::convertStringsToChars_reg);
    engine.registerFunction("isstringscalar",
                                          &builtin::detail::isstringscalar_reg);
    // MATLAB exports the canonical camelCase name `isStringScalar`;
    // alias it to the same impl. See BUGS.md #25.
    engine.registerFunction("isStringScalar",
                                          &builtin::detail::isstringscalar_reg);
    engine.registerFunction("isstrprop",  &builtin::detail::isstrprop_reg);
    engine.registerFunction("isletter",   &builtin::detail::isletter_reg);
    engine.registerFunction("isspace",    &builtin::detail::isspace_reg);
    engine.registerFunction("extractAfter",   &builtin::detail::extractAfter_reg);
    engine.registerFunction("extractBefore",  &builtin::detail::extractBefore_reg);
    engine.registerFunction("extractBetween", &builtin::detail::extractBetween_reg);
    engine.registerFunction("insertAfter",    &builtin::detail::insertAfter_reg);
    engine.registerFunction("insertBefore",   &builtin::detail::insertBefore_reg);
    engine.registerFunction("eraseBetween",   &builtin::detail::eraseBetween_reg);
    engine.registerFunction("replaceBetween", &builtin::detail::replaceBetween_reg);
    engine.registerFunction("dec2bin",    &builtin::detail::dec2bin_reg);
    engine.registerFunction("dec2hex",    &builtin::detail::dec2hex_reg);
    engine.registerFunction("dec2base",   &builtin::detail::dec2base_reg);
    engine.registerFunction("base2dec",   &builtin::detail::base2dec_reg);
    engine.registerFunction("bin2dec",    &builtin::detail::bin2dec_reg);
    engine.registerFunction("hex2dec",    &builtin::detail::hex2dec_reg);
    engine.registerFunction("rat",        &builtin::detail::rat_reg);
    engine.registerFunction("rats",       &builtin::detail::rats_reg);
    engine.registerFunction("regexp",     &builtin::detail::regexp_reg);
    engine.registerFunction("regexpi",    &builtin::detail::regexpi_reg);
    engine.registerFunction("regexprep",  &builtin::detail::regexprep_reg);
    engine.registerFunction("regexptranslate", &builtin::detail::regexptranslate_reg);

    // ── Phase 6c: types.cpp public-API-backed built-ins ────────────
    engine.registerFunction("double",    &builtin::detail::double_reg);
    engine.registerFunction("single",    &builtin::detail::single_reg);
    engine.registerFunction("int8",      &builtin::detail::int8_reg);
    engine.registerFunction("int16",     &builtin::detail::int16_reg);
    engine.registerFunction("int32",     &builtin::detail::int32_reg);
    engine.registerFunction("int64",     &builtin::detail::int64_reg);
    engine.registerFunction("uint8",     &builtin::detail::uint8_reg);
    engine.registerFunction("uint16",    &builtin::detail::uint16_reg);
    engine.registerFunction("uint32",    &builtin::detail::uint32_reg);
    engine.registerFunction("uint64",    &builtin::detail::uint64_reg);
    engine.registerFunction("logical",   &builtin::detail::logical_reg);
    engine.registerFunction("cast",      &builtin::detail::cast_reg);
    engine.registerFunction("swapbytes", &builtin::detail::swapbytes_reg);
    engine.registerFunction("typecast",  &builtin::detail::typecast_reg);
    engine.registerFunction("isnumeric", &builtin::detail::isnumeric_reg);
    engine.registerFunction("islogical", &builtin::detail::islogical_reg);
    engine.registerFunction("ischar",    &builtin::detail::ischar_reg);
    engine.registerFunction("isstring",  &builtin::detail::isstring_reg);
    engine.registerFunction("iscell",    &builtin::detail::iscell_reg);
    engine.registerFunction("isstruct",  &builtin::detail::isstruct_reg);
    engine.registerFunction("isempty",   &builtin::detail::isempty_reg);
    engine.registerFunction("isscalar",  &builtin::detail::isscalar_reg);
    engine.registerFunction("isreal",    &builtin::detail::isreal_reg);
    engine.registerFunction("isinteger", &builtin::detail::isinteger_reg);
    engine.registerFunction("isfloat",   &builtin::detail::isfloat_reg);
    engine.registerFunction("issingle",  &builtin::detail::issingle_reg);
    engine.registerFunction("issparse",  &builtin::detail::issparse_reg);
    engine.registerFunction("isnan",     &builtin::detail::isnan_reg);
    engine.registerFunction("isinf",     &builtin::detail::isinf_reg);
    engine.registerFunction("isfinite",  &builtin::detail::isfinite_reg);
    engine.registerFunction("ismissing", &builtin::detail::ismissing_reg);
    engine.registerFunction("anymissing",&builtin::detail::anymissing_reg);
    engine.registerFunction("standardizeMissing", &builtin::detail::standardizeMissing_reg);
    engine.registerFunction("isvector",   &builtin::detail::isvector_reg);
    engine.registerFunction("isrow",      &builtin::detail::isrow_reg);
    engine.registerFunction("iscolumn",   &builtin::detail::iscolumn_reg);
    engine.registerFunction("ismatrix",   &builtin::detail::ismatrix_reg);
    engine.registerFunction("issorted",   &builtin::detail::issorted_reg);
    engine.registerFunction("issortedrows",&builtin::detail::issortedrows_reg);
    engine.registerFunction("isuniform",  &builtin::detail::isuniform_reg);
    // issymmetric / ishermitian / isbanded / isdiag / istril / istriu /
    // bandwidth registered by LinalgLibrary::install (libs/linalg).
    // vecnorm registered by LinalgLibrary::install (libs/linalg).
    // compat aliases — same fns reachable via `import compat.*`.
    // compat.{issymmetric,ishermitian,isbanded,isdiag,istril,istriu,bandwidth}
    // registered by LinalgLibrary::install (libs/linalg).
    // compat.vecnorm registered by LinalgLibrary::install (libs/linalg).
    // rref / planerot + compat aliases registered by
    // LinalgLibrary::install (libs/linalg).
    // ldl / compat.ldl registered by LinalgLibrary::install (libs/linalg).
    // lsqminnorm / lsqnonneg + compat aliases registered by
    // LinalgLibrary::install (libs/linalg).
    // balance registered by LinalgLibrary::install (libs/linalg).
    // compat.balance registered by LinalgLibrary::install (libs/linalg).
    engine.registerFunction("flintmax",   &builtin::detail::flintmax_reg);
    engine.registerFunction("intmax",     &builtin::detail::intmax_reg);
    engine.registerFunction("intmin",     &builtin::detail::intmin_reg);
    engine.registerFunction("realmax",    &builtin::detail::realmax_reg);
    engine.registerFunction("realmin",    &builtin::detail::realmin_reg);
    engine.registerFunction("allfinite",  &builtin::detail::allfinite_reg);
    engine.registerFunction("anynan",     &builtin::detail::anynan_reg);
    engine.registerFunction("isequal",   &builtin::detail::isequal_reg);
    engine.registerFunction("isequaln",  &builtin::detail::isequaln_reg);
    // `class` registered in registerWorkspaceBuiltins() as a lambda
    // (formats type via mtypeName, more elaborate than the bare reg).

    // ── Phase 6c: datatypes/strings/format.cpp public-API-backed built-ins ───────────
    engine.registerFunction("sprintf",    &builtin::detail::sprintf_reg);

    // ── Phase 6c: print.cpp public-API-backed built-ins ────────────
    engine.registerFunction("disp",       &builtin::detail::disp_reg);
    engine.registerFunction("fprintf",    &builtin::detail::fprintf_reg);

    // fopen / fclose / fread / fwrite / fgetl / fgets / feof / ferror /
    // ftell / fseek / frewind / csvread / csvwrite / save / load all
    // moved to libs/io (Phase 7c). Their registrations live in
    // IoLibrary::install (called from Engine::Engine ctor).

    // ── scan.cpp public-API-backed built-ins ───────────────────────
    engine.registerFunction("fscanf",     &builtin::detail::fscanf_reg);
    engine.registerFunction("sscanf",     &builtin::detail::sscanf_reg);
    engine.registerFunction("textscan",   &builtin::detail::textscan_reg);

    // ── env.cpp public-API-backed built-ins ────────────────────────
    engine.registerFunction("setenv",     &builtin::detail::setenv_reg);
    engine.registerFunction("getenv",     &builtin::detail::getenv_reg);

    // ── programming/errors/diagnostics.cpp public-API-backed built-ins ──────
    engine.registerFunction("error",      &builtin::detail::error_reg);
    engine.registerFunction("warning",    &builtin::detail::warning_reg);
    engine.registerFunction("lastwarn",   &builtin::detail::lastwarn_reg);
    engine.registerFunction("MException", &builtin::detail::MException_reg);
    engine.registerFunction("rethrow",    &builtin::detail::rethrow_reg);
    engine.registerFunction("throw",      &builtin::detail::throw_reg);
    engine.registerFunction("assert",     &builtin::detail::assert_reg);

    // ── Phase 6c: datatypes/{cell,struct}/ public-API-backed built-ins ───────
    engine.registerFunction("struct",     &builtin::detail::struct_reg);
    engine.registerFunction("fieldnames", &builtin::detail::fieldnames_reg);
    engine.registerFunction("isfield",    &builtin::detail::isfield_reg);
    engine.registerFunction("rmfield",    &builtin::detail::rmfield_reg);
    engine.registerFunction("cell",       &builtin::detail::cell_reg);
    engine.registerFunction("cellfun",    &builtin::detail::cellfun_reg);
    engine.registerFunction("num2cell",   &builtin::detail::num2cell_reg);
    engine.registerFunction("cell2mat",   &builtin::detail::cell2mat_reg);
    engine.registerFunction("iscellstr",  &builtin::detail::iscellstr_reg);
    engine.registerFunction("cellstr",    &builtin::detail::cellstr_reg);
    engine.registerFunction("mat2cell",   &builtin::detail::mat2cell_reg);

    // ── Pack 24: deal + celldisp (lambdas) ────────────────────────────
    // deal — distribute inputs to outputs. Single input → broadcast to
    // all outputs; multiple inputs → 1-to-1 with outs.
    engine.registerFunction("deal",
        [](Span<const Value> args, size_t nargout,
           Span<Value> outs, CallContext &) {
            if (args.empty())
                throw std::runtime_error("deal requires at least 1 argument");
            if (args.size() == 1) {
                for (size_t i = 0; i < nargout && i < outs.size(); ++i)
                    outs[i] = args[0];
                return;
            }
            const size_t n = std::min(nargout, args.size());
            for (size_t i = 0; i < n && i < outs.size(); ++i)
                outs[i] = args[i];
        });

    // celldisp(c[, name]) — print each cell's contents.
    engine.registerFunction("celldisp",
        [](Span<const Value> args, size_t, Span<Value>, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("celldisp requires 1 argument");
            const Value &c = args[0];
            if (!c.isCell())
                throw std::runtime_error("celldisp: input must be a cell");
            const std::string name = (args.size() >= 2)
                                          ? args[1].toString()
                                          : std::string("ans");
            for (size_t i = 0; i < c.numel(); ++i) {
                ctx.engine->outputText(name + "{" + std::to_string(i + 1) + "} =\n");
                ctx.engine->outputText(c.cellAt(i).formatDisplay("") + "\n");
            }
        });
    engine.registerFunction("structfun",  &builtin::detail::structfun_reg);
    engine.registerFunction("getfield",   &builtin::detail::getfield_reg);
    engine.registerFunction("setfield",   &builtin::detail::setfield_reg);
    engine.registerFunction("orderfields",&builtin::detail::orderfields_reg);
    engine.registerFunction("struct2cell",&builtin::detail::struct2cell_reg);
    engine.registerFunction("cell2struct",&builtin::detail::cell2struct_reg);

    // arrayfun(@fn, A [, B, ...] [, 'UniformOutput', true|false])
    //
    // For each element of A (and any additional input arrays B, C,
    // ...), invokes fn with the per-position scalar values and
    // collects the results. UniformOutput=true (the default) packs
    // scalar results into a numeric array of the same shape as A;
    // false collects them in a cell array.
    //
    // Earlier this function was a stub that returned A verbatim,
    // ignoring fn — see BUGS.md #11. The real lambda body is now
    // applied via Engine::callFunctionHandle, the same path
    // cellfun/structfun already use.
    engine.registerFunction("arrayfun",
                            [](Span<const Value> args,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                (void)nargout;
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "arrayfun requires at least 2 arguments");
                                if (!args[0].isFuncHandle())
                                    throw std::runtime_error(
                                        "arrayfun: first argument must be a function handle");
                                const Value &handle = args[0];

                                // Collect input arrays + parse trailing
                                // 'UniformOutput' / 'ErrorHandler' N-V pairs.
                                bool uniformOutput = true;
                                std::vector<const Value *> inputs;
                                inputs.reserve(args.size() - 1);
                                for (size_t i = 1; i < args.size(); ++i) {
                                    if (args[i].isChar() && i + 1 < args.size()) {
                                        std::string key = args[i].toString();
                                        for (auto &c : key)
                                            c = (char)std::tolower((unsigned char)c);
                                        if (key == "uniformoutput") {
                                            uniformOutput = args[i + 1].toScalar() != 0.0;
                                            ++i;   // skip the value
                                            continue;
                                        }
                                        if (key == "errorhandler") {
                                            // not modelled; just skip
                                            ++i;
                                            continue;
                                        }
                                    }
                                    inputs.push_back(&args[i]);
                                }
                                if (inputs.empty())
                                    throw std::runtime_error(
                                        "arrayfun: at least one input array required");

                                const size_t n = inputs[0]->numel();
                                for (const auto *p : inputs) {
                                    if (p->numel() != n)
                                        throw std::runtime_error(
                                            "arrayfun: all input arrays must be the same size");
                                }
                                auto *mr = ctx.engine->resource();

                                // Walk every element. Per call, build a
                                // scalar-Value arg list and invoke the handle.
                                std::vector<Value> callArgs(inputs.size());
                                if (uniformOutput) {
                                    auto out = Value::matrix(inputs[0]->dims().rows(),
                                                             inputs[0]->dims().cols(),
                                                             ValueType::DOUBLE, mr);
                                    for (size_t i = 0; i < n; ++i) {
                                        for (size_t k = 0; k < inputs.size(); ++k)
                                            callArgs[k] = Value::scalar(
                                                inputs[k]->elemAsDouble(i), mr);
                                        Value r = ctx.engine->callFunctionHandle(
                                            handle,
                                            Span<const Value>(callArgs.data(), callArgs.size()),
                                            ctx.env);
                                        out.doubleDataMut()[i] = r.toScalar();
                                    }
                                    outs[0] = std::move(out);
                                } else {
                                    // UniformOutput=false → result is a CELL
                                    // of size matching A.
                                    auto cell = Value::cell(inputs[0]->dims().rows(),
                                                            inputs[0]->dims().cols(), mr);
                                    for (size_t i = 0; i < n; ++i) {
                                        for (size_t k = 0; k < inputs.size(); ++k)
                                            callArgs[k] = Value::scalar(
                                                inputs[k]->elemAsDouble(i), mr);
                                        Value r = ctx.engine->callFunctionHandle(
                                            handle,
                                            Span<const Value>(callArgs.data(), callArgs.size()),
                                            ctx.env);
                                        cell.cellAt(i) = std::move(r);
                                    }
                                    outs[0] = std::move(cell);
                                }
                            });

    // ── Pack 13: function handles ─────────────────────────────────────
    // feval(handle_or_name, args...) — invoke through the engine's
    // existing handle-call path. Accepts either a real function handle
    // or a name/string (str2func it on the fly).
    engine.registerFunction("feval",
                            [](Span<const Value> args, size_t nargout,
                               Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("feval requires at least 1 argument");
                                Value handle;
                                if (args[0].isFuncHandle()) {
                                    handle = args[0];
                                } else if (args[0].isChar() || args[0].isString()) {
                                    handle = Value::funcHandle(args[0].toString(),
                                                                ctx.engine->resource());
                                } else {
                                    throw std::runtime_error(
                                        "feval: first argument must be a function handle or name");
                                }
                                Span<const Value> callArgs(args.data() + 1, args.size() - 1);
                                if (nargout <= 1) {
                                    outs[0] = ctx.engine->callFunctionHandle(handle, callArgs, ctx.env);
                                } else {
                                    auto rs = ctx.engine->callFunctionHandleMulti(
                                        handle, callArgs, nargout, ctx.env);
                                    for (size_t i = 0; i < nargout && i < outs.size() && i < rs.size(); ++i)
                                        outs[i] = std::move(rs[i]);
                                }
                            });

    // str2func('name') — create a function handle by name.
    engine.registerFunction("str2func",
                            [](Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("str2func requires 1 argument");
                                if (!args[0].isChar() && !args[0].isString())
                                    throw std::runtime_error(
                                        "str2func: argument must be a string");
                                outs[0] = Value::funcHandle(args[0].toString(),
                                                             ctx.engine->resource());
                            });

    // func2str(@fn) — recover the name of a function handle.
    // MATLAB: named handles (`@sin`) return just the bare name `'sin'`;
    // anonymous handles (`@(x) x*2`) return the full `'@(x)x*2'` source
    // text. We don't store anon source text, so fall back to the
    // internal `__anon_<N>` name with `@` prefix for those — best-
    // effort placeholder. See BUGS.md #16.
    engine.registerFunction("func2str",
                            [](Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("func2str requires 1 argument");
                                if (!args[0].isFuncHandle())
                                    throw std::runtime_error(
                                        "func2str: argument must be a function handle");
                                const std::string name = args[0].funcHandleName();
                                // Detect anon-handle naming convention: parser
                                // assigns `__anon_<N>` to lambdas. Named
                                // handles (sin, foo, etc.) get the bare name.
                                const bool isAnon = name.rfind("__anon_", 0) == 0;
                                outs[0] = Value::fromString(
                                    isAnon ? ("@" + name) : name,
                                    ctx.engine->resource());
                            });
}

// ============================================================
// Workspace / session builtins
//
// These were previously handled only by TreeWalker::tryBuiltinCall(),
// which meant the VM could never execute them. By registering them
// as externalFuncs, both backends can dispatch them uniformly
// through the standard CALL opcode.
// ============================================================

void BuiltinLibrary::registerWorkspaceBuiltins(Engine &engine)
{
    // ── clear ──────────────────────────────────────────────────
    engine.registerFunction("clear",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                auto *env = ctx.env;
                                bool insideFunc = ctx.engine->isInsideFunctionCall();

                                if (args.empty()) {
                                    // MATLAB: bare 'clear' clears workspace variables only,
                                    // NOT user functions or figures.
                                    env->clearAll();
                                    if (!insideFunc) {
                                        ctx.engine->reinstallConstants();
                                        ctx.engine->markClearAll();
                                    }
                                } else {
                                    std::string first = args[0].isChar() ? args[0].toString() : "";

                                    if (first == "-regexp") {
                                        // clear -regexp pat1 pat2 ... — drop every workspace
                                        // variable whose name matches at least one pattern.
                                        // MATLAB uses regexp-style partial match (so `^foo`
                                        // matches "foo1"), not whole-string regex_match.
                                        // std::regex is slow but the workspace is tiny.
                                        std::vector<std::regex> pats;
                                        for (size_t i = 1; i < args.size(); ++i) {
                                            if (!args[i].isChar() && !args[i].isString()) continue;
                                            try { pats.emplace_back(args[i].toString()); }
                                            catch (const std::regex_error &) {
                                                throw std::runtime_error(
                                                    "clear -regexp: invalid pattern '"
                                                    + args[i].toString() + "'");
                                            }
                                        }
                                        // Inside a function `env` is the local frame; the
                                        // user's intent for `clear -regexp` is the SAME
                                        // workspace they'd see via plain `clear x`. Apply
                                        // to both env and (when distinct) the engine's base
                                        // workspace so VM-mode top-level evals work too.
                                        auto applyTo = [&](Environment *e) {
                                            if (!e) return;
                                            for (const auto &n : e->localNames()) {
                                                for (const auto &re : pats) {
                                                    if (std::regex_search(n, re)) {
                                                        e->remove(n);
                                                        break;
                                                    }
                                                }
                                            }
                                        };
                                        applyTo(env);
                                        if (env != &ctx.engine->workspaceEnv())
                                            applyTo(&ctx.engine->workspaceEnv());
                                        outs[0] = Value::empty();
                                        return;
                                    }
                                    if (first == "global") {
                                        auto *gs = ctx.env->globalsEnv();
                                        if (args.size() == 1) {
                                            // clear global — clear all globals
                                            if (gs)
                                                gs->clearAll();
                                            env->clearAll();
                                            ctx.engine->markClearAll();
                                        } else {
                                            // clear global x y — clear specific globals
                                            for (size_t i = 1; i < args.size(); ++i) {
                                                if (args[i].isChar()) {
                                                    std::string gname = args[i].toString();
                                                    if (gs)
                                                        gs->remove(gname);
                                                    env->remove(gname);
                                                }
                                            }
                                        }
                                        outs[0] = Value::empty();
                                        return;
                                    }
                                    if (first == "import") {
                                        // Drop every active import in the current scope.
                                        // Subsequent unqualified lookups fall back to core +
                                        // parent-scope imports (if any).
                                        env->clearImports();
                                        outs[0] = Value::empty();
                                        return;
                                    }

                                    if (first == "all" || first == "classes") {
                                        if (insideFunc) {
                                            env->clearAll();
                                        } else {
                                            env->clearAll();
                                            ctx.engine->clearUserFunctions();
                                            ctx.engine->figureManager().closeAll();
                                            ctx.engine->reinstallConstants();
                                            ctx.engine->markClearAll();
                                        }
                                    } else if (first == "functions") {
                                        if (!insideFunc)
                                            ctx.engine->clearUserFunctions();
                                    } else {
                                        // `clear x`, `clear pi`, etc.
                                        // Un-shadow a built-in by removing the
                                        // workspace slot — the next read then
                                        // falls back to constantsEnv_. No
                                        // special filtering: MATLAB allows it.
                                        for (auto &a : args) {
                                            if (a.isChar())
                                                env->remove(a.toString());
                                        }
                                    }
                                }
                                outs[0] = Value::empty();
                            });

    // ── import ────────────────────────────────────────────────
    // Command-style: `import signal.*` → import('signal.*').
    // Function-style: `import('signal', 'as', 's')` → alias form.
    // Each string arg is one of:
    //   'a.b.c'   — single-symbol import (path = [a, b, c])
    //   'a.b.*'   — wildcard import      (path = [a, b], wildcard=true)
    //   3-arg form 'a.b' / 'as' / 'name'  → alias
    // Multiple args allowed: `import a.* b.*` pushes two imports.
    engine.registerFunction(
        "import", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            auto fail = [](const std::string &msg) {
                throw std::runtime_error("import: " + msg);
            };
            auto asString = [&](const Value &v, size_t i) {
                if (!v.isChar() && !v.isString())
                    fail("argument " + std::to_string(i + 1) + " must be a string");
                return v.toString();
            };
            auto parseSpec = [&](const std::string &spec, Import &imp) {
                if (spec.empty()) fail("empty import specifier");
                size_t pos = 0;
                while (pos < spec.size()) {
                    size_t dot = spec.find('.', pos);
                    std::string seg = spec.substr(pos, dot == std::string::npos ? std::string::npos
                                                                                : dot - pos);
                    if (seg == "*") {
                        if (dot != std::string::npos)
                            fail("'*' must be the last component in '" + spec + "'");
                        imp.wildcard = true;
                        return;
                    }
                    if (seg.empty())
                        fail("empty path component in '" + spec + "'");
                    imp.path.push_back(std::move(seg));
                    if (dot == std::string::npos) break;
                    pos = dot + 1;
                }
                if (imp.path.empty()) fail("missing path in '" + spec + "'");
            };

            if (args.empty()) fail("requires at least one argument");

            // Alias form: import('a.b', 'as', 'name') — exactly 3 args, args[1] == 'as'.
            if (args.size() == 3 && (args[1].isChar() || args[1].isString())
                && args[1].toString() == "as") {
                Import imp;
                parseSpec(asString(args[0], 0), imp);
                if (imp.wildcard) fail("'as' alias is not allowed with wildcard import");
                imp.alias = asString(args[2], 2);
                if (imp.alias.empty()) fail("alias name must be non-empty");
                ctx.env->pushImport(std::move(imp));
                outs[0] = Value::empty();
                return;
            }

            for (size_t i = 0; i < args.size(); ++i) {
                std::string spec = asString(args[i], i);
                Import imp;
                parseSpec(spec, imp);
                ctx.env->pushImport(std::move(imp));
            }
            outs[0] = Value::empty();
        });

    // ── assignin ──────────────────────────────────────────────
    // assignin(workspace, name, val) — write `name = val` in
    // `workspace`, where `workspace` is 'base' (top-level) or 'caller'
    // (the workspace of the function that called the function
    // containing this assignin). VM mode also write-throughs to the
    // target frame's register if `name` is statically allocated, so
    // subsequent register-based reads in the target pick up the value.
    engine.registerFunction(
        "assignin", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() != 3)
                throw std::runtime_error("assignin: requires 3 arguments (workspace, name, value)");
            if (!args[0].isChar())
                throw std::runtime_error("assignin: workspace must be 'base' or 'caller'");
            if (!args[1].isChar())
                throw std::runtime_error("assignin: name must be a string");
            std::string where = args[0].toString();
            std::string name = args[1].toString();
            if (name.empty())
                throw std::runtime_error("assignin: name must be non-empty");
            if (where == "base") {
                ctx.engine->workspaceEnv().set(name, args[2]);
            } else if (where == "caller") {
                // 'caller' is invalid when assignin is called directly
                // from the base workspace — there's no enclosing
                // function to take a caller of (matches MATLAB's
                // "ASSIGNIN cannot have 'caller' as a workspace when
                // used in the base workspace").
                if (ctx.engine->callerDepth() < 1)
                    throw std::runtime_error(
                        "assignin: 'caller' is not valid in the base workspace");
                // Depth 1 = the function that called the function
                // containing this assignin call. Depth 0 would be the
                // assignin-containing function itself.
                ctx.engine->assignToCaller(1, name, args[2]);
            } else {
                throw std::runtime_error(
                    "assignin: workspace must be 'base' or 'caller', got '" + where + "'");
            }
            outs[0] = Value::empty();
        });

    // ── inputname ────────────────────────────────────────────
    // inputname(k) returns the variable name of the k-th input arg as
    // written at the call site of the function containing this call.
    // Empty string if the arg was a literal / expression / non-identifier.
    // Throws when called from outside a function or for k < 1.
    engine.registerFunction(
        "inputname", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() != 1)
                throw std::runtime_error("inputname: requires one argument (k)");
            double kd = args[0].toScalar();
            int k = static_cast<int>(kd);
            if (static_cast<double>(k) != kd || k < 1)
                throw std::runtime_error("inputname: k must be a positive integer");
            std::string name = ctx.engine->inputName(k);
            outs[0] = Value::fromString(name, ctx.engine->resource());
        });

    // ── clc ────────────────────────────────────────────────────
    engine.registerFunction("clc",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
                                ctx.engine->outputText("__CLEAR__\n");
                                outs[0] = Value::empty();
                            });

    // ── who ────────────────────────────────────────────────────
    engine.registerFunction("who",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // -file <fname>: in our ASCII save format the file
                                // contains a single matrix; load() would assign it to
                                // a workspace variable named after the file stem.
                                // Mirror that contract here.
                                if (!args.empty() && args[0].isChar() && args[0].toString() == "-file") {
                                    if (args.size() < 2 || !args[1].isChar())
                                        throw std::runtime_error("who -file requires a filename");
                                    std::string fname = args[1].toString();
                                    auto rp = ctx.engine->resolvePath(fname);
                                    if (!rp.fs || !rp.fs->exists(rp.path))
                                        throw std::runtime_error("who -file: file not found: " + fname);
                                    std::string stem = fname;
                                    size_t sep = stem.find_last_of("/\\:");
                                    if (sep != std::string::npos) stem = stem.substr(sep + 1);
                                    size_t dot = stem.find_last_of('.');
                                    if (dot != std::string::npos && dot > 0) stem = stem.substr(0, dot);
                                    std::ostringstream os;
                                    os << "\nYour variables are:\n\n" << stem << "  \n\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value::empty();
                                    return;
                                }

                                ScratchArena scratch(ctx.engine->resource());
                                ScratchVec<std::string> names(&scratch);
                                // Pseudo-vars set by callUserFunction (nargin /
                                // nargout) shouldn't appear in `who` output —
                                // matches MATLAB.
                                auto isPseudo = [](const std::string &n) {
                                    return n == "nargin" || n == "nargout";
                                };
                                if (args.empty()) {
                                    // localNames() excludes parent-env constants
                                    // (pi, eps, …) — they show up here only if
                                    // shadowed in the workspace, as in MATLAB.
                                    auto src = env->localNames();
                                    for (auto &n : src)
                                        if (!isPseudo(n)) names.push_back(n);
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName) && !isPseudo(varName))
                                                names.push_back(varName);
                                        }
                                    }
                                }
                                std::sort(names.begin(), names.end());

                                std::ostringstream os;
                                if (!names.empty()) {
                                    os << "\nYour variables are:\n\n";
                                    for (auto &n : names)
                                        os << n << "  ";
                                    os << "\n\n";
                                }
                                ctx.engine->outputText(os.str());
                                outs[0] = Value::empty();
                            });

    // ── whos ───────────────────────────────────────────────────
    engine.registerFunction("whos",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // -file <fname>: ASCII save format holds a single matrix
                                // assigned to the file's stem. Surface that as a one-row
                                // listing with a usable bytes/size estimate from stat().
                                if (!args.empty() && args[0].isChar() && args[0].toString() == "-file") {
                                    if (args.size() < 2 || !args[1].isChar())
                                        throw std::runtime_error("whos -file requires a filename");
                                    std::string fname = args[1].toString();
                                    auto rp = ctx.engine->resolvePath(fname);
                                    if (!rp.fs || !rp.fs->exists(rp.path))
                                        throw std::runtime_error("whos -file: file not found: " + fname);
                                    auto st = rp.fs->stat(rp.path);
                                    int64_t bytes = st ? st->size : 0;
                                    std::string stem = fname;
                                    size_t sep = stem.find_last_of("/\\:");
                                    if (sep != std::string::npos) stem = stem.substr(sep + 1);
                                    size_t dot = stem.find_last_of('.');
                                    if (dot != std::string::npos && dot > 0) stem = stem.substr(0, dot);
                                    std::ostringstream os;
                                    os << "  Name      Size    Bytes  Class\n";
                                    os << "  " << std::left << std::setw(8) << stem
                                       << "  ?       " << bytes << "  double\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value::empty();
                                    return;
                                }

                                ScratchArena scratch(ctx.engine->resource());
                                ScratchVec<std::string> names(&scratch);
                                auto isPseudo = [](const std::string &n) {
                                    return n == "nargin" || n == "nargout";
                                };
                                if (args.empty()) {
                                    auto src = env->localNames();
                                    for (auto &n : src)
                                        if (!isPseudo(n)) names.push_back(n);
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName) && !isPseudo(varName))
                                                names.push_back(varName);
                                        }
                                    }
                                }
                                std::sort(names.begin(), names.end());

                                std::ostringstream os;
                                if (!names.empty()) {
                                    os << "  Name" << std::string(6, ' ') << "Size"
                                       << std::string(13, ' ') << "Bytes  Class"
                                       << std::string(5, ' ') << "Attributes\n\n";
                                    for (auto &n : names) {
                                        auto *val = env->get(n);
                                        if (!val)
                                            continue;
                                        auto &d = val->dims();
                                        std::string sizeStr = std::to_string(d.rows()) + "x"
                                                              + std::to_string(d.cols());
                                        if (d.is3D())
                                            sizeStr += "x" + std::to_string(d.pages());
                                        std::string bytesStr = std::to_string(val->rawBytes());
                                        std::string classStr = mtypeName(val->type());
                                        std::string attrStr;
                                        if (env->isGlobal(n))
                                            attrStr = "global";

                                        os << "  " << n;
                                        for (size_t i = n.size(); i < 10; ++i)
                                            os << " ";
                                        os << sizeStr;
                                        for (size_t i = sizeStr.size(); i < 17; ++i)
                                            os << " ";
                                        for (size_t i = bytesStr.size(); i < 5; ++i)
                                            os << " ";
                                        os << bytesStr << "  " << classStr;
                                        for (size_t i = classStr.size(); i < 10; ++i)
                                            os << " ";
                                        os << attrStr << "\n";
                                    }
                                    os << "\n";
                                }
                                ctx.engine->outputText(os.str());
                                outs[0] = Value::empty();
                            });

    // ── which ──────────────────────────────────────────────────
    engine.registerFunction("which",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("which requires a name argument");
                                std::string qname = args[0].isChar() ? args[0].toString() : "";
                                auto *env = ctx.env;

                                std::ostringstream os;
                                if (env->getLocal(qname)
                                    || (env->isGlobal(qname) && env->globalsEnv()
                                        && env->globalsEnv()->get(qname)))
                                    os << qname << " is a variable.\n";
                                else if (ctx.engine->hasUserFunction(qname))
                                    os << qname << " is a user-defined function.\n";
                                else if (ctx.engine->hasExternalFunction(qname))
                                    os << "built-in (" << qname << ")\n";
                                else {
                                    // M-file lookup via Engine path registry.
                                    bool found = false;
                                    for (const auto &dir : ctx.engine->path()) {
                                        std::string p = dir;
                                        if (!p.empty() && p.back() != '/' && p.back() != '\\') p += '/';
                                        p += qname + ".m";
                                        try {
                                            auto rp = ctx.engine->resolvePath(p);
                                            if (rp.fs && rp.fs->exists(rp.path)) {
                                                os << p << "\n";
                                                found = true;
                                                break;
                                            }
                                        } catch (...) {}
                                    }
                                    if (!found)
                                        os << "'" << qname << "' not found.\n";
                                }

                                ctx.engine->outputText(os.str());
                                outs[0] = Value::empty();
                            });

    // ── exist ──────────────────────────────────────────────────
    engine.registerFunction("exist",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("exist requires a name argument");
                                std::string varName = args[0].toString();
                                auto *env = ctx.env;

                                // Optional second argument: type filter
                                std::string typeFilter;
                                if (args.size() >= 2 && args[1].isChar())
                                    typeFilter = args[1].toString();

                                if (typeFilter == "class") {
                                    // No user-defined classdef yet (no class system in
                                    // numkit::Engine). Returning 0 matches MATLAB's
                                    // behaviour for unknown class names — scripts that
                                    // probe for a class get a benign "no" rather than
                                    // a fatal "unsupported".
                                    outs[0] = Value::scalar(0.0, ctx.engine->resource());
                                    return;
                                }

                                auto vfsExists = [&](const std::string &p) -> bool {
                                    try {
                                        auto rp = ctx.engine->resolvePath(p);
                                        return rp.fs && rp.fs->exists(rp.path);
                                    } catch (...) { return false; }
                                };
                                auto vfsIsDir = [&](const std::string &p) -> bool {
                                    try {
                                        auto rp = ctx.engine->resolvePath(p);
                                        if (!rp.fs) return false;
                                        auto st = rp.fs->stat(rp.path);
                                        return st && st->kind == FileStat::Kind::Directory;
                                    } catch (...) { return false; }
                                };

                                double code = 0;
                                // Check local scope only for variables (don't leak to parent)
                                bool isVar = (env->getLocal(varName) != nullptr);
                                if (!isVar && env->isGlobal(varName)) {
                                    auto *gs = env->globalsEnv();
                                    isVar = (gs && gs->get(varName) != nullptr);
                                }
                                bool isFunc = ctx.engine->hasFunction(varName);

                                // Walk path list to check for `<name>.m` (m-file resolver)
                                auto findMFile = [&]() -> bool {
                                    for (const auto &dir : ctx.engine->path()) {
                                        std::string p = dir;
                                        if (!p.empty() && p.back() != '/' && p.back() != '\\') p += '/';
                                        p += varName + ".m";
                                        if (vfsExists(p)) return true;
                                    }
                                    return false;
                                };

                                if (typeFilter.empty()) {
                                    if (isVar)              code = 1;
                                    else if (isFunc)        code = 5;
                                    else if (vfsExists(varName)) code = 2;       // file
                                    else if (findMFile())   code = 2;            // m-file in path
                                } else if (typeFilter == "var") {
                                    if (isVar)              code = 1;
                                } else if (typeFilter == "builtin") {
                                    if (ctx.engine->hasExternalFunction(varName)) code = 5;
                                } else if (typeFilter == "file") {
                                    // Direct path — file or m-file in search path
                                    if (vfsExists(varName))                       code = 2;
                                    else if (findMFile())                          code = 2;
                                } else if (typeFilter == "dir") {
                                    if (vfsIsDir(varName))                        code = 7;
                                }

                                outs[0] = Value::scalar(code, ctx.engine->resource());
                            });

    // ── class ──────────────────────────────────────────────────
    engine.registerFunction("class",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("class requires an argument");
                                outs[0] = Value::fromString(mtypeName(args[0].type()),
                                                             ctx.engine->resource());
                            });

    // ── tic ────────────────────────────────────────────────────
    engine.registerFunction("tic",
                            [](Span<const Value>,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                auto now = Clock::now();
                                ctx.engine->setTicTimer(now);
                                if (nargout > 0) {
                                    double id = static_cast<double>(
                                        std::chrono::duration_cast<std::chrono::microseconds>(
                                            now.time_since_epoch())
                                            .count());
                                    outs[0] = Value::scalar(id, ctx.engine->resource());
                                } else {
                                    outs[0] = Value::empty();
                                }
                            });

    // ── toc ────────────────────────────────────────────────────
    engine.registerFunction("toc",
                            [](Span<const Value> args,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                auto now = Clock::now();
                                TimePoint start;
                                if (!args.empty() && args[0].isScalar()) {
                                    auto us = static_cast<long long>(args[0].toScalar());
                                    start = TimePoint(std::chrono::microseconds(us));
                                } else if (ctx.engine->ticWasCalled()) {
                                    start = ctx.engine->ticTimer();
                                } else {
                                    throw std::runtime_error(
                                        "toc: You must call 'tic' before calling 'toc'.");
                                }
                                double elapsed = std::chrono::duration<double>(now - start).count();
                                if (nargout > 0) {
                                    outs[0] = Value::scalar(elapsed, ctx.engine->resource());
                                } else {
                                    std::ostringstream os;
                                    os << "Elapsed time is " << elapsed << " seconds.\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value::empty();
                                }
                            });

    // ── cputime ───────────────────────────────────────────────
    // MATLAB cputime: total CPU seconds used by the current process
    // since startup. std::clock() is the standard portable handle.
    engine.registerFunction("cputime",
                            [](Span<const Value>,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                const double t = static_cast<double>(std::clock())
                                               / static_cast<double>(CLOCKS_PER_SEC);
                                outs[0] = Value::scalar(t, ctx.engine->resource());
                            });

    // ── now ───────────────────────────────────────────────────
    // MATLAB now: serial date number for current local time.
    // Days since 0000-01-00 (MATLAB epoch). 1970-01-01 maps to 719529.
    //   now = 719529 + (Unix microseconds) / 86_400_000_000
    // (MATLAB has deprecated `now` in favour of datetime() but many
    // scripts still call it.)
    engine.registerFunction("now",
                            [](Span<const Value>,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                const auto unix_us = std::chrono::duration_cast<
                                    std::chrono::microseconds>(
                                    std::chrono::system_clock::now().time_since_epoch()).count();
                                const double serial =
                                    719529.0 + static_cast<double>(unix_us) / 86400000000.0;
                                outs[0] = Value::scalar(serial, ctx.engine->resource());
                            });

    // ── datenum ───────────────────────────────────────────────
    // MATLAB datenum: serial date number from date components.
    //
    // Supported forms (string-parse forms deferred):
    //   datenum(Y, M, D)
    //   datenum(Y, M, D, H, MI, S)
    //   datenum(V)               with V row 1x3, 1x6, or matrix Nx3 / Nx6
    //
    // Algorithm: Howard Hinnant's `days_from_civil` (proleptic Gregorian)
    // returns days since 1970-01-01; add 719529 for the MATLAB epoch
    // (1 = 0000-01-01, MATLAB's "year zero" reference). Month/day overflow
    // wraps naturally, matching MATLAB behaviour.
    engine.registerFunction("datenum",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "datenum requires at least one argument");

                                auto civilToSerial = [](double yd, double md,
                                                        double dd, double hd,
                                                        double mind, double sd) {
                                    // Floor day to integer; fractional part
                                    // contributes to time-of-day fraction.
                                    double dInt;
                                    const double dFrac = std::modf(dd, &dInt);
                                    int64_t y = static_cast<int64_t>(std::floor(yd));
                                    int64_t m = static_cast<int64_t>(std::floor(md));
                                    int64_t d = static_cast<int64_t>(dInt);
                                    if (m <= 2) y -= 1;
                                    const int64_t era = (y < 0 ? y - 399 : y) / 400;
                                    const int64_t yoe = y - era * 400;
                                    const int64_t doy =
                                        (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5
                                        + d - 1;
                                    const int64_t doe =
                                        yoe * 365 + yoe / 4 - yoe / 100 + doy;
                                    const int64_t days =
                                        era * 146097 + doe - 719468;
                                    const double frac =
                                        (hd * 3600.0 + mind * 60.0 + sd) / 86400.0
                                        + dFrac;
                                    return static_cast<double>(days) + 719529.0
                                         + frac;
                                };

                                auto *mr = ctx.engine->resource();

                                // ── String date input: datenum(str [, fmt]) ──
                                // Parses a single date string with an explicit
                                // format string, or auto-detects the common ISO
                                // (yyyy-mm-dd[ HH:MM:SS]) and dd-mmm-yyyy forms.
                                if (args[0].isChar() || args[0].isString()) {
                                    const std::string s = args[0].toString();
                                    static const char *MON3[] = {
                                        "jan","feb","mar","apr","may","jun",
                                        "jul","aug","sep","oct","nov","dec"};
                                    auto tryFmt = [&](const std::string &fmt,
                                                      double &Y, double &Mo,
                                                      double &D, double &H,
                                                      double &MI, double &S) -> bool {
                                        Y = 0; Mo = 1; D = 1; H = 0; MI = 0; S = 0;
                                        size_t si = 0, fi = 0;
                                        auto readNum = [&](int maxD) -> long {
                                            long v = 0; int n = 0;
                                            while (si < s.size() && n < maxD
                                                   && std::isdigit(
                                                       static_cast<unsigned char>(s[si]))) {
                                                v = v * 10 + (s[si] - '0'); ++si; ++n;
                                            }
                                            return n > 0 ? v : -1;
                                        };
                                        while (fi < fmt.size()) {
                                            if (fmt.compare(fi, 4, "yyyy") == 0) {
                                                long v = readNum(4); if (v < 0) return false;
                                                Y = static_cast<double>(v); fi += 4;
                                            } else if (fmt.compare(fi, 4, "mmmm") == 0
                                                       || fmt.compare(fi, 3, "mmm") == 0) {
                                                bool full = fmt.compare(fi, 4, "mmmm") == 0;
                                                if (si + 3 > s.size()) return false;
                                                std::string mon = s.substr(si, 3);
                                                for (auto &c : mon)
                                                    c = static_cast<char>(std::tolower(
                                                        static_cast<unsigned char>(c)));
                                                int mi = -1;
                                                for (int k = 0; k < 12; ++k)
                                                    if (mon == MON3[k]) { mi = k + 1; break; }
                                                if (mi < 0) return false;
                                                Mo = static_cast<double>(mi);
                                                si += 3;
                                                if (full) {
                                                    while (si < s.size() && std::isalpha(
                                                               static_cast<unsigned char>(s[si]))) ++si;
                                                    fi += 4;
                                                } else { fi += 3; }
                                            } else if (fmt.compare(fi, 2, "mm") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                Mo = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "dd") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                D = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "HH") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                H = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "MM") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                MI = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "SS") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                S = static_cast<double>(v); fi += 2;
                                            } else {
                                                if (si < s.size() && s[si] == fmt[fi]) { ++si; ++fi; }
                                                else return false;
                                            }
                                        }
                                        return si == s.size();   // full consume
                                    };

                                    double Y, Mo, D, H, MI, S;
                                    bool ok = false;
                                    if (args.size() >= 2
                                        && (args[1].isChar() || args[1].isString())) {
                                        ok = tryFmt(args[1].toString(), Y, Mo, D, H, MI, S);
                                    } else {
                                        static const char *cands[] = {
                                            "yyyy-mm-dd HH:MM:SS", "yyyy-mm-dd",
                                            "dd-mmm-yyyy HH:MM:SS", "dd-mmm-yyyy"};
                                        for (const char *c : cands)
                                            if (tryFmt(c, Y, Mo, D, H, MI, S)) { ok = true; break; }
                                    }
                                    if (!ok)
                                        throw std::runtime_error(
                                            "datenum: could not parse date string "
                                            "(supported: explicit format string, or "
                                            "ISO yyyy-mm-dd and dd-mmm-yyyy forms)");
                                    outs[0] = Value::scalar(
                                        civilToSerial(Y, Mo, D, H, MI, S), mr);
                                    return;
                                }

                                // ── Single-arg form: V is 1x3, 1x6, Nx3, Nx6 ─
                                if (args.size() == 1) {
                                    const Value &V = args[0];
                                    const size_t R = V.dims().rows();
                                    const size_t C = V.dims().cols();
                                    if (C != 3 && C != 6)
                                        throw std::runtime_error(
                                            "datenum: single-arg matrix must "
                                            "have 3 or 6 columns");
                                    auto out = Value::matrix(
                                        R, 1, ValueType::DOUBLE, mr);
                                    double *o = out.doubleDataMut();
                                    for (size_t i = 0; i < R; ++i) {
                                        const double y = V.elemAsDouble(i);
                                        const double m = V.elemAsDouble(i + R);
                                        const double d = V.elemAsDouble(i + 2 * R);
                                        double h = 0.0, mi = 0.0, s = 0.0;
                                        if (C == 6) {
                                            h  = V.elemAsDouble(i + 3 * R);
                                            mi = V.elemAsDouble(i + 4 * R);
                                            s  = V.elemAsDouble(i + 5 * R);
                                        }
                                        o[i] = civilToSerial(y, m, d, h, mi, s);
                                    }
                                    if (R == 1)
                                        outs[0] = Value::scalar(o[0], mr);
                                    else
                                        outs[0] = std::move(out);
                                    return;
                                }

                                // ── Multi-arg form: 3 or 6 args ─────────────
                                if (args.size() != 3 && args.size() != 6)
                                    throw std::runtime_error(
                                        "datenum: expected 3 or 6 arguments "
                                        "(Y,M,D[,H,MI,S])");
                                // Determine output size = max numel across args
                                // (broadcast scalars). All non-scalar inputs
                                // must share the same numel.
                                size_t N = 1;
                                for (const auto &a : args) {
                                    if (a.numel() > 1) {
                                        if (N > 1 && a.numel() != N)
                                            throw std::runtime_error(
                                                "datenum: input vector lengths "
                                                "must match");
                                        N = a.numel();
                                    }
                                }
                                auto pick = [](const Value &a, size_t i) {
                                    return a.numel() == 1
                                               ? a.toScalar()
                                               : a.elemAsDouble(i);
                                };
                                if (N == 1) {
                                    const double h = args.size() == 6
                                                         ? args[3].toScalar()
                                                         : 0.0;
                                    const double mi = args.size() == 6
                                                          ? args[4].toScalar()
                                                          : 0.0;
                                    const double s = args.size() == 6
                                                         ? args[5].toScalar()
                                                         : 0.0;
                                    outs[0] = Value::scalar(
                                        civilToSerial(args[0].toScalar(),
                                                      args[1].toScalar(),
                                                      args[2].toScalar(),
                                                      h, mi, s),
                                        mr);
                                    return;
                                }
                                auto out = Value::matrix(
                                    N, 1, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i) {
                                    const double y = pick(args[0], i);
                                    const double m = pick(args[1], i);
                                    const double d = pick(args[2], i);
                                    double h = 0.0, mi = 0.0, s = 0.0;
                                    if (args.size() == 6) {
                                        h  = pick(args[3], i);
                                        mi = pick(args[4], i);
                                        s  = pick(args[5], i);
                                    }
                                    o[i] = civilToSerial(y, m, d, h, mi, s);
                                }
                                outs[0] = std::move(out);
                            });

    // ── weekday ───────────────────────────────────────────────
    // MATLAB weekday(D[, fmt]): day-of-week index 1..7 with Sunday=1,
    // Saturday=7 (US convention). Optional second output is the day
    // name as 'short' (Sun..Sat) or 'long' (Sunday..Saturday).
    //
    // Algorithm: serial 1 (= 0000-01-01) is a Saturday in MATLAB's
    // calendar. Solving: dow(d) = ((floor(d) - 2) mod 7) + 1 with
    // positive-modulo. Verified against MATLAB R2025b for current
    // and historical dates.
    engine.registerFunction("weekday",
                            [](Span<const Value> args,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "weekday requires at least one input");
                                bool wantLong = false;
                                if (args.size() >= 2
                                    && (args[1].isChar() || args[1].isString())) {
                                    std::string fmt = args[1].toString();
                                    for (auto &c : fmt)
                                        c = static_cast<char>(
                                            std::tolower(
                                                static_cast<unsigned char>(c)));
                                    if (fmt == "long")
                                        wantLong = true;
                                    else if (fmt != "short")
                                        throw std::runtime_error(
                                            "weekday: format must be 'short' "
                                            "or 'long'");
                                }
                                static const char *kShort[7] = {
                                    "Sun", "Mon", "Tue", "Wed",
                                    "Thu", "Fri", "Sat"
                                };
                                static const char *kLong[7] = {
                                    "Sunday", "Monday",   "Tuesday",
                                    "Wednesday", "Thursday", "Friday",
                                    "Saturday"
                                };
                                auto dayIndex = [](double d) -> int {
                                    // Positive-result modulo of (floor(d)-2) by 7.
                                    int64_t f = static_cast<int64_t>(
                                        std::floor(d)) - 2;
                                    int64_t r = f % 7;
                                    if (r < 0) r += 7;
                                    return static_cast<int>(r) + 1;
                                };

                                auto *mr = ctx.engine->resource();
                                const Value &D = args[0];
                                const size_t N = D.numel();

                                if (N == 1) {
                                    int idx = dayIndex(D.toScalar());
                                    outs[0] = Value::scalar(
                                        static_cast<double>(idx), mr);
                                    if (nargout > 1) {
                                        outs[1] = Value::fromString(
                                            wantLong ? kLong[idx - 1]
                                                     : kShort[idx - 1],
                                            mr);
                                    }
                                    return;
                                }

                                // Vector / matrix output: same shape as input.
                                auto out = Value::matrix(
                                    D.dims().rows(), D.dims().cols(),
                                    ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i)
                                    o[i] = static_cast<double>(
                                        dayIndex(D.elemAsDouble(i)));
                                outs[0] = std::move(out);
                                // Name output: only meaningful for scalar D
                                // in MATLAB's current API; for vector input,
                                // MATLAB returns the name of the FIRST element
                                // (legacy behaviour). Match it.
                                if (nargout > 1) {
                                    int idx = dayIndex(D.elemAsDouble(0));
                                    outs[1] = Value::fromString(
                                        wantLong ? kLong[idx - 1]
                                                 : kShort[idx - 1],
                                        mr);
                                }
                            });

    // ── juliandate ────────────────────────────────────────────
    // MATLAB juliandate: Julian day number from date components.
    //
    // Reference relationship: serial-MATLAB-date(1970,1,1) = 719529
    // and Julian-Date(1970-01-01 00:00 UTC) = 2440587.5, so:
    //
    //   JD = datenum-serial + 1721058.5
    //
    // Verified against well-known anchors:
    //   1970-01-01 00:00 = 2440587.5 (Unix epoch)
    //   2000-01-01 12:00 = 2451545.0 (J2000.0)
    //
    // Signatures (string + datetime forms deferred):
    //   juliandate(Y, M, D)              -- separate scalar/vector args
    //   juliandate(Y, M, D, H, MI, S)
    //   juliandate(V)                    -- V is Nx3 or Nx6
    engine.registerFunction("juliandate",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "juliandate requires at least one "
                                        "argument");
                                if (args[0].isChar() || args[0].isString())
                                    throw std::runtime_error(
                                        "juliandate: string parsing not "
                                        "yet supported");

                                auto civilToSerial = [](double yd, double md,
                                                        double dd, double hd,
                                                        double mind, double sd) {
                                    double dInt;
                                    const double dFrac = std::modf(dd, &dInt);
                                    int64_t y = static_cast<int64_t>(
                                        std::floor(yd));
                                    int64_t m = static_cast<int64_t>(
                                        std::floor(md));
                                    int64_t d = static_cast<int64_t>(dInt);
                                    if (m <= 2) y -= 1;
                                    const int64_t era =
                                        (y < 0 ? y - 399 : y) / 400;
                                    const int64_t yoe = y - era * 400;
                                    const int64_t doy =
                                        (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5
                                        + d - 1;
                                    const int64_t doe =
                                        yoe * 365 + yoe / 4 - yoe / 100 + doy;
                                    const int64_t days =
                                        era * 146097 + doe - 719468;
                                    const double frac =
                                        (hd * 3600.0 + mind * 60.0 + sd) / 86400.0
                                        + dFrac;
                                    return static_cast<double>(days) + 719529.0
                                         + frac;
                                };
                                const double kJDOffset = 1721058.5;

                                auto *mr = ctx.engine->resource();

                                // Single-arg matrix form
                                if (args.size() == 1) {
                                    const Value &V = args[0];
                                    const size_t R = V.dims().rows();
                                    const size_t C = V.dims().cols();
                                    if (C != 3 && C != 6)
                                        throw std::runtime_error(
                                            "juliandate: single-arg matrix "
                                            "must have 3 or 6 columns");
                                    auto out = Value::matrix(
                                        R, 1, ValueType::DOUBLE, mr);
                                    double *o = out.doubleDataMut();
                                    for (size_t i = 0; i < R; ++i) {
                                        const double y = V.elemAsDouble(i);
                                        const double m = V.elemAsDouble(i + R);
                                        const double d = V.elemAsDouble(i + 2 * R);
                                        double h = 0.0, mi = 0.0, s = 0.0;
                                        if (C == 6) {
                                            h  = V.elemAsDouble(i + 3 * R);
                                            mi = V.elemAsDouble(i + 4 * R);
                                            s  = V.elemAsDouble(i + 5 * R);
                                        }
                                        o[i] = civilToSerial(y, m, d, h, mi, s)
                                             + kJDOffset;
                                    }
                                    if (R == 1)
                                        outs[0] = Value::scalar(o[0], mr);
                                    else
                                        outs[0] = std::move(out);
                                    return;
                                }

                                if (args.size() != 3 && args.size() != 6)
                                    throw std::runtime_error(
                                        "juliandate: expected 3 or 6 "
                                        "arguments (Y,M,D[,H,MI,S])");
                                size_t N = 1;
                                for (const auto &a : args) {
                                    if (a.numel() > 1) {
                                        if (N > 1 && a.numel() != N)
                                            throw std::runtime_error(
                                                "juliandate: input vector "
                                                "lengths must match");
                                        N = a.numel();
                                    }
                                }
                                auto pick = [](const Value &a, size_t i) {
                                    return a.numel() == 1
                                               ? a.toScalar()
                                               : a.elemAsDouble(i);
                                };
                                if (N == 1) {
                                    const double h = args.size() == 6
                                                         ? args[3].toScalar()
                                                         : 0.0;
                                    const double mi = args.size() == 6
                                                          ? args[4].toScalar()
                                                          : 0.0;
                                    const double s = args.size() == 6
                                                         ? args[5].toScalar()
                                                         : 0.0;
                                    outs[0] = Value::scalar(
                                        civilToSerial(args[0].toScalar(),
                                                      args[1].toScalar(),
                                                      args[2].toScalar(),
                                                      h, mi, s)
                                            + kJDOffset,
                                        mr);
                                    return;
                                }
                                auto out = Value::matrix(
                                    N, 1, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i) {
                                    const double y = pick(args[0], i);
                                    const double m = pick(args[1], i);
                                    const double d = pick(args[2], i);
                                    double h = 0.0, mi = 0.0, s = 0.0;
                                    if (args.size() == 6) {
                                        h  = pick(args[3], i);
                                        mi = pick(args[4], i);
                                        s  = pick(args[5], i);
                                    }
                                    o[i] = civilToSerial(y, m, d, h, mi, s)
                                         + kJDOffset;
                                }
                                outs[0] = std::move(out);
                            });

    // ── eomday ────────────────────────────────────────────────
    // MATLAB eomday(y, m): last day of the given month (28..31).
    //
    // Leap year rule (proleptic Gregorian):
    //   isLeap(y) = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0
    //
    // Shape: output preserves the broadcast shape of (y, m). Both
    // scalar -> scalar; matched non-scalars must have identical
    // shape; one scalar broadcasts.
    engine.registerFunction("eomday",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "eomday requires (year, month)");
                                static const int kMonthDays[12] = {
                                    31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31
                                };
                                auto isLeap = [](int64_t y) {
                                    return (y % 4 == 0 && y % 100 != 0)
                                        || (y % 400 == 0);
                                };
                                auto monthEnd = [&](double yd, double md) {
                                    int64_t y = static_cast<int64_t>(
                                        std::floor(yd));
                                    int64_t m = static_cast<int64_t>(
                                        std::floor(md));
                                    if (m < 1 || m > 12)
                                        throw std::runtime_error(
                                            "eomday: month must be in 1..12");
                                    int days = kMonthDays[m - 1];
                                    if (m == 2 && isLeap(y)) days = 29;
                                    return static_cast<double>(days);
                                };

                                auto *mr = ctx.engine->resource();
                                const Value &Y = args[0];
                                const Value &M = args[1];

                                // Both scalar -> scalar output.
                                if (Y.numel() == 1 && M.numel() == 1) {
                                    outs[0] = Value::scalar(
                                        monthEnd(Y.toScalar(), M.toScalar()),
                                        mr);
                                    return;
                                }
                                // Determine output shape (broadcast).
                                size_t R, C;
                                if (Y.numel() == 1) {
                                    R = M.dims().rows();
                                    C = M.dims().cols();
                                } else if (M.numel() == 1) {
                                    R = Y.dims().rows();
                                    C = Y.dims().cols();
                                } else {
                                    if (Y.dims().rows() != M.dims().rows()
                                        || Y.dims().cols() != M.dims().cols())
                                        throw std::runtime_error(
                                            "eomday: y and m must have the "
                                            "same shape (or one scalar)");
                                    R = Y.dims().rows();
                                    C = Y.dims().cols();
                                }
                                auto out = Value::matrix(
                                    R, C, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                const size_t N = R * C;
                                for (size_t i = 0; i < N; ++i) {
                                    const double yi = Y.numel() == 1
                                                          ? Y.toScalar()
                                                          : Y.elemAsDouble(i);
                                    const double mi = M.numel() == 1
                                                          ? M.toScalar()
                                                          : M.elemAsDouble(i);
                                    o[i] = monthEnd(yi, mi);
                                }
                                outs[0] = std::move(out);
                            });

    // ── datevec ───────────────────────────────────────────────
    // MATLAB datevec(d): inverse of datenum.
    //
    // Single output: N-by-6 matrix, one row per scalar input element
    // (column-major linearisation for matrix input). Six outputs:
    // separate length-N column vectors (Y, M, D, H, MI, S).
    //
    // Algorithm: Howard Hinnant's `civil_from_days` to recover (Y, M, D)
    // from the integer day index, then extract H, MI, S from the
    // fractional part. Microsecond rounding tames double-precision
    // noise so datenum->datevec round-trips give exact integers.
    //
    // Edge: datevec(0) = [0 0 0 0 0 0] (matches MATLAB literal).
    // datestr(D [, fmt]) — format a serial date number (or a 1x6 date vector)
    // as text. Supports a format STRING with the common field tokens
    // (yyyy yy mmmm mmm mm dddd ddd dd HH MM SS) and an auto-selected default
    // format. (numeric format codes, AM/PM 12-hour, and multi-date matrix
    // inputs are not yet handled.)
    engine.registerFunction("datestr",
                            [](Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "datestr requires at least one argument");
                                auto *mr = ctx.engine->resource();
                                const Value &din = args[0];
                                if (din.isChar() || din.isString())
                                    throw std::runtime_error(
                                        "datestr: string date input not yet "
                                        "supported");

                                auto civilFromDays = [](int64_t z, int64_t &Y,
                                                        int &M, int &D) {
                                    z += 719468;
                                    const int64_t era =
                                        (z >= 0 ? z : z - 146096) / 146097;
                                    const int64_t doe = z - era * 146097;
                                    const int64_t yoe =
                                        (doe - doe / 1460 + doe / 36524
                                         - doe / 146096) / 365;
                                    const int64_t y = yoe + era * 400;
                                    const int64_t doy =
                                        doe - (365 * yoe + yoe / 4 - yoe / 100);
                                    const int64_t mp = (5 * doy + 2) / 153;
                                    D = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
                                    M = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
                                    Y = y + (M <= 2 ? 1 : 0);
                                };

                                int yi, moi, di, hi, mii, si;
                                if (din.numel() == 6) {
                                    yi  = static_cast<int>(din.elemAsDouble(0));
                                    moi = static_cast<int>(din.elemAsDouble(1));
                                    di  = static_cast<int>(din.elemAsDouble(2));
                                    hi  = static_cast<int>(din.elemAsDouble(3));
                                    mii = static_cast<int>(din.elemAsDouble(4));
                                    si  = static_cast<int>(std::round(
                                              din.elemAsDouble(5)));
                                } else if (din.numel() == 1) {
                                    const double dval = din.elemAsDouble(0);
                                    const double floored = std::floor(dval);
                                    const int64_t z =
                                        static_cast<int64_t>(floored) - 719529;
                                    const double frac = dval - floored;
                                    int64_t Y; int M, D;
                                    civilFromDays(z, Y, M, D);
                                    int64_t ms = static_cast<int64_t>(
                                        std::round(frac * 86400.0 * 1.0e3));
                                    int H  = static_cast<int>(ms / 3600000LL); ms %= 3600000LL;
                                    int MI = static_cast<int>(ms / 60000LL);   ms %= 60000LL;
                                    double S = static_cast<double>(ms) / 1.0e3;
                                    if (S >= 60.0) { S -= 60.0; ++MI; }
                                    if (MI >= 60)  { MI -= 60;  ++H;  }
                                    if (H  >= 24)  { H  -= 24; civilFromDays(z + 1, Y, M, D); }
                                    yi = static_cast<int>(Y); moi = M; di = D;
                                    hi = H; mii = MI; si = static_cast<int>(std::round(S));
                                } else {
                                    throw std::runtime_error(
                                        "datestr: multi-date matrix input not "
                                        "yet supported");
                                }

                                std::string fmt;
                                if (args.size() >= 2) {
                                    const Value &f = args[1];
                                    if (f.isChar() || f.isString())
                                        fmt = f.toString();
                                    else
                                        throw std::runtime_error(
                                            "datestr: numeric format codes not "
                                            "yet supported; pass a format string");
                                } else {
                                    fmt = (hi != 0 || mii != 0 || si != 0)
                                              ? "dd-mmm-yyyy HH:MM:SS"
                                              : "dd-mmm-yyyy";
                                }

                                static const char *MON3[] = {
                                    "Jan","Feb","Mar","Apr","May","Jun",
                                    "Jul","Aug","Sep","Oct","Nov","Dec"};
                                static const char *MONF[] = {
                                    "January","February","March","April","May",
                                    "June","July","August","September","October",
                                    "November","December"};
                                static const char *DOW3[] = {
                                    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
                                static const char *DOWF[] = {
                                    "Sunday","Monday","Tuesday","Wednesday",
                                    "Thursday","Friday","Saturday"};
                                // Day of week via Sakamoto's algorithm.
                                static const int dt[] = {0,3,2,5,0,3,5,1,4,6,2,4};
                                int yw = yi - (moi < 3 ? 1 : 0);
                                int dow = ((yw + yw/4 - yw/100 + yw/400
                                            + dt[(moi - 1 + 12) % 12] + di) % 7 + 7) % 7;

                                std::string out;
                                char buf[16];
                                size_t i = 0;
                                auto at = [&](const char *t, size_t L) {
                                    return fmt.compare(i, L, t) == 0;
                                };
                                while (i < fmt.size()) {
                                    if (at("yyyy", 4)) { std::snprintf(buf,sizeof buf,"%04d",yi); out+=buf; i+=4; }
                                    else if (at("yy", 2)) { std::snprintf(buf,sizeof buf,"%02d",((yi%100)+100)%100); out+=buf; i+=2; }
                                    else if (at("mmmm", 4)) { out += MONF[(moi-1+12)%12]; i+=4; }
                                    else if (at("mmm", 3)) { out += MON3[(moi-1+12)%12]; i+=3; }
                                    else if (at("mm", 2)) { std::snprintf(buf,sizeof buf,"%02d",moi); out+=buf; i+=2; }
                                    else if (at("dddd", 4)) { out += DOWF[dow]; i+=4; }
                                    else if (at("ddd", 3)) { out += DOW3[dow]; i+=3; }
                                    else if (at("dd", 2)) { std::snprintf(buf,sizeof buf,"%02d",di); out+=buf; i+=2; }
                                    else if (at("HH", 2)) { std::snprintf(buf,sizeof buf,"%02d",hi); out+=buf; i+=2; }
                                    else if (at("MM", 2)) { std::snprintf(buf,sizeof buf,"%02d",mii); out+=buf; i+=2; }
                                    else if (at("SS", 2)) { std::snprintf(buf,sizeof buf,"%02d",si); out+=buf; i+=2; }
                                    else { out += fmt[i]; ++i; }
                                }
                                outs[0] = Value::fromString(out, mr);
                            });

    engine.registerFunction("datevec",
                            [](Span<const Value> args,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "datevec requires at least one "
                                        "argument");
                                // String date input: datevec(str [, fmt]) —
                                // parse with an explicit format or auto-detect
                                // the common ISO / dd-mmm-yyyy forms (same
                                // parser as datenum), returning [Y M D H MI S].
                                if (args[0].isChar() || args[0].isString()) {
                                    auto *mrs = ctx.engine->resource();
                                    const std::string s = args[0].toString();
                                    static const char *MON3s[] = {
                                        "jan","feb","mar","apr","may","jun",
                                        "jul","aug","sep","oct","nov","dec"};
                                    auto tryFmt = [&](const std::string &fmt,
                                                      double &Y, double &Mo,
                                                      double &D, double &H,
                                                      double &MI, double &S) -> bool {
                                        Y = 0; Mo = 1; D = 1; H = 0; MI = 0; S = 0;
                                        size_t si = 0, fi = 0;
                                        auto readNum = [&](int maxD) -> long {
                                            long v = 0; int n = 0;
                                            while (si < s.size() && n < maxD
                                                   && std::isdigit(
                                                       static_cast<unsigned char>(s[si]))) {
                                                v = v * 10 + (s[si] - '0'); ++si; ++n;
                                            }
                                            return n > 0 ? v : -1;
                                        };
                                        while (fi < fmt.size()) {
                                            if (fmt.compare(fi, 4, "yyyy") == 0) {
                                                long v = readNum(4); if (v < 0) return false;
                                                Y = static_cast<double>(v); fi += 4;
                                            } else if (fmt.compare(fi, 4, "mmmm") == 0
                                                       || fmt.compare(fi, 3, "mmm") == 0) {
                                                bool full = fmt.compare(fi, 4, "mmmm") == 0;
                                                if (si + 3 > s.size()) return false;
                                                std::string mon = s.substr(si, 3);
                                                for (auto &c : mon)
                                                    c = static_cast<char>(std::tolower(
                                                        static_cast<unsigned char>(c)));
                                                int mi = -1;
                                                for (int k = 0; k < 12; ++k)
                                                    if (mon == MON3s[k]) { mi = k + 1; break; }
                                                if (mi < 0) return false;
                                                Mo = static_cast<double>(mi); si += 3;
                                                if (full) {
                                                    while (si < s.size() && std::isalpha(
                                                               static_cast<unsigned char>(s[si]))) ++si;
                                                    fi += 4;
                                                } else { fi += 3; }
                                            } else if (fmt.compare(fi, 2, "mm") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                Mo = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "dd") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                D = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "HH") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                H = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "MM") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                MI = static_cast<double>(v); fi += 2;
                                            } else if (fmt.compare(fi, 2, "SS") == 0) {
                                                long v = readNum(2); if (v < 0) return false;
                                                S = static_cast<double>(v); fi += 2;
                                            } else {
                                                if (si < s.size() && s[si] == fmt[fi]) { ++si; ++fi; }
                                                else return false;
                                            }
                                        }
                                        return si == s.size();
                                    };
                                    double Y, Mo, D, H, MI, S;
                                    bool ok = false;
                                    if (args.size() >= 2
                                        && (args[1].isChar() || args[1].isString())) {
                                        ok = tryFmt(args[1].toString(), Y, Mo, D, H, MI, S);
                                    } else {
                                        static const char *cands[] = {
                                            "yyyy-mm-dd HH:MM:SS", "yyyy-mm-dd",
                                            "dd-mmm-yyyy HH:MM:SS", "dd-mmm-yyyy"};
                                        for (const char *c : cands)
                                            if (tryFmt(c, Y, Mo, D, H, MI, S)) { ok = true; break; }
                                    }
                                    if (!ok)
                                        throw std::runtime_error(
                                            "datevec: could not parse date string "
                                            "(supported: explicit format string, or "
                                            "ISO yyyy-mm-dd and dd-mmm-yyyy forms)");
                                    const double vals[6] = {Y, Mo, D, H, MI, S};
                                    if (nargout <= 1) {
                                        auto out = Value::matrix(1, 6, ValueType::DOUBLE, mrs);
                                        double *o = out.doubleDataMut();
                                        for (int k = 0; k < 6; ++k) o[k] = vals[k];
                                        outs[0] = std::move(out);
                                    } else {
                                        for (int k = 0; k < 6
                                                        && k < static_cast<int>(nargout); ++k)
                                            outs[k] = Value::scalar(vals[k], mrs);
                                    }
                                    return;
                                }

                                auto civilFromDays = [](int64_t z,
                                                        int64_t &Y, int &M,
                                                        int &D) {
                                    z += 719468;
                                    const int64_t era =
                                        (z >= 0 ? z : z - 146096) / 146097;
                                    const int64_t doe = z - era * 146097;
                                    const int64_t yoe =
                                        (doe - doe / 1460 + doe / 36524
                                         - doe / 146096)
                                        / 365;
                                    const int64_t y = yoe + era * 400;
                                    const int64_t doy =
                                        doe - (365 * yoe + yoe / 4 - yoe / 100);
                                    const int64_t mp = (5 * doy + 2) / 153;
                                    D = static_cast<int>(
                                        doy - (153 * mp + 2) / 5 + 1);
                                    M = static_cast<int>(
                                        mp < 10 ? mp + 3 : mp - 9);
                                    Y = y + (M <= 2 ? 1 : 0);
                                };
                                auto extractTime = [](double frac, int &H,
                                                      int &MI, double &S) {
                                    // Round to milliseconds. Microsecond
                                    // rounding is at the FP-precision edge
                                    // for typical serial-date magnitudes
                                    // (~7e5 days -> ~7us absolute precision)
                                    // and shows up as +/-1us noise on round-
                                    // trips. Millisecond gives a comfortable
                                    // margin while still preserving MATLAB-
                                    // displayed fractional-second resolution.
                                    const double total_ms =
                                        std::round(frac * 86400.0 * 1.0e3);
                                    int64_t ms = static_cast<int64_t>(total_ms);
                                    H  = static_cast<int>(ms / 3600000LL);
                                    ms %= 3600000LL;
                                    MI = static_cast<int>(ms / 60000LL);
                                    ms %= 60000LL;
                                    S  = static_cast<double>(ms) / 1.0e3;
                                };
                                auto vecOf = [&](double dval, double *out6) {
                                    if (dval == 0.0) {
                                        for (int k = 0; k < 6; ++k)
                                            out6[k] = 0.0;
                                        return;
                                    }
                                    const double floored = std::floor(dval);
                                    const int64_t days =
                                        static_cast<int64_t>(floored);
                                    const double frac = dval - floored;
                                    const int64_t z = days - 719529;
                                    int64_t Y;
                                    int M, D, H, MI;
                                    double S;
                                    civilFromDays(z, Y, M, D);
                                    extractTime(frac, H, MI, S);
                                    // Carry from S/MI/H into D/M/Y if rounding
                                    // pushed seconds to 60.
                                    if (S >= 60.0) { S -= 60.0; ++MI; }
                                    if (MI >= 60)  { MI -= 60;  ++H;  }
                                    if (H  >= 24)  { H  -= 24;
                                        // Day rolled over -- recompute civil.
                                        civilFromDays(z + 1, Y, M, D);
                                    }
                                    out6[0] = static_cast<double>(Y);
                                    out6[1] = static_cast<double>(M);
                                    out6[2] = static_cast<double>(D);
                                    out6[3] = static_cast<double>(H);
                                    out6[4] = static_cast<double>(MI);
                                    out6[5] = S;
                                };

                                auto *mr = ctx.engine->resource();
                                const Value &Din = args[0];
                                const size_t N = Din.numel();

                                // Compute N x 6 output column-major.
                                auto out = Value::matrix(
                                    N, 6, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                double tmp[6];
                                for (size_t i = 0; i < N; ++i) {
                                    vecOf(Din.elemAsDouble(i), tmp);
                                    for (int c = 0; c < 6; ++c)
                                        o[i + c * N] = tmp[c];
                                }

                                if (nargout <= 1) {
                                    outs[0] = std::move(out);
                                    return;
                                }
                                // 6-output form: separate column vectors
                                // (or scalars if N == 1).
                                for (int c = 0; c < 6
                                                && c < static_cast<int>(nargout);
                                     ++c) {
                                    if (N == 1) {
                                        outs[c] = Value::scalar(
                                            o[c * N], mr);
                                    } else {
                                        auto col = Value::matrix(
                                            N, 1, ValueType::DOUBLE, mr);
                                        double *p = col.doubleDataMut();
                                        for (size_t i = 0; i < N; ++i)
                                            p[i] = o[i + c * N];
                                        outs[c] = std::move(col);
                                    }
                                }
                            });

    // ── yyyymmdd ──────────────────────────────────────────────
    // Packed integer date: Y*10000 + M*100 + D from a MATLAB serial
    // date number. Output preserves input shape.
    //
    // EXTENSION vs MATLAB: MATLAB R2025b's yyyymmdd accepts only
    // datetime input (numkit has no datetime class yet). Accepting
    // a serial date number here matches the spirit of the function
    // and is the call most users want; the equivalent MATLAB call
    // is `yyyymmdd(datetime(d, 'ConvertFrom', 'datenum'))`. Parity
    // spec wraps the input with that conversion.
    engine.registerFunction("yyyymmdd",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "yyyymmdd requires one argument");
                                if (args[0].isChar() || args[0].isString())
                                    throw std::runtime_error(
                                        "yyyymmdd: string parsing not "
                                        "supported");
                                auto civilFromDays = [](int64_t z,
                                                        int64_t &Y, int &M,
                                                        int &D) {
                                    z += 719468;
                                    const int64_t era =
                                        (z >= 0 ? z : z - 146096) / 146097;
                                    const int64_t doe = z - era * 146097;
                                    const int64_t yoe =
                                        (doe - doe / 1460 + doe / 36524
                                         - doe / 146096)
                                        / 365;
                                    const int64_t y = yoe + era * 400;
                                    const int64_t doy =
                                        doe - (365 * yoe + yoe / 4 - yoe / 100);
                                    const int64_t mp = (5 * doy + 2) / 153;
                                    D = static_cast<int>(
                                        doy - (153 * mp + 2) / 5 + 1);
                                    M = static_cast<int>(
                                        mp < 10 ? mp + 3 : mp - 9);
                                    Y = y + (M <= 2 ? 1 : 0);
                                };
                                auto packOne = [&](double dval) {
                                    if (dval == 0.0) return 0.0;
                                    const int64_t days =
                                        static_cast<int64_t>(std::floor(dval));
                                    const int64_t z = days - 719529;
                                    int64_t Y;
                                    int M, D;
                                    civilFromDays(z, Y, M, D);
                                    return static_cast<double>(
                                        Y * 10000 + M * 100 + D);
                                };

                                auto *mr = ctx.engine->resource();
                                const Value &Din = args[0];
                                const size_t N = Din.numel();
                                if (N == 1) {
                                    outs[0] = Value::scalar(
                                        packOne(Din.toScalar()), mr);
                                    return;
                                }
                                auto out = Value::matrix(
                                    Din.dims().rows(), Din.dims().cols(),
                                    ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i)
                                    o[i] = packOne(Din.elemAsDouble(i));
                                outs[0] = std::move(out);
                            });

    // ── mjuliandate ───────────────────────────────────────────
    // Modified Julian Date = JD - 2400000.5. MJD epoch is
    // 1858-11-17 00:00 (so mjuliandate(1858,11,17,0,0,0) = 0).
    //
    // Relationship to MATLAB serial date:
    //   MJD = serial + 1721058.5 - 2400000.5 = serial - 678942
    // both fractional offsets cancel exactly so noon at Y-M-D 00:00
    // gives a half-integer MJD only via the H,MI,S contribution.
    //
    // Signatures (string + datetime forms deferred):
    //   mjuliandate(Y, M, D)
    //   mjuliandate(Y, M, D, H, MI, S)
    //   mjuliandate(V)               with V row 1x3, 1x6, or matrix Nx3 / Nx6
    engine.registerFunction("mjuliandate",
                            [](Span<const Value> args,
                               size_t /*nargout*/,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error(
                                        "mjuliandate requires at least one "
                                        "argument");
                                if (args[0].isChar() || args[0].isString())
                                    throw std::runtime_error(
                                        "mjuliandate: string parsing not "
                                        "yet supported");

                                auto civilToSerial = [](double yd, double md,
                                                        double dd, double hd,
                                                        double mind, double sd) {
                                    double dInt;
                                    const double dFrac = std::modf(dd, &dInt);
                                    int64_t y = static_cast<int64_t>(
                                        std::floor(yd));
                                    int64_t m = static_cast<int64_t>(
                                        std::floor(md));
                                    int64_t d = static_cast<int64_t>(dInt);
                                    if (m <= 2) y -= 1;
                                    const int64_t era =
                                        (y < 0 ? y - 399 : y) / 400;
                                    const int64_t yoe = y - era * 400;
                                    const int64_t doy =
                                        (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5
                                        + d - 1;
                                    const int64_t doe =
                                        yoe * 365 + yoe / 4 - yoe / 100 + doy;
                                    const int64_t days =
                                        era * 146097 + doe - 719468;
                                    const double frac =
                                        (hd * 3600.0 + mind * 60.0 + sd) / 86400.0
                                        + dFrac;
                                    return static_cast<double>(days) + 719529.0
                                         + frac;
                                };
                                const double kMJDFromSerial = -678942.0;

                                auto *mr = ctx.engine->resource();

                                // Single-arg matrix form
                                if (args.size() == 1) {
                                    const Value &V = args[0];
                                    const size_t R = V.dims().rows();
                                    const size_t C = V.dims().cols();
                                    if (C != 3 && C != 6)
                                        throw std::runtime_error(
                                            "mjuliandate: single-arg matrix "
                                            "must have 3 or 6 columns");
                                    auto out = Value::matrix(
                                        R, 1, ValueType::DOUBLE, mr);
                                    double *o = out.doubleDataMut();
                                    for (size_t i = 0; i < R; ++i) {
                                        const double y = V.elemAsDouble(i);
                                        const double m = V.elemAsDouble(i + R);
                                        const double d = V.elemAsDouble(i + 2 * R);
                                        double h = 0.0, mi = 0.0, s = 0.0;
                                        if (C == 6) {
                                            h  = V.elemAsDouble(i + 3 * R);
                                            mi = V.elemAsDouble(i + 4 * R);
                                            s  = V.elemAsDouble(i + 5 * R);
                                        }
                                        o[i] = civilToSerial(y, m, d, h, mi, s)
                                             + kMJDFromSerial;
                                    }
                                    if (R == 1)
                                        outs[0] = Value::scalar(o[0], mr);
                                    else
                                        outs[0] = std::move(out);
                                    return;
                                }

                                if (args.size() != 3 && args.size() != 6)
                                    throw std::runtime_error(
                                        "mjuliandate: expected 3 or 6 "
                                        "arguments (Y,M,D[,H,MI,S])");
                                size_t N = 1;
                                for (const auto &a : args) {
                                    if (a.numel() > 1) {
                                        if (N > 1 && a.numel() != N)
                                            throw std::runtime_error(
                                                "mjuliandate: input vector "
                                                "lengths must match");
                                        N = a.numel();
                                    }
                                }
                                auto pick = [](const Value &a, size_t i) {
                                    return a.numel() == 1
                                               ? a.toScalar()
                                               : a.elemAsDouble(i);
                                };
                                if (N == 1) {
                                    const double h = args.size() == 6
                                                         ? args[3].toScalar()
                                                         : 0.0;
                                    const double mi = args.size() == 6
                                                          ? args[4].toScalar()
                                                          : 0.0;
                                    const double s = args.size() == 6
                                                         ? args[5].toScalar()
                                                         : 0.0;
                                    outs[0] = Value::scalar(
                                        civilToSerial(args[0].toScalar(),
                                                      args[1].toScalar(),
                                                      args[2].toScalar(),
                                                      h, mi, s)
                                            + kMJDFromSerial,
                                        mr);
                                    return;
                                }
                                auto out = Value::matrix(
                                    N, 1, ValueType::DOUBLE, mr);
                                double *o = out.doubleDataMut();
                                for (size_t i = 0; i < N; ++i) {
                                    const double y = pick(args[0], i);
                                    const double m = pick(args[1], i);
                                    const double d = pick(args[2], i);
                                    double h = 0.0, mi = 0.0, s = 0.0;
                                    if (args.size() == 6) {
                                        h  = pick(args[3], i);
                                        mi = pick(args[4], i);
                                        s  = pick(args[5], i);
                                    }
                                    o[i] = civilToSerial(y, m, d, h, mi, s)
                                         + kMJDFromSerial;
                                }
                                outs[0] = std::move(out);
                            });

    // ── addpath / rmpath / path / rehash / run (Phase 9b) ──────
    engine.registerFunction("addpath",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                for (const auto &a : args) {
                                    if (a.isChar()) ctx.engine->addPath(a.toString());
                                }
                                outs[0] = Value::empty();
                            });

    engine.registerFunction("rmpath",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                for (const auto &a : args) {
                                    if (a.isChar()) ctx.engine->rmPath(a.toString());
                                }
                                outs[0] = Value::empty();
                            });

    engine.registerFunction("path",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                // path() with no args: print current path; with nargout, return as char string.
                                // path('a:b:c') in MATLAB also sets the path — we treat single-arg as
                                // a list of paths (string with pathsep) and replace.
                                if (args.empty()) {
                                    const auto &paths = ctx.engine->path();
                                    if (nargout == 0) {
                                        std::ostringstream os;
                                        for (const auto &p : paths) os << p << "\n";
                                        ctx.engine->outputText(os.str());
                                        outs[0] = Value::empty();
                                    } else {
                                        // Return as a single newline-joined char vector
                                        std::ostringstream os;
                                        for (size_t i = 0; i < paths.size(); ++i) {
                                            if (i) os << "\n";
                                            os << paths[i];
                                        }
                                        outs[0] = Value::fromString(os.str(), ctx.engine->resource());
                                    }
                                    return;
                                }
                                // path(p1, p2, ...) — replace path with the given list.
                                // Drop existing entries first.
                                auto current = ctx.engine->path();
                                for (const auto &p : current) ctx.engine->rmPath(p);
                                for (const auto &a : args) {
                                    if (a.isChar()) ctx.engine->addPath(a.toString());
                                }
                                outs[0] = Value::empty();
                            });

    engine.registerFunction("rehash",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
                                ctx.engine->rehashMFiles();
                                outs[0] = Value::empty();
                            });

    // ── run ──────────────────────────────────────────────────
    // run('script.m') executes the script in the caller's workspace
    // (matches MATLAB semantics: scripts share scope with the caller).
    // ctx.env is the caller's frame.env in VM mode, workspaceEnv at
    // top-level. eval(content, scope) routes the inner top-level's
    // imports + variable assignments to that scope.
    //
    // Legacy compat: env var NUMKIT_LEGACY_EVAL_SCOPE=1 reverts to the
    // pre-2026-05 behaviour where eval/run from inside a function
    // leaked variables and imports into workspaceEnv. Provided as an
    // escape hatch for code that depended on the old (buggy) scope.
    auto resolveEvalScope = [](CallContext &ctx) -> Environment * {
        const char *legacy = std::getenv("NUMKIT_LEGACY_EVAL_SCOPE");
        if (legacy && legacy[0] == '1' && legacy[1] == '\0')
            return &ctx.engine->workspaceEnv();
        return ctx.env;
    };

    engine.registerFunction(
        "run", [resolveEvalScope](Span<const Value> args, size_t,
                                   Span<Value> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isChar())
                throw std::runtime_error("run requires a string filename");
            std::string p = args[0].toString();
            auto rp = ctx.engine->resolvePath(p);
            if (!rp.fs || !rp.fs->exists(rp.path))
                throw std::runtime_error("run: file not found: " + p);
            std::string content = rp.fs->readFile(rp.path);

            // Push script origin for the duration of the run so that
            // sibling .m files in the same directory resolve without
            // addpath. The dir is extracted from the resolved path
            // (rp.path is the script's full path inside rp.fs).
            // Root-level files ("/foo.m") need scriptDir = "/" — an
            // empty string would be treated as "no scriptDir" by the
            // resolver, so substr(0,0) won't do.
            std::string scriptDir;
            {
                size_t slash = rp.path.find_last_of("/\\");
                if (slash == 0)
                    scriptDir.assign(1, rp.path[0]);   // "/" or "\\"
                else if (slash != std::string::npos)
                    scriptDir = rp.path.substr(0, slash);
            }
            ctx.engine->pushScriptOrigin(rp.fs->name(), scriptDir);
            try {
                ctx.engine->eval(content, resolveEvalScope(ctx));
            } catch (...) {
                ctx.engine->popScriptOrigin();
                throw;
            }
            ctx.engine->popScriptOrigin();
            outs[0] = Value::empty();
        });

    // ── eval ─────────────────────────────────────────────────
    // eval(str) executes `str` in the caller's workspace. Matches
    // MATLAB: variables defined in the eval'd code are visible to the
    // caller (when caller is at top-level), and imports are scoped to
    // the caller's lifetime.
    //
    // When the caller captures the result (`r = eval(...)`, nargout>=1),
    // MATLAB suppresses any "ans = ..." display the inner code would
    // otherwise emit. The third arg routes that suppress through the
    // engine, which flips suppressOutput on top-level statements before
    // executing.
    engine.registerFunction(
        "eval", [resolveEvalScope](Span<const Value> args, size_t nargout,
                                    Span<Value> outs, CallContext &ctx) {
            if (args.empty() || !args[0].isChar())
                throw std::runtime_error("eval requires a string");
            const bool suppress = (nargout >= 1);
            outs[0] = ctx.engine->eval(args[0].toString(),
                                       resolveEvalScope(ctx),
                                       suppress);
        });

    // ── evalin ───────────────────────────────────────────────
    // evalin(workspace, str) executes `str` in either the base
    // workspace ('base') or the workspace of the caller of the function
    // containing this evalin call ('caller'). The latter matches
    // MATLAB's two-frames-up rule.
    engine.registerFunction(
        "evalin", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("evalin: requires (workspace, code)");
            if (!args[0].isChar() || !args[1].isChar())
                throw std::runtime_error("evalin: arguments must be strings");
            std::string where = args[0].toString();
            std::string code = args[1].toString();
            Environment *target = nullptr;
            if (where == "base") {
                target = &ctx.engine->workspaceEnv();
            } else if (where == "caller") {
                if (ctx.engine->callerDepth() < 1)
                    throw std::runtime_error(
                        "evalin: 'caller' is not valid in the base workspace");
                target = ctx.engine->callerEnv(1);
                if (!target) target = &ctx.engine->workspaceEnv();
            } else {
                throw std::runtime_error(
                    "evalin: workspace must be 'base' or 'caller', got '" + where + "'");
            }
            outs[0] = ctx.engine->eval(code, target);
        });

    // ── pwd / cd (Phase 9c) ────────────────────────────────────
    engine.registerFunction("pwd",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
                                std::string c = ctx.engine->cwd();
                                if (c.empty()) {
                                    // No engine-level cwd set — ask the active backend.
                                    try {
                                        auto rp = ctx.engine->resolvePath(".");
                                        if (rp.fs) c = rp.fs->cwd();
                                    } catch (...) {}
                                    if (c.empty()) {
                                        if (auto *fs = ctx.engine->findVirtualFS("native"))
                                            c = fs->cwd();
                                    }
                                }
                                outs[0] = Value::fromString(c, ctx.engine->resource());
                            });

    engine.registerFunction("cd",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                // No args: behave like pwd (return / print current dir).
                                if (args.empty()) {
                                    std::string c = ctx.engine->cwd();
                                    if (nargout == 0) {
                                        ctx.engine->outputText(c + "\n");
                                        outs[0] = Value::empty();
                                    } else {
                                        outs[0] = Value::fromString(c, ctx.engine->resource());
                                    }
                                    return;
                                }
                                if (!args[0].isChar())
                                    throw std::runtime_error("cd: directory must be a string");
                                std::string target = args[0].toString();
                                auto rp = ctx.engine->resolvePath(target);
                                if (!rp.fs)
                                    throw std::runtime_error("cd: cannot resolve '" + target + "'");
                                auto st = rp.fs->stat(rp.path);
                                if (!st || st->kind != FileStat::Kind::Directory)
                                    throw std::runtime_error("cd: not a directory: " + target);
                                std::string prev = ctx.engine->cwd();
                                ctx.engine->setCwd(rp.path);
                                // MATLAB: with output, return the PREVIOUS cwd.
                                if (nargout > 0)
                                    outs[0] = Value::fromString(prev, ctx.engine->resource());
                                else
                                    outs[0] = Value::empty();
                            });

    // ── version ───────────────────────────────────────────────
    // numkit-m doesn't carry a SemVer; the build's link-time stamp
    // serves as our "version". The timestamp lives in a tiny
    // separate TU (version_string.cpp) that gets recompiled on
    // every build via the `numkit_build_info` CMake target — so the
    // value here ALWAYS reflects the actual link time, not stale
    // __DATE__/__TIME__ macros from whenever library.cpp's .o
    // happened to last refresh. Declaration lives in the generated
    // build_info.hpp (alongside the NUMKIT_BUILD_TIMESTAMP macro).
    engine.registerFunction(
        "version",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            outs[0] = Value::fromString(std::string(numkit::buildTimestamp()),
                                        ctx.engine->resource());
        });

    // ── mkdir / rmdir / delete ────────────────────────────────
    engine.registerFunction("mkdir",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                if (args.empty() || !args[0].isChar())
                                    throw std::runtime_error("mkdir requires a directory name");
                                std::string p = args[0].toString();
                                if (args.size() >= 2 && args[1].isChar()) {
                                    // mkdir(parent, name) — concatenate.
                                    std::string parent = p;
                                    std::string name = args[1].toString();
                                    if (!parent.empty() && parent.back() != '/' && parent.back() != '\\')
                                        parent += '/';
                                    p = parent + name;
                                }
                                auto rp = ctx.engine->resolvePath(p);
                                if (!rp.fs)
                                    throw std::runtime_error("mkdir: cannot resolve '" + p + "'");
                                bool ok = true;
                                std::string msg;
                                try {
                                    rp.fs->mkdir(rp.path);
                                } catch (const std::exception &e) {
                                    ok = false;
                                    msg = e.what();
                                }
                                // MATLAB returns [status, msg]; default suppresses errors.
                                if (nargout > 0)
                                    outs[0] = Value::logicalScalar(ok, ctx.engine->resource());
                                else if (!ok)
                                    throw std::runtime_error("mkdir: " + msg);
                                else
                                    outs[0] = Value::empty();
                                if (nargout > 1)
                                    outs[1] = Value::fromString(msg, ctx.engine->resource());
                            });

    engine.registerFunction("rmdir",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                if (args.empty() || !args[0].isChar())
                                    throw std::runtime_error("rmdir requires a directory name");
                                std::string p = args[0].toString();
                                auto rp = ctx.engine->resolvePath(p);
                                if (!rp.fs)
                                    throw std::runtime_error("rmdir: cannot resolve '" + p + "'");
                                bool ok = true;
                                std::string msg;
                                try {
                                    rp.fs->rmdir(rp.path);
                                } catch (const std::exception &e) {
                                    ok = false;
                                    msg = e.what();
                                }
                                if (nargout > 0)
                                    outs[0] = Value::logicalScalar(ok, ctx.engine->resource());
                                else if (!ok)
                                    throw std::runtime_error("rmdir: " + msg);
                                else
                                    outs[0] = Value::empty();
                                if (nargout > 1)
                                    outs[1] = Value::fromString(msg, ctx.engine->resource());
                            });

    engine.registerFunction("delete",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                for (const auto &a : args) {
                                    if (!a.isChar())
                                        throw std::runtime_error("delete: filename must be a string");
                                    std::string p = a.toString();
                                    auto rp = ctx.engine->resolvePath(p);
                                    if (!rp.fs)
                                        throw std::runtime_error("delete: cannot resolve '" + p + "'");
                                    rp.fs->unlink(rp.path);
                                }
                                outs[0] = Value::empty();
                            });

    // ── dir / ls ──────────────────────────────────────────────
    engine.registerFunction("dir",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                std::string target = args.empty() ? std::string(".")
                                                                   : args[0].toString();
                                auto rp = ctx.engine->resolvePath(target);
                                if (!rp.fs)
                                    throw std::runtime_error("dir: cannot resolve '" + target + "'");
                                std::vector<DirEntry> entries;
                                auto st = rp.fs->stat(rp.path);
                                if (st && st->kind == FileStat::Kind::File) {
                                    DirEntry e;
                                    e.name = rp.path;
                                    e.isDirectory = false;
                                    entries.push_back(e);
                                } else {
                                    entries = rp.fs->listDir(rp.path);
                                }

                                if (nargout == 0) {
                                    // Print tabular listing (MATLAB-ish).
                                    std::ostringstream os;
                                    for (const auto &e : entries) {
                                        os << e.name;
                                        if (e.isDirectory) os << "/";
                                        os << "\n";
                                    }
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value::empty();
                                    return;
                                }

                                // n×1 struct array — same fields per element so MATLAB-style
                                // d(i).name / [d.bytes] usage works.
                                auto *mr = ctx.engine->resource();
                                if (entries.empty()) {
                                    outs[0] = Value::structure(mr);
                                    return;
                                }
                                Value arr = Value::structArray(entries.size(), 1, mr);
                                for (size_t i = 0; i < entries.size(); ++i) {
                                    auto &fields = arr.structArrayElem(i);
                                    fields["name"]    = Value::fromString(entries[i].name, mr);
                                    fields["folder"]  = Value::fromString(rp.path, mr);
                                    fields["isdir"]   = Value::logicalScalar(entries[i].isDirectory, mr);
                                    std::string full = rp.path;
                                    if (!full.empty() && full.back() != '/' && full.back() != '\\')
                                        full += '/';
                                    full += entries[i].name;
                                    auto est = rp.fs->stat(full);
                                    fields["bytes"]   = Value::scalar(est ? double(est->size) : 0.0, mr);
                                    fields["datenum"] = Value::scalar(est ? double(est->mtime) : 0.0, mr);
                                    fields["date"]    = Value::fromString(std::string{}, mr);
                                }
                                outs[0] = std::move(arr);
                            });

    engine.registerFunction("ls",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                std::string target = args.empty() ? std::string(".")
                                                                   : args[0].toString();
                                auto rp = ctx.engine->resolvePath(target);
                                if (!rp.fs)
                                    throw std::runtime_error("ls: cannot resolve '" + target + "'");
                                auto entries = rp.fs->listDir(rp.path);
                                if (nargout == 0) {
                                    std::ostringstream os;
                                    for (const auto &e : entries)
                                        os << e.name << "\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value::empty();
                                } else {
                                    // Return as newline-joined char vector (MATLAB ls semantics).
                                    std::ostringstream os;
                                    for (size_t i = 0; i < entries.size(); ++i) {
                                        if (i) os << "  ";
                                        os << entries[i].name;
                                    }
                                    outs[0] = Value::fromString(os.str(), ctx.engine->resource());
                                }
                            });

    // ── pathsep ───────────────────────────────────────────────
    // (filesep / fullfile / fileparts / tempdir / tempname moved to
    //  libs/io/src/paths/paths.cpp + compat alias — they belong to the
    //  io toolbox per the MATLAB taxonomy. pathsep stays here only
    //  because no io equivalent exists yet.)
    engine.registerFunction("pathsep",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
#ifdef _WIN32
                                outs[0] = Value::fromString(";", ctx.engine->resource());
#else
                                outs[0] = Value::fromString(":", ctx.engine->resource());
#endif
                            });

    // ── Pack 25: workspace / display utilities ────────────────────────

    // clearvars — clear named workspace variables. With no args, clears
    // all. With "-except name1 name2 ...", clears everything except the
    // listed names. Doesn't unload functions, never affects globals
    // (matches MATLAB's `clearvars` vs. `clear` distinction).
    engine.registerFunction("clearvars",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            auto *env = ctx.env;
            if (args.empty()) {
                env->clearAll();
                outs[0] = Value::empty();
                return;
            }
            // Parse "-except".
            bool exceptMode = false;
            std::vector<std::string> names;
            for (size_t i = 0; i < args.size(); ++i) {
                if (!args[i].isChar() && !args[i].isString()) continue;
                const std::string s = args[i].toString();
                if (s == "-except") { exceptMode = true; continue; }
                names.push_back(s);
            }
            if (exceptMode) {
                std::set<std::string> keep(names.begin(), names.end());
                for (const auto &n : env->localNames()) {
                    if (!keep.count(n)) env->remove(n);
                }
            } else {
                for (const auto &n : names) env->remove(n);
            }
            outs[0] = Value::empty();
        });

    // formatteddisplaytext(x) — return what disp(x) would print, but
    // as a string instead of writing it to the output stream.
    engine.registerFunction("formatteddisplaytext",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("formatteddisplaytext requires 1 argument");
            outs[0] = Value::fromString(args[0].formatDisplay(""),
                                         ctx.engine->resource());
        });

    // format(spec) — accepted for compatibility, no-op (numkit always
    // formats with ~15 significant digits). Recognised specs are
    // 'short', 'long', 'compact', 'loose', 'shortG', 'longG', 'shortE',
    // 'longE', 'rat', 'hex'; unknown specs throw.
    engine.registerFunction("format",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &) {
            if (!args.empty()) {
                if (!args[0].isChar() && !args[0].isString())
                    throw std::runtime_error(
                        "format: argument must be a string");
                static const std::set<std::string> known = {
                    "short", "long", "compact", "loose",
                    "shortg", "longg", "shorte", "longe",
                    "shorteng", "longeng", "rat", "hex", "bank", "+", "default"
                };
                std::string s = args[0].toString();
                for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (!known.count(s))
                    throw std::runtime_error(
                        "format: unrecognised format spec '" + s + "'");
            }
            // No-op: numkit's display already runs at full precision.
            outs[0] = Value::empty();
        });

    // ── Pack 31: misc gap-filling ─────────────────────────────────────

    // home — like clc; cursor to top + clear.
    engine.registerFunction("home",
        [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
            ctx.engine->outputText("__CLEAR__\n");
            outs[0] = Value::empty();
        });

    // pause(t) — block for t seconds. pause() with no arg is a no-op
    // (we don't have a wait-for-keypress hook).
    engine.registerFunction("pause",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &) {
            if (!args.empty() && !args[0].isEmpty()) {
                const double t = args[0].toScalar();
                if (std::isfinite(t) && t > 0) {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(static_cast<long long>(t * 1000.0)));
                }
            }
            outs[0] = Value::empty();
        });

    // iskeyword() / iskeyword(s) — list MATLAB reserved words / test one.
    engine.registerFunction("iskeyword",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            static const std::vector<std::string> kw = {
                "break", "case", "catch", "classdef", "continue", "else",
                "elseif", "end", "for", "function", "global", "if",
                "otherwise", "parfor", "persistent", "return", "spmd",
                "switch", "try", "while"
            };
            auto *mr = ctx.engine->resource();
            if (args.empty()) {
                auto c = Value::cell(kw.size(), 1, mr);
                for (size_t i = 0; i < kw.size(); ++i)
                    c.cellAt(i) = Value::fromString(kw[i], mr);
                outs[0] = std::move(c);
                return;
            }
            const std::string s = args[0].toString();
            outs[0] = Value::logicalScalar(
                std::find(kw.begin(), kw.end(), s) != kw.end(), mr);
        });

    // isvarname(s) — true if s is a valid MATLAB variable name: a non-empty
    // char vector / string scalar that starts with a letter, contains only
    // letters / digits / underscores, and is not a reserved keyword. Any
    // non-text input (numeric, cell, multi-element string) yields false rather
    // than erroring. R2025b imposes no length limit. vs MATLAB R2025b.
    engine.registerFunction("isvarname",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            auto *mr = ctx.engine->resource();
            if (args.empty())
                throw std::runtime_error("isvarname requires 1 argument");
            const Value &a = args[0];
            const bool isText = a.isChar() || (a.isString() && a.numel() == 1);
            bool ok = false;
            if (isText) {
                const std::string s = a.toString();
                ok = !s.empty()
                     && std::isalpha(static_cast<unsigned char>(s[0])) != 0;
                for (size_t i = 1; ok && i < s.size(); ++i) {
                    const unsigned char c = static_cast<unsigned char>(s[i]);
                    if (!(std::isalnum(c) || c == '_')) ok = false;
                }
                if (ok) {
                    static const std::vector<std::string> kw = {
                        "break", "case", "catch", "classdef", "continue",
                        "else", "elseif", "end", "for", "function", "global",
                        "if", "otherwise", "parfor", "persistent", "return",
                        "spmd", "switch", "try", "while"
                    };
                    if (std::find(kw.begin(), kw.end(), s) != kw.end())
                        ok = false;
                }
            }
            outs[0] = Value::logicalScalar(ok, mr);
        });

    // full(A) — convert sparse to dense. numkit doesn't have a sparse
    // class, so inputs are always dense and `full` is the identity.
    engine.registerFunction("full",
        [](Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
           CallContext & /*ctx*/) {
            if (args.empty())
                throw std::runtime_error("full requires 1 argument");
            outs[0] = args[0];
        });

    // freqspace(n) — frequency-spacing vector for FFT-style problems.
    // MATLAB R2025b semantics:
    //   freqspace(n) default form:
    //     n even → n/2+1 points on [0, 1]
    //     n odd  → (n+1)/2 points on [0, 1 - 1/n]
    //   freqspace(n, 'whole'):
    //     n points on [0, 2 - 2/n]
    // See BUGS.md #19.
    engine.registerFunction("freqspace",
        [](Span<const Value> args, size_t nargout, Span<Value> outs,
           CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error("freqspace requires (n[, 'whole'])");
            // Parse arg0 as scalar n or 2-vec [rows cols].
            size_t n_rows = 0, n_cols = 0;
            const Value &a0 = args[0];
            if (a0.numel() == 1) {
                n_rows = n_cols = static_cast<size_t>(a0.toScalar());
            } else if (a0.numel() == 2) {
                n_rows = static_cast<size_t>(a0.elemAsDouble(0));
                n_cols = static_cast<size_t>(a0.elemAsDouble(1));
            } else {
                throw std::runtime_error(
                    "freqspace: N must be a scalar or 2-element vector");
            }
            bool whole = false;
            if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
                std::string s = args[1].toString();
                for (auto &c : s)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                whole = (s == "whole");
            }
            auto *mr = ctx.engine->resource();

            // Helpers for the three frequency-vector flavors.
            auto whole_vec = [&](size_t n) {
                auto v = Value::matrix(1, n, ValueType::DOUBLE, mr);
                if (n == 0) return v;
                double *d = v.doubleDataMut();
                const double step = 2.0 / static_cast<double>(n);
                for (size_t i = 0; i < n; ++i)
                    d[i] = step * static_cast<double>(i);
                return v;
            };
            auto centered_vec = [&](size_t n) {
                // Even n: f = (-n:2:n-2)/n; Odd n: f = (-n+1:2:n-1)/n.
                auto v = Value::matrix(1, n, ValueType::DOUBLE, mr);
                if (n == 0) return v;
                double *d = v.doubleDataMut();
                const long start = (n % 2 == 0) ? -static_cast<long>(n)
                                                : -static_cast<long>(n) + 1;
                for (size_t i = 0; i < n; ++i)
                    d[i] = static_cast<double>(start + 2 * static_cast<long>(i))
                         / static_cast<double>(n);
                return v;
            };
            auto half_vec = [&](size_t n) {
                if (n == 0)
                    return Value::matrix(1, 0, ValueType::DOUBLE, mr);
                size_t m;
                double last;
                if (n % 2 == 0) { m = n / 2 + 1; last = 1.0; }
                else            { m = (n + 1) / 2;
                                  last = 1.0 - 1.0 / static_cast<double>(n); }
                auto v = Value::matrix(1, m, ValueType::DOUBLE, mr);
                double *d = v.doubleDataMut();
                if (m == 1) d[0] = 0.0;
                else {
                    const double step = last / static_cast<double>(m - 1);
                    for (size_t i = 0; i < m; ++i)
                        d[i] = step * static_cast<double>(i);
                }
                return v;
            };

            if (nargout > 1) {
                if (whole)
                    throw std::runtime_error(
                        "freqspace: 2-output 'whole' form is not supported");
                // [f1, f2]: f1 from cols, f2 from rows (centered).
                outs[0] = centered_vec(n_cols);
                outs[1] = centered_vec(n_rows);
                return;
            }
            // Single-output: scalar input only.
            if (a0.numel() == 2)
                throw std::runtime_error(
                    "freqspace: 2-vec input requires 2 output args");
            outs[0] = whole ? whole_vec(n_rows) : half_vec(n_rows);
        });

    // head(A[, n]) / tail(A[, n]) — first / last n rows of A. Defaults
    // to min(8, size(A, 1)) like MATLAB.
    auto headTailImpl = [](bool isHead) {
        return [isHead](Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx) {
            if (args.empty())
                throw std::runtime_error((isHead ? "head" : "tail")
                                         + std::string(" requires 1 argument"));
            const Value &A = args[0];
            if (A.dims().ndim() > 2)
                throw std::runtime_error("head/tail: ND inputs (>2) not supported");
            const size_t R = A.dims().rows(), C = A.dims().cols();
            size_t n = std::min<size_t>(R, 8);
            if (args.size() >= 2 && !args[1].isEmpty())
                n = static_cast<size_t>(args[1].toScalar());
            n = std::min(n, R);
            auto *mr = ctx.engine->resource();
            auto out = Value::matrix(n, C, A.type(), mr);
            const size_t es = elementSize(A.type());
            const char *src = static_cast<const char *>(A.rawData());
            char *dst = static_cast<char *>(out.rawDataMut());
            const size_t rowOff = isHead ? 0 : R - n;
            for (size_t c = 0; c < C; ++c) {
                std::memcpy(dst + (c * n) * es,
                            src + (c * R + rowOff) * es,
                            n * es);
            }
            outs[0] = std::move(out);
        };
    };
    engine.registerFunction("head", headTailImpl(true));
    engine.registerFunction("tail", headTailImpl(false));

    // ── Pack 36: type-predicate stubs for absent types ─────────────
    // These predicates always return logical false because numkit has
    // no categorical / table / timetable / datetime / duration types
    // yet. Returning false (instead of erroring) is correct MATLAB
    // behaviour — `iscategorical(double_array)` etc. all return false.
    // Lets MATLAB-source code that defensively checks the type port
    // without errors.
    //
    // Note: `isordinal` / `isprotected` do throw in MATLAB when the
    // input is non-categorical, so we deliberately do NOT stub those
    // here — keeping their absence preserves the type-error signal.
    auto alwaysFalsePredicate =
        [](Span<const Value>, size_t /*nargout*/, Span<Value> outs,
           CallContext &ctx) {
            outs[0] = Value::logicalScalar(false, ctx.engine->resource());
        };
    engine.registerFunction("iscategorical",     alwaysFalsePredicate);
    engine.registerFunction("istable",           alwaysFalsePredicate);
    engine.registerFunction("istimetable",       alwaysFalsePredicate);
    engine.registerFunction("istabular",         alwaysFalsePredicate);
    engine.registerFunction("isdatetime",        alwaysFalsePredicate);
    engine.registerFunction("isduration",        alwaysFalsePredicate);
    engine.registerFunction("iscalendarduration",alwaysFalsePredicate);
}

} // namespace numkit