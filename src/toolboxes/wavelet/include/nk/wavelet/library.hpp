/// @file library.hpp
/// @ingroup group_wavelet
// toolboxes/wavelet/include/nk/wavelet/library.hpp
//
// Wavelet builtins — function-form only.

#pragma once

namespace nk {

/// @addtogroup group_wavelet
/// @{
 class Engine; }  // fwd-decl — keep this public header core-free

namespace nk {

class WaveletLibrary
{
public:
    static void install(Engine &engine);
};


/// @}
} // namespace nk
