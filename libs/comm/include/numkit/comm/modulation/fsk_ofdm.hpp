// libs/comm/include/numkit/comm/modulation/fsk_ofdm.hpp
//
// Frequency-shift keying and OFDM modulation.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit::comm {

/// @brief M-ary FSK modulator
/// (`y = fskmod(x, M, freq_sep, nsamp, fs, phase_continuity, symbol_order)`).
///
/// @param x                 Integer symbols in `0..M-1` (column or row).
/// @param M                 Number of frequency tones.
/// @param freq_sep          Spacing between tones in Hz.
/// @param nsamp             Samples per symbol.
/// @param fs                Sample rate in Hz.
/// @param phase_continuity  `"cont"` (default) or `"discont"`.
/// @param symbol_order      `"gray"` (default) or `"bin"`.
/// @param mr                Memory resource (nullptr → process default).
/// @return                  Complex baseband waveform of length
///                          `numel(x)·nsamp`.
/// @see fskdemod
Value fskmod(const Value &x, int M, double freq_sep, int nsamp,
             double fs, const std::string &phase_continuity,
             const std::string &symbol_order,
             std::pmr::memory_resource *mr = nullptr);

/// @brief M-ary FSK demodulator
/// (`x = fskdemod(y, M, freq_sep, nsamp, fs, symbol_order)`).
///
/// Per-symbol energy detection across the `M` candidate tones.
///
/// @param y             Complex baseband waveform.
/// @param M             Number of frequency tones.
/// @param freq_sep      Spacing between tones in Hz.
/// @param nsamp         Samples per symbol.
/// @param fs            Sample rate in Hz.
/// @param symbol_order  `"gray"` (default) or `"bin"`.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Integer symbol vector of length
///                      `numel(y) / nsamp`.
/// @see fskmod
Value fskdemod(const Value &y, int M, double freq_sep, int nsamp,
               double fs, const std::string &symbol_order,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Basic OFDM modulator
/// (`y = ofdmmod(in, nfft, cplen)`).
///
/// @param in     `nfft`×Nsymbols complex matrix of subcarrier values.
/// @param nfft   IFFT length.
/// @param cplen  Cyclic prefix length.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Column vector of length `(nfft + cplen)·Nsymbols`.
/// @see ofdmdemod
Value ofdmmod(const Value &in, int nfft, int cplen,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Basic OFDM demodulator
/// (`out = ofdmdemod(in, nfft, cplen, symoffset)`).
///
/// `symoffset` (default = `cplen`) drops the cyclic prefix.
///
/// @param in         Time-domain OFDM signal.
/// @param nfft       FFT length.
/// @param cplen      Cyclic prefix length.
/// @param symoffset  Sample offset within each symbol to start the
///                   FFT window.
/// @param mr         Memory resource (nullptr → process default).
/// @return           `nfft`×Nsymbols complex matrix of subcarriers.
/// @see ofdmmod
Value ofdmdemod(const Value &in, int nfft, int cplen, int symoffset,
                std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
