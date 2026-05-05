// libs/wavelet/src/library.cpp
//
// Registration hub for the Wavelet Toolbox builtins.
// Namespace: wavelet.<sub>.<name>; every function is also aliased into
// `compat.<name>` so MATLAB-style scripts can call them flat.

#include <numkit/wavelet/library.hpp>

#include <numkit/core/types.hpp>

namespace numkit::wavelet::detail {
// filter/wfilters.cpp
void wfilters_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
// filter/qmf.cpp
void qmf_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void wrev_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
// dwt/dwt.cpp
void dwt_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void idwt_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
// dwt/multilevel.cpp
void wavedec_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void waverec_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void appcoef_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void detcoef_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
// dwt/dwt2.cpp
void dwt2_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void idwt2_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
// dwt/dyad.cpp
void dyaddown_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void dyadup_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void wmaxlev_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
// dwt/wkeep_wextend.cpp
void wkeep_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void wextend_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
// denoise/denoise.cpp
void wthresh_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void wnoisest_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void wdenoise_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
// swt/swt.cpp
void swt_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void iswt_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void modwt_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imodwt_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
// shape/shape.cpp
void mexihat_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void morlet_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void meyeraux_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void shanwavf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void cmorwavf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void fbspwavf_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::wavelet::detail

namespace numkit {

void WaveletLibrary::install(Engine &engine)
{
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("wavelet.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    reg("filt", "wfilters", &wavelet::detail::wfilters_reg);
    reg("filt", "qmf",      &wavelet::detail::qmf_reg);
    reg("filt", "wrev",     &wavelet::detail::wrev_reg);
    reg("dwt",  "dwt",      &wavelet::detail::dwt_reg);
    reg("dwt",  "idwt",     &wavelet::detail::idwt_reg);
    reg("dwt",  "wavedec",  &wavelet::detail::wavedec_reg);
    reg("dwt",  "waverec",  &wavelet::detail::waverec_reg);
    reg("dwt",  "appcoef",  &wavelet::detail::appcoef_reg);
    reg("dwt",  "detcoef",  &wavelet::detail::detcoef_reg);
    reg("dwt",  "dyaddown", &wavelet::detail::dyaddown_reg);
    reg("dwt",  "dyadup",   &wavelet::detail::dyadup_reg);
    reg("dwt",  "wmaxlev",  &wavelet::detail::wmaxlev_reg);
    reg("dwt",  "wkeep",    &wavelet::detail::wkeep_reg);
    reg("dwt",  "wextend",  &wavelet::detail::wextend_reg);
    reg("dwt2", "dwt2",     &wavelet::detail::dwt2_reg);
    reg("dwt2", "idwt2",    &wavelet::detail::idwt2_reg);

    reg("denoise", "wthresh",  &wavelet::detail::wthresh_reg);
    reg("denoise", "wnoisest", &wavelet::detail::wnoisest_reg);
    reg("denoise", "wdenoise", &wavelet::detail::wdenoise_reg);

    reg("swt", "swt",    &wavelet::detail::swt_reg);
    reg("swt", "iswt",   &wavelet::detail::iswt_reg);
    reg("swt", "modwt",  &wavelet::detail::modwt_reg);
    reg("swt", "imodwt", &wavelet::detail::imodwt_reg);

    reg("shape", "mexihat",  &wavelet::detail::mexihat_reg);
    reg("shape", "morlet",   &wavelet::detail::morlet_reg);
    reg("shape", "meyeraux", &wavelet::detail::meyeraux_reg);
    reg("shape", "shanwavf", &wavelet::detail::shanwavf_reg);
    reg("shape", "cmorwavf", &wavelet::detail::cmorwavf_reg);
    reg("shape", "fbspwavf", &wavelet::detail::fbspwavf_reg);
}

} // namespace numkit
