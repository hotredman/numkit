// libs/wavelet/include/numkit/wavelet/library.hpp
//
// Wavelet Toolbox builtins. Mirrors MATLAB's documentation root
// `/help/wavelet/`. Function-form only.

#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class WaveletLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
