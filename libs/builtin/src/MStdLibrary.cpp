#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MTypes.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace numkit::m::builtin::detail {
// Forward declarations for Phase 6c public-API-backed adapters.
// Each is defined in the corresponding M<Name>.cpp translation unit.

// MStdMath.cpp
void sqrt_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void abs_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sin_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cos_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void tan_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void asin_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void acos_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void atan_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void atan2_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void exp_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void log_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void log2_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void log10_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void floor_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ceil_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void round_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fix_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void mod_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void rem_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sign_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void max_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void min_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sum_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void prod_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void mean_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdStats.cpp
void var_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void std_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void median_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void quantile_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void prctile_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void mode_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void skewness_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void kurtosis_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void primes_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isprime_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void factor_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void perms_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void factorial_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nchoosek_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nansum_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nanmean_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nanmax_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nanmin_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nanvar_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nanstd_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nanmedian_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void linspace_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void logspace_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
// MStdRng.cpp (rand/randn moved here from MStdMath.cpp; new functions added)
void rand_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void randn_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void randi_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void randperm_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void rng_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdManip.cpp
void repmat_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fliplr_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void flipud_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void rot90_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void circshift_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void tril_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void triu_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdNDManip.cpp
void permute_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ipermute_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void squeeze_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cat_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void blkdiag_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdMath.cpp (Phase 7 floating-point additions)
void hypot_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nthroot_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void expm1_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void log1p_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void gamma_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void gammaln_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void erf_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void erfc_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void erfinv_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdIntMath.cpp
void gcd_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void lcm_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void bitand_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void bitor_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void bitxor_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void bitshift_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void bitcmp_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdSetOps.cpp
void unique_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ismember_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void union_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void intersect_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void setdiff_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void histcounts_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void discretize_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdAccum.cpp
void accumarray_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void deg2rad_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void rad2deg_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdComplex.cpp
void real_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void imag_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void conj_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void complex_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void angle_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdStrings.cpp
void num2str_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void str2num_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void str2double_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void string_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void char_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strcmp_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strcmpi_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void upper_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void lower_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strtrim_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strsplit_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strcat_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strlength_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void strrep_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void contains_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void startsWith_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void endsWith_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdTypes.cpp
void double_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void single_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void int8_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void int16_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void int32_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void int64_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void uint8_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void uint16_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void uint32_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void uint64_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void logical_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isnumeric_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void islogical_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ischar_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isstring_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void iscell_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isstruct_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isempty_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isscalar_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isreal_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isinteger_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isfloat_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void issingle_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isnan_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isinf_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isfinite_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isequal_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isequaln_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void class_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdFormat.cpp
void sprintf_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdPrint.cpp
void disp_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fprintf_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdFileIO.cpp
void fopen_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fclose_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fgetl_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fgets_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void feof_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ferror_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ftell_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fseek_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void frewind_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fread_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fwrite_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdScan.cpp
void fscanf_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sscanf_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void textscan_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdCsv.cpp
void csvread_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void csvwrite_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdEnv.cpp
void setenv_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void getenv_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdSaveLoad.cpp
void save_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void load_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdDiagnostics.cpp
void error_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void warning_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void MException_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void rethrow_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void throw_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void assert_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdCellStruct.cpp
void struct_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void fieldnames_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void isfield_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void rmfield_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cell_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cellfun_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void structfun_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);

// MStdMatrix.cpp
void zeros_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ones_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void eye_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void size_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void length_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void numel_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void ndims_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void reshape_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void transpose_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void pagemtimes_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void diag_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sort_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void sortrows_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void find_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nnz_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void nonzeros_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void horzcat_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void vertcat_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void meshgrid_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cumsum_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cumprod_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cummax_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cummin_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void diff_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void any_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void all_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void xor_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void cross_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
void dot_reg(Span<const MValue>, size_t, Span<MValue>, CallContext&);
} // namespace numkit::m::builtin::detail

namespace numkit::m {

// ── Warning helper for unsupported features ──────────────────
static void warnNotSupported(CallContext &ctx, const std::string &feature)
{
    ctx.engine->outputText("Warning: '" + feature + "' is not yet supported.\n");
}

void StdLibrary::install(Engine &engine)
{
    registerBinaryOps(engine);
    registerUnaryOps(engine);
    registerTypeFunctions(engine);
    registerCellStructFunctions(engine);
    registerStringFunctions(engine);
    registerComplexFunctions(engine);

    registerWorkspaceBuiltins(engine);

    // ── Phase 6c: MStdMath public-API-backed built-ins ─────────────
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
    engine.registerFunction("skewness", &builtin::detail::skewness_reg);
    engine.registerFunction("kurtosis", &builtin::detail::kurtosis_reg);
    engine.registerFunction("primes",   &builtin::detail::primes_reg);
    engine.registerFunction("isprime",  &builtin::detail::isprime_reg);
    engine.registerFunction("factor",   &builtin::detail::factor_reg);
    engine.registerFunction("perms",     &builtin::detail::perms_reg);
    engine.registerFunction("factorial", &builtin::detail::factorial_reg);
    engine.registerFunction("nchoosek",  &builtin::detail::nchoosek_reg);

    // ── Phase 2 NaN-aware reductions ───────────────────────────────
    engine.registerFunction("nansum",    &builtin::detail::nansum_reg);
    engine.registerFunction("nanmean",   &builtin::detail::nanmean_reg);
    engine.registerFunction("nanmax",    &builtin::detail::nanmax_reg);
    engine.registerFunction("nanmin",    &builtin::detail::nanmin_reg);
    engine.registerFunction("nanvar",    &builtin::detail::nanvar_reg);
    engine.registerFunction("nanstd",    &builtin::detail::nanstd_reg);
    engine.registerFunction("nanmedian", &builtin::detail::nanmedian_reg);

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

    // ── Phase 6c: MStdMatrix public-API-backed built-ins ───────────
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

    // ── Phase 6c: MStdComplex public-API-backed built-ins ──────────
    engine.registerFunction("real",    &builtin::detail::real_reg);
    engine.registerFunction("imag",    &builtin::detail::imag_reg);
    engine.registerFunction("conj",    &builtin::detail::conj_reg);
    engine.registerFunction("complex", &builtin::detail::complex_reg);
    engine.registerFunction("angle",   &builtin::detail::angle_reg);

    // ── Phase 6c: MStdStrings public-API-backed built-ins ──────────
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

    // ── Phase 6c: MStdTypes public-API-backed built-ins ────────────
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

    // ── Phase 6c: MStdFormat public-API-backed built-ins ───────────
    engine.registerFunction("sprintf",    &builtin::detail::sprintf_reg);

    // ── Phase 6c: MStdPrint public-API-backed built-ins ────────────
    engine.registerFunction("disp",       &builtin::detail::disp_reg);
    engine.registerFunction("fprintf",    &builtin::detail::fprintf_reg);

    // ── Phase 6c: MStdFileIO public-API-backed built-ins ───────────
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

    // ── Phase 6c: MStdScan public-API-backed built-ins ─────────────
    engine.registerFunction("fscanf",     &builtin::detail::fscanf_reg);
    engine.registerFunction("sscanf",     &builtin::detail::sscanf_reg);
    engine.registerFunction("textscan",   &builtin::detail::textscan_reg);

    // ── Phase 6c: MStdCsv public-API-backed built-ins ──────────────
    engine.registerFunction("csvread",    &builtin::detail::csvread_reg);
    engine.registerFunction("csvwrite",   &builtin::detail::csvwrite_reg);

    // ── Phase 6c: MStdEnv public-API-backed built-ins ──────────────
    engine.registerFunction("setenv",     &builtin::detail::setenv_reg);
    engine.registerFunction("getenv",     &builtin::detail::getenv_reg);

    // ── Phase 6c: MStdSaveLoad public-API-backed built-ins ─────────
    engine.registerFunction("save",       &builtin::detail::save_reg);
    engine.registerFunction("load",       &builtin::detail::load_reg);

    // ── Phase 6c: MStdDiagnostics public-API-backed built-ins ──────
    engine.registerFunction("error",      &builtin::detail::error_reg);
    engine.registerFunction("warning",    &builtin::detail::warning_reg);
    engine.registerFunction("MException", &builtin::detail::MException_reg);
    engine.registerFunction("rethrow",    &builtin::detail::rethrow_reg);
    engine.registerFunction("throw",      &builtin::detail::throw_reg);
    engine.registerFunction("assert",     &builtin::detail::assert_reg);

    // ── Phase 6c: MStdCellStruct public-API-backed built-ins ───────
    engine.registerFunction("struct",     &builtin::detail::struct_reg);
    engine.registerFunction("fieldnames", &builtin::detail::fieldnames_reg);
    engine.registerFunction("isfield",    &builtin::detail::isfield_reg);
    engine.registerFunction("rmfield",    &builtin::detail::rmfield_reg);
    engine.registerFunction("cell",       &builtin::detail::cell_reg);
    engine.registerFunction("cellfun",    &builtin::detail::cellfun_reg);
    engine.registerFunction("structfun",  &builtin::detail::structfun_reg);

    // --- arrayfun (basic scalar version) ---
    engine.registerFunction("arrayfun",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
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

void StdLibrary::registerWorkspaceBuiltins(Engine &engine)
{
    // ── clear ──────────────────────────────────────────────────
    engine.registerFunction("clear",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
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
                                        outs[0] = MValue::empty();
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
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                    if (first == "import") {
                                        warnNotSupported(ctx, "clear import");
                                        outs[0] = MValue::empty();
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
                                outs[0] = MValue::empty();
                            });

    // ── clc ────────────────────────────────────────────────────
    engine.registerFunction("clc",
                            [](Span<const MValue>, size_t, Span<MValue> outs, CallContext &ctx) {
                                ctx.engine->outputText("__CLEAR__\n");
                                outs[0] = MValue::empty();
                            });

    // ── who ────────────────────────────────────────────────────
    engine.registerFunction("who",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // Check for unsupported flags
                                if (!args.empty() && args[0].isChar()) {
                                    std::string first = args[0].toString();
                                    if (first == "-file") {
                                        warnNotSupported(ctx, "who -file");
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                }

                                std::vector<std::string> names;
                                if (args.empty()) {
                                    // localNames() excludes parent-env constants
                                    // (pi, eps, …) — they show up here only if
                                    // shadowed in the workspace, as in MATLAB.
                                    names = env->localNames();
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
                                outs[0] = MValue::empty();
                            });

    // ── whos ───────────────────────────────────────────────────
    engine.registerFunction("whos",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                auto *env = ctx.env;

                                // Check for unsupported flags
                                if (!args.empty() && args[0].isChar()) {
                                    std::string first = args[0].toString();
                                    if (first == "-file") {
                                        warnNotSupported(ctx, "whos -file");
                                        outs[0] = MValue::empty();
                                        return;
                                    }
                                }

                                std::vector<std::string> names;
                                if (args.empty()) {
                                    // localNames() excludes parent-env constants
                                    // (pi, eps, …) — they show up here only if
                                    // shadowed in the workspace, as in MATLAB.
                                    names = env->localNames();
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
                                outs[0] = MValue::empty();
                            });

    // ── which ──────────────────────────────────────────────────
    engine.registerFunction("which",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
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
                                outs[0] = MValue::empty();
                            });

    // ── exist ──────────────────────────────────────────────────
    engine.registerFunction("exist",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
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
                                    outs[0] = MValue::scalar(0.0, &ctx.engine->allocator());
                                    return;
                                }
                                if (typeFilter == "class") {
                                    warnNotSupported(ctx, "exist(name, 'class')");
                                    outs[0] = MValue::scalar(0.0, &ctx.engine->allocator());
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

                                outs[0] = MValue::scalar(code, &ctx.engine->allocator());
                            });

    // ── class ──────────────────────────────────────────────────
    engine.registerFunction("class",
                            [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("class requires an argument");
                                outs[0] = MValue::fromString(mtypeName(args[0].type()),
                                                             &ctx.engine->allocator());
                            });

    // ── tic ────────────────────────────────────────────────────
    engine.registerFunction("tic",
                            [](Span<const MValue>,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto now = Clock::now();
                                ctx.engine->setTicTimer(now);
                                if (nargout > 0) {
                                    double id = static_cast<double>(
                                        std::chrono::duration_cast<std::chrono::microseconds>(
                                            now.time_since_epoch())
                                            .count());
                                    outs[0] = MValue::scalar(id, &ctx.engine->allocator());
                                } else {
                                    outs[0] = MValue::empty();
                                }
                            });

    // ── toc ────────────────────────────────────────────────────
    engine.registerFunction("toc",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
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
                                    outs[0] = MValue::scalar(elapsed, &ctx.engine->allocator());
                                } else {
                                    std::ostringstream os;
                                    os << "Elapsed time is " << elapsed << " seconds.\n";
                                    ctx.engine->outputText(os.str());
                                    outs[0] = MValue::empty();
                                }
                            });
}

} // namespace numkit::m