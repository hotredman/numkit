#include <numkit/builtin/library.hpp>

#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <random>
#include <regex>
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

    engine.registerFunction("run",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                if (args.empty() || !args[0].isChar())
                                    throw std::runtime_error("run requires a string filename");
                                std::string p = args[0].toString();
                                auto rp = ctx.engine->resolvePath(p);
                                if (!rp.fs || !rp.fs->exists(rp.path))
                                    throw std::runtime_error("run: file not found: " + p);
                                std::string content = rp.fs->readFile(rp.path);
                                ctx.engine->eval(content);
                                outs[0] = Value::empty();
                            });

    // ── pwd / cd (Phase 9c) ────────────────────────────────────
    engine.registerFunction("pwd",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
                                std::string c = ctx.engine->cwd();
                                if (c.empty()) {
                                    // Resolve "." against active backend to surface a usable path.
                                    try {
                                        auto rp = ctx.engine->resolvePath(".");
                                        if (rp.fs) {
                                            auto st = rp.fs->stat(rp.path);
                                            if (st && st->kind == FileStat::Kind::Directory)
                                                c = rp.path;
                                        }
                                    } catch (...) {}
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

    // ── tempdir / tempname ────────────────────────────────────
    engine.registerFunction("tempdir",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
                                // Prefer the active backend's tempArea; fall back to "native".
                                std::string t;
                                try {
                                    auto rp = ctx.engine->resolvePath(".");
                                    if (rp.fs) t = rp.fs->tempArea();
                                } catch (...) {}
                                if (t.empty()) {
                                    if (auto *fs = ctx.engine->findVirtualFS("native"))
                                        t = fs->tempArea();
                                }
                                outs[0] = Value::fromString(t, ctx.engine->resource());
                            });

    engine.registerFunction("tempname",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
                                std::string t;
                                try {
                                    auto rp = ctx.engine->resolvePath(".");
                                    if (rp.fs) t = rp.fs->tempArea();
                                } catch (...) {}
                                if (t.empty()) {
                                    if (auto *fs = ctx.engine->findVirtualFS("native"))
                                        t = fs->tempArea();
                                }
                                if (!t.empty() && t.back() != '/' && t.back() != '\\')
                                    t += '/';
                                // Combine three sources of entropy so collisions across
                                // processes / engines / threads are statistically negligible:
                                //   * 64-bit nanosecond timestamp (monotonic, broad)
                                //   * thread-local random_device draw (per-process surprise)
                                //   * atomic counter (within-process tie-breaker)
                                static std::atomic<uint64_t> ctr{0};
                                thread_local std::mt19937_64 rng{std::random_device{}()};
                                uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                  Clock::now().time_since_epoch()).count();
                                uint64_t r = rng();
                                uint64_t c = ctr.fetch_add(1, std::memory_order_relaxed);
                                std::ostringstream os;
                                os << t << "tp" << std::hex
                                   << ns << "_" << r << "_" << c;
                                outs[0] = Value::fromString(os.str(), ctx.engine->resource());
                            });

    // ── filesep / pathsep ─────────────────────────────────────
    engine.registerFunction("filesep",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
#ifdef _WIN32
                                outs[0] = Value::fromString("\\", ctx.engine->resource());
#else
                                outs[0] = Value::fromString("/", ctx.engine->resource());
#endif
                            });

    engine.registerFunction("pathsep",
                            [](Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
#ifdef _WIN32
                                outs[0] = Value::fromString(";", ctx.engine->resource());
#else
                                outs[0] = Value::fromString(":", ctx.engine->resource());
#endif
                            });

    // ── fullfile / fileparts ──────────────────────────────────
    engine.registerFunction("fullfile",
                            [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
                                std::string out;
                                for (const auto &a : args) {
                                    if (!a.isChar()) continue;
                                    std::string s = a.toString();
                                    if (s.empty()) continue;
                                    if (out.empty())
                                        out = s;
                                    else {
                                        if (out.back() != '/' && out.back() != '\\')
                                            out += '/';
                                        out += s;
                                    }
                                }
                                outs[0] = Value::fromString(out, ctx.engine->resource());
                            });

    engine.registerFunction("fileparts",
                            [](Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
                                if (args.empty() || !args[0].isChar())
                                    throw std::runtime_error("fileparts: filename must be a string");
                                std::string p = args[0].toString();
                                // Find last separator.
                                auto sep = p.find_last_of("/\\");
                                std::string dir, base;
                                if (sep == std::string::npos) {
                                    base = p;
                                } else {
                                    dir = p.substr(0, sep);
                                    base = p.substr(sep + 1);
                                }
                                // Split base on last '.'.
                                std::string name, ext;
                                auto dot = base.find_last_of('.');
                                if (dot == std::string::npos || dot == 0) {
                                    name = base;
                                } else {
                                    name = base.substr(0, dot);
                                    ext = base.substr(dot);
                                }
                                auto *mr = ctx.engine->resource();
                                outs[0] = Value::fromString(dir, mr);
                                if (nargout > 1) outs[1] = Value::fromString(name, mr);
                                if (nargout > 2) outs[2] = Value::fromString(ext, mr);
                            });
}

} // namespace numkit