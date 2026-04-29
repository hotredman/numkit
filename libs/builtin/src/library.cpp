#include <numkit/builtin/library.hpp>

#include <numkit/core/scratch_arena.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
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
void max_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void min_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sum_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void prod_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mean_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// stats.cpp
void var_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void std_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void median_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void quantile_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void prctile_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mode_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
// skewness / kurtosis moved to libs/stats (see StatsLibrary::install)
void cov_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void corrcoef_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
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
void spline_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pchip_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyfit_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyval_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void trapz_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fzero_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void integral_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void roots_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyder_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyint_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
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

// nd_manip.cpp
void permute_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ipermute_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void squeeze_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cat_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void blkdiag_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// math/elementary/ (Phase 7 floating-point additions)
void hypot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nthroot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void expm1_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log1p_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gamma_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gammaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void erfinv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// int_math.cpp
void gcd_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void lcm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitxor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitshift_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitcmp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// discrete.cpp
void unique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismember_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void union_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intersect_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setdiff_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histcounts_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void discretize_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// accum.cpp
void accumarray_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void deg2rad_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rad2deg_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// complex.cpp
void real_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void imag_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void conj_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void complex_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void angle_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// strings.cpp
void num2str_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
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
void startsWith_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void endsWith_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexpi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void regexprep_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

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
void isnan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isinf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isfinite_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequal_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void isequaln_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void class_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// format.cpp
void sprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// print.cpp
void disp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fprintf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// fileio.cpp
void fopen_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fclose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fgetl_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fgets_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void feof_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ferror_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ftell_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fseek_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void frewind_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fread_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void fwrite_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// scan.cpp
void fscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sscanf_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void textscan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// csv.cpp
void csvread_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void csvwrite_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// env.cpp
void setenv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void getenv_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// saveload.cpp
void save_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void load_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// diagnostics.cpp
void error_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void warning_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
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
void structfun_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

// matrix.cpp
void zeros_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ones_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void eye_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void size_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void length_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void numel_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ndims_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void reshape_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void transpose_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
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
void kron_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cumsum_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cumprod_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cummax_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cummin_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void diff_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void any_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void all_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void xor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void cross_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void dot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
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
    engine.registerFunction("max",      &builtin::detail::max_reg);
    engine.registerFunction("min",      &builtin::detail::min_reg);
    engine.registerFunction("sum",      &builtin::detail::sum_reg);
    engine.registerFunction("prod",     &builtin::detail::prod_reg);
    engine.registerFunction("mean",     &builtin::detail::mean_reg);

    // ── Phase 1 stats: var/std/median/quantile/prctile/mode ────────
    engine.registerFunction("var",      &builtin::detail::var_reg);
    engine.registerFunction("std",      &builtin::detail::std_reg);
    engine.registerFunction("median",   &builtin::detail::median_reg);
    engine.registerFunction("quantile", &builtin::detail::quantile_reg);
    engine.registerFunction("prctile",  &builtin::detail::prctile_reg);
    engine.registerFunction("mode",     &builtin::detail::mode_reg);
    // skewness / kurtosis registered by StatsLibrary::install()
    engine.registerFunction("cov",      &builtin::detail::cov_reg);
    engine.registerFunction("corrcoef", &builtin::detail::corrcoef_reg);
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
    engine.registerFunction("spline",    &builtin::detail::spline_reg);
    engine.registerFunction("pchip",     &builtin::detail::pchip_reg);
    engine.registerFunction("polyfit",   &builtin::detail::polyfit_reg);
    engine.registerFunction("polyval",   &builtin::detail::polyval_reg);
    engine.registerFunction("trapz",     &builtin::detail::trapz_reg);
    engine.registerFunction("fzero",     &builtin::detail::fzero_reg);
    engine.registerFunction("integral",  &builtin::detail::integral_reg);
    engine.registerFunction("roots",     &builtin::detail::roots_reg);
    engine.registerFunction("polyder",   &builtin::detail::polyder_reg);
    engine.registerFunction("polyint",   &builtin::detail::polyint_reg);
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

    // ── Phase 6 N-D manipulation ──────────────────────────────────
    engine.registerFunction("permute",  &builtin::detail::permute_reg);
    engine.registerFunction("ipermute", &builtin::detail::ipermute_reg);
    engine.registerFunction("squeeze",  &builtin::detail::squeeze_reg);
    engine.registerFunction("cat",      &builtin::detail::cat_reg);
    engine.registerFunction("blkdiag",  &builtin::detail::blkdiag_reg);

    // ── Phase 7 numeric utilities ─────────────────────────────────
    engine.registerFunction("hypot",    &builtin::detail::hypot_reg);
    engine.registerFunction("nthroot",  &builtin::detail::nthroot_reg);
    engine.registerFunction("expm1",    &builtin::detail::expm1_reg);
    engine.registerFunction("log1p",    &builtin::detail::log1p_reg);
    engine.registerFunction("gamma",    &builtin::detail::gamma_reg);
    engine.registerFunction("gammaln",  &builtin::detail::gammaln_reg);
    engine.registerFunction("erf",      &builtin::detail::erf_reg);
    engine.registerFunction("erfc",     &builtin::detail::erfc_reg);
    engine.registerFunction("erfinv",   &builtin::detail::erfinv_reg);
    engine.registerFunction("gcd",      &builtin::detail::gcd_reg);
    engine.registerFunction("lcm",      &builtin::detail::lcm_reg);
    engine.registerFunction("bitand",   &builtin::detail::bitand_reg);
    engine.registerFunction("bitor",    &builtin::detail::bitor_reg);
    engine.registerFunction("bitxor",   &builtin::detail::bitxor_reg);
    engine.registerFunction("bitshift", &builtin::detail::bitshift_reg);
    engine.registerFunction("bitcmp",   &builtin::detail::bitcmp_reg);

    // ── Phase 8 set / search ops ──────────────────────────────────
    engine.registerFunction("unique",     &builtin::detail::unique_reg);
    engine.registerFunction("ismember",   &builtin::detail::ismember_reg);
    engine.registerFunction("union",      &builtin::detail::union_reg);
    engine.registerFunction("intersect",  &builtin::detail::intersect_reg);
    engine.registerFunction("setdiff",    &builtin::detail::setdiff_reg);
    engine.registerFunction("histcounts", &builtin::detail::histcounts_reg);
    engine.registerFunction("discretize", &builtin::detail::discretize_reg);
    engine.registerFunction("accumarray", &builtin::detail::accumarray_reg);
    engine.registerFunction("deg2rad",  &builtin::detail::deg2rad_reg);
    engine.registerFunction("rad2deg",  &builtin::detail::rad2deg_reg);

    // ── Phase 6c: matrix.cpp public-API-backed built-ins ───────────
    engine.registerFunction("zeros",     &builtin::detail::zeros_reg);
    engine.registerFunction("ones",      &builtin::detail::ones_reg);
    engine.registerFunction("eye",       &builtin::detail::eye_reg);
    engine.registerFunction("size",      &builtin::detail::size_reg);
    engine.registerFunction("length",    &builtin::detail::length_reg);
    engine.registerFunction("numel",     &builtin::detail::numel_reg);
    engine.registerFunction("ndims",     &builtin::detail::ndims_reg);
    engine.registerFunction("reshape",   &builtin::detail::reshape_reg);
    engine.registerFunction("transpose", &builtin::detail::transpose_reg);
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
    engine.registerFunction("kron",      &builtin::detail::kron_reg);
    engine.registerFunction("cumsum",    &builtin::detail::cumsum_reg);
    engine.registerFunction("cumprod",   &builtin::detail::cumprod_reg);
    engine.registerFunction("cummax",    &builtin::detail::cummax_reg);
    engine.registerFunction("cummin",    &builtin::detail::cummin_reg);
    engine.registerFunction("diff",      &builtin::detail::diff_reg);
    engine.registerFunction("any",       &builtin::detail::any_reg);
    engine.registerFunction("all",       &builtin::detail::all_reg);
    engine.registerFunction("xor",       &builtin::detail::xor_reg);
    engine.registerFunction("cross",     &builtin::detail::cross_reg);
    engine.registerFunction("dot",       &builtin::detail::dot_reg);

    // ── Phase 6c: math/elementary/complex.cpp public-API-backed built-ins ──────────
    engine.registerFunction("real",    &builtin::detail::real_reg);
    engine.registerFunction("imag",    &builtin::detail::imag_reg);
    engine.registerFunction("conj",    &builtin::detail::conj_reg);
    engine.registerFunction("complex", &builtin::detail::complex_reg);
    engine.registerFunction("angle",   &builtin::detail::angle_reg);

    // ── Phase 6c: strings.cpp public-API-backed built-ins ──────────
    engine.registerFunction("num2str",    &builtin::detail::num2str_reg);
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
    engine.registerFunction("regexp",     &builtin::detail::regexp_reg);
    engine.registerFunction("regexpi",    &builtin::detail::regexpi_reg);
    engine.registerFunction("regexprep",  &builtin::detail::regexprep_reg);

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
    engine.registerFunction("isnan",     &builtin::detail::isnan_reg);
    engine.registerFunction("isinf",     &builtin::detail::isinf_reg);
    engine.registerFunction("isfinite",  &builtin::detail::isfinite_reg);
    engine.registerFunction("isequal",   &builtin::detail::isequal_reg);
    engine.registerFunction("isequaln",  &builtin::detail::isequaln_reg);
    engine.registerFunction("class",     &builtin::detail::class_reg);

    // ── Phase 6c: datatypes/strings/format.cpp public-API-backed built-ins ───────────
    engine.registerFunction("sprintf",    &builtin::detail::sprintf_reg);

    // ── Phase 6c: print.cpp public-API-backed built-ins ────────────
    engine.registerFunction("disp",       &builtin::detail::disp_reg);
    engine.registerFunction("fprintf",    &builtin::detail::fprintf_reg);

    // ── Phase 6c: data_io/fileio.cpp public-API-backed built-ins ───────────
    engine.registerFunction("fopen",      &builtin::detail::fopen_reg);
    engine.registerFunction("fclose",     &builtin::detail::fclose_reg);
    engine.registerFunction("fgetl",      &builtin::detail::fgetl_reg);
    engine.registerFunction("fgets",      &builtin::detail::fgets_reg);
    engine.registerFunction("feof",       &builtin::detail::feof_reg);
    engine.registerFunction("ferror",     &builtin::detail::ferror_reg);
    engine.registerFunction("ftell",      &builtin::detail::ftell_reg);
    engine.registerFunction("fseek",      &builtin::detail::fseek_reg);
    engine.registerFunction("frewind",    &builtin::detail::frewind_reg);
    engine.registerFunction("fread",      &builtin::detail::fread_reg);
    engine.registerFunction("fwrite",     &builtin::detail::fwrite_reg);

    // ── Phase 6c: scan.cpp public-API-backed built-ins ─────────────
    engine.registerFunction("fscanf",     &builtin::detail::fscanf_reg);
    engine.registerFunction("sscanf",     &builtin::detail::sscanf_reg);
    engine.registerFunction("textscan",   &builtin::detail::textscan_reg);

    // ── Phase 6c: data_io/csv.cpp public-API-backed built-ins ──────────────
    engine.registerFunction("csvread",    &builtin::detail::csvread_reg);
    engine.registerFunction("csvwrite",   &builtin::detail::csvwrite_reg);

    // ── Phase 6c: lang/commands/env.cpp public-API-backed built-ins ──────────────
    engine.registerFunction("setenv",     &builtin::detail::setenv_reg);
    engine.registerFunction("getenv",     &builtin::detail::getenv_reg);

    // ── Phase 6c: saveload.cpp public-API-backed built-ins ─────────
    engine.registerFunction("save",       &builtin::detail::save_reg);
    engine.registerFunction("load",       &builtin::detail::load_reg);

    // ── Phase 6c: programming/errors/diagnostics.cpp public-API-backed built-ins ──────
    engine.registerFunction("error",      &builtin::detail::error_reg);
    engine.registerFunction("warning",    &builtin::detail::warning_reg);
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
    engine.registerFunction("structfun",  &builtin::detail::structfun_reg);

    // --- arrayfun (basic scalar version) ---
    engine.registerFunction("arrayfun",
                            [](Span<const Value> args,
                               size_t nargout,
                               Span<Value> outs,
                               CallContext &ctx) {
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "arrayfun requires at least 2 arguments");
                                {
                                    outs[0] = args[1];
                                    return;
                                }
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

                                    // Unsupported flags
                                    if (first == "-regexp") {
                                        warnNotSupported(ctx, "clear -regexp");
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
                                        warnNotSupported(ctx, "clear import");
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

                                // Check for unsupported flags
                                if (!args.empty() && args[0].isChar()) {
                                    std::string first = args[0].toString();
                                    if (first == "-file") {
                                        warnNotSupported(ctx, "who -file");
                                        outs[0] = Value::empty();
                                        return;
                                    }
                                }

                                ScratchArena scratch_arena(ctx.engine->allocator());
                                ScratchVec<std::string> names(scratch_arena.resource());
                                if (args.empty()) {
                                    // localNames() excludes parent-env constants
                                    // (pi, eps, …) — they show up here only if
                                    // shadowed in the workspace, as in MATLAB.
                                    auto src = env->localNames();
                                    names.assign(src.begin(), src.end());
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName))
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

                                // Check for unsupported flags
                                if (!args.empty() && args[0].isChar()) {
                                    std::string first = args[0].toString();
                                    if (first == "-file") {
                                        warnNotSupported(ctx, "whos -file");
                                        outs[0] = Value::empty();
                                        return;
                                    }
                                }

                                ScratchArena scratch_arena(ctx.engine->allocator());
                                ScratchVec<std::string> names(scratch_arena.resource());
                                if (args.empty()) {
                                    // localNames() excludes parent-env constants
                                    // (pi, eps, …) — they show up here only if
                                    // shadowed in the workspace, as in MATLAB.
                                    auto src = env->localNames();
                                    names.assign(src.begin(), src.end());
                                } else {
                                    for (auto &a : args) {
                                        if (a.isChar()) {
                                            std::string varName = a.toString();
                                            if (env->getLocal(varName))
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
                                else
                                    os << "'" << qname << "' not found.\n";

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

                                // Unsupported type filters
                                if (typeFilter == "file" || typeFilter == "dir") {
                                    warnNotSupported(ctx, "exist(name, '" + typeFilter + "')");
                                    outs[0] = Value::scalar(0.0, &ctx.engine->allocator());
                                    return;
                                }
                                if (typeFilter == "class") {
                                    warnNotSupported(ctx, "exist(name, 'class')");
                                    outs[0] = Value::scalar(0.0, &ctx.engine->allocator());
                                    return;
                                }

                                double code = 0;
                                // Check local scope only for variables (don't leak to parent)
                                bool isVar = (env->getLocal(varName) != nullptr);
                                // Also check global declarations in current env
                                if (!isVar && env->isGlobal(varName)) {
                                    auto *gs = env->globalsEnv();
                                    isVar = (gs && gs->get(varName) != nullptr);
                                }
                                bool isFunc = ctx.engine->hasFunction(varName);

                                if (typeFilter.empty()) {
                                    // No filter: return first match
                                    if (isVar)
                                        code = 1;
                                    else if (isFunc)
                                        code = 5;
                                } else if (typeFilter == "var") {
                                    if (isVar)
                                        code = 1;
                                } else if (typeFilter == "builtin") {
                                    if (ctx.engine->hasExternalFunction(varName))
                                        code = 5;
                                }

                                outs[0] = Value::scalar(code, &ctx.engine->allocator());
                            });

    // ── class ──────────────────────────────────────────────────
    engine.registerFunction("class",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("class requires an argument");
                                outs[0] = Value::fromString(mtypeName(args[0].type()),
                                                             &ctx.engine->allocator());
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
                                    outs[0] = Value::scalar(id, &ctx.engine->allocator());
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
                                    outs[0] = Value::scalar(elapsed, &ctx.engine->allocator());
                                } else {
                                    std::ostringstream os;
                                    os << "Elapsed time is " << elapsed << " seconds.\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = Value::empty();
                                }
                            });
}

} // namespace numkit