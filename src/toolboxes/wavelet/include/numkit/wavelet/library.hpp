/// @file library.hpp
/// @ingroup group_wavelet
// toolboxes/wavelet/include/numkit/wavelet/library.hpp
//
// Wavelet builtins — function-form only.

#pragma once

namespace numkit {

/// @addtogroup group_wavelet
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace numkit {

class WaveletLibrary
{
public:
    static void install(Engine &engine);
};


/// @}
} // namespace numkit
