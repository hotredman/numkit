// toolboxes/wavelet/include/numkit/wavelet/library.hpp
//
// Wavelet builtins — function-form only.

#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class WaveletLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
