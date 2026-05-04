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
// dwt/dwt.cpp
void dwt_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void idwt_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::wavelet::detail

namespace numkit {

void WaveletLibrary::install(Engine &engine)
{
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("wavelet.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    reg("filt", "wfilters", &wavelet::detail::wfilters_reg);
    reg("dwt",  "dwt",      &wavelet::detail::dwt_reg);
    reg("dwt",  "idwt",     &wavelet::detail::idwt_reg);
}

} // namespace numkit
