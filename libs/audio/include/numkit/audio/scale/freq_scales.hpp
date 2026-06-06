// libs/audio/include/numkit/audio/scale/freq_scales.hpp
//
// Frequency-scale conversions (Mel / Bark / ERB / phon-sone) used in
// audio feature extraction. All elementwise, double in / out.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::audio {

/// Hertz → Mel (O'Shaughnessy 1987 default form).
///
/// \f$ m = 2595 \log_{10}(1 + f / 700) \f$
///
/// @param hz  Frequency values in Hz (real array).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE array of mel-frequency values.
///
/// @see mel2hz
Value hz2mel(const Value &hz, std::pmr::memory_resource *mr = nullptr);

/// Mel → Hertz (inverse of `hz2mel`).
///
/// @param mel  Mel-frequency values.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Same-shape DOUBLE array of Hz values.
Value mel2hz(const Value &mel, std::pmr::memory_resource *mr = nullptr);

/// Hertz → Bark (Traunmüller 1990 with low/high-frequency corrections).
///
/// @param hz  Frequency values in Hz.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE array.
///
/// @see bark2hz
Value hz2bark(const Value &hz, std::pmr::memory_resource *mr = nullptr);

/// @brief Bark → Hertz (inverse of @ref hz2bark).
///
/// @param bark  Bark-scale values.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Same-shape DOUBLE array of Hz values.
/// @see hz2bark
Value bark2hz(const Value &bark, std::pmr::memory_resource *mr = nullptr);

/// Hertz → ERB (Glasberg & Moore 1990).
///
/// Equivalent Rectangular Bandwidth scale used in psychoacoustic models.
///
/// @param hz  Frequency values in Hz.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape DOUBLE array.
///
/// @see erb2hz
Value hz2erb(const Value &hz, std::pmr::memory_resource *mr = nullptr);

/// @brief ERB → Hertz (inverse of @ref hz2erb).
///
/// @param erb  ERB-scale values.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Same-shape DOUBLE array of Hz values.
/// @see hz2erb
Value erb2hz(const Value &erb, std::pmr::memory_resource *mr = nullptr);

/// Phon → Sone — loudness-level to loudness conversion.
///
/// Default standard ISO 532-1: closed-form piecewise power law.
/// Pass `standardIs532_2 = true` to use ISO 532-2 (table lookup with
/// PCHIP interpolation per Table 5, linear extrapolation beyond 120 phon).
///
/// @param phon              Loudness level in phon.
/// @param standardIs532_2   Use ISO 532-2 table form instead of 532-1.
/// @param mr                Memory resource (nullptr → process default).
/// @return                  Same-shape DOUBLE array of loudness in sone.
///
/// @see sone2phon
Value phon2sone(const Value &phon,
                bool standardIs532_2 = false,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Sone → Phon — loudness to loudness-level conversion.
///
/// Inverse of @ref phon2sone.
///
/// @param sone              Loudness in sone (any shape).
/// @param standardIs532_2   Use ISO 532-2 table form instead of 532-1.
/// @param mr                Memory resource (nullptr → process default).
/// @return                  Same-shape DOUBLE array of loudness in phon.
/// @see phon2sone
Value sone2phon(const Value &sone,
                bool standardIs532_2 = false,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::audio
