// libs/comm/include/numkit/comm/modulation/fsk_ofdm.hpp
//
// Frequency-shift keying and OFDM modulation.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::comm {

/// fskmod(x, M, freq_sep, nsamp, fs[, phase_continuity, symbol_order])
///   x:               column or row of integers in 0..M-1
///   M:               number of frequency tones
///   freq_sep:        spacing between tones in Hz
///   nsamp:           samples per symbol
///   fs:              sampling rate in Hz
///   phase_continuity:"cont" (default) or "discont"
///   symbol_order:    "gray" (default) or "bin"
/// Returns complex-valued baseband waveform of length numel(x)·nsamp.
Value fskmod(std::pmr::memory_resource *mr, const Value &x, int M,
             double freq_sep, int nsamp, double fs,
             const std::string &phase_continuity,
             const std::string &symbol_order);

/// fskdemod(y, M, freq_sep, nsamp, fs[, symbol_order])
/// Per-symbol energy detection across the M candidate tones; returns
/// integer symbols of length numel(y) / nsamp.
Value fskdemod(std::pmr::memory_resource *mr, const Value &y, int M,
               double freq_sep, int nsamp, double fs,
               const std::string &symbol_order);

/// ofdmmod(in, nfft, cplen) — basic OFDM modulator.
///   in:    nfft × Nsymbols complex matrix of subcarrier values
///   nfft:  IFFT length
///   cplen: cyclic prefix length
/// Returns column vector of length (nfft + cplen)·Nsymbols.
Value ofdmmod(std::pmr::memory_resource *mr, const Value &in,
              int nfft, int cplen);

/// ofdmdemod(in, nfft, cplen[, symoffset]) — basic OFDM demodulator.
/// `symoffset` (default = cplen) drops the CP.
/// Returns nfft × Nsymbols complex matrix.
Value ofdmdemod(std::pmr::memory_resource *mr, const Value &in,
                int nfft, int cplen, int symoffset);

} // namespace numkit::comm
