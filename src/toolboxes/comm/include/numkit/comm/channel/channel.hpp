/// @file channel.hpp
/// @ingroup group_comm
// toolboxes/comm/include/numkit/comm/channel/channel.hpp
//
// AWGN / WGN / BSC channel models, Q-functions, analytical BER, and
// SNR conversions.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <string>
#include <tuple>

namespace numkit { namespace ops { class RngContext; } }

namespace numkit::comm {

/// @brief Add white Gaussian noise so the resulting SNR is `snr_db` dB
/// (`y = awgn(x, snr_db, sigpower_db)`).
///
/// Noise is complex when `x` is complex, real otherwise. The signal
/// power may be specified or estimated.
///
/// @param x            Input signal.
/// @param snr_db       Target SNR in dB.
/// @param sigpower_db  Signal power in dBW, or `-1` ⇒ "measured"
///                     (estimate from `x`).
/// @param mr           Memory resource (nullptr → process default).
/// @return             Noisy signal of the same shape as `x`.
/// @see wgn, bsc
Value awgn(::numkit::ops::RngContext &rng, const Value &x, double snr_db, double sigpower_db,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Generate an `m`×`n` matrix of white Gaussian noise samples
/// (`y = wgn(m, n, p, type, complex)`).
///
/// @param m            Row count.
/// @param n            Column count.
/// @param p            Power, interpreted per `type`.
/// @param type         `"dBW"` (default), `"dBm"`, or `"linear"`.
/// @param complex_out  If true, samples are circular complex Gaussian.
/// @param mr           Memory resource (nullptr → process default).
/// @return             `m`×`n` matrix of noise samples.
/// @see awgn
Value wgn(::numkit::ops::RngContext &rng, int m, int n, double p, const std::string &type,
          bool complex_out,
          std::pmr::memory_resource *mr = nullptr);

/// @brief Binary symmetric channel — flip each bit with probability
/// `p` (`y = bsc(x, p)`).
///
/// @param x   Input bit array (logical or 0/1 numeric).
/// @param p   Bit-flip probability in [0, 1].
/// @param mr  Memory resource (nullptr → process default).
/// @return    Output array of the same shape as `x`.
/// @see awgn
Value bsc(::numkit::ops::RngContext &rng, const Value &x, double p,
          std::pmr::memory_resource *mr = nullptr);

/// @brief Gaussian Q-function: `Q(x) = 0.5·erfc(x/√2)`.
///
/// @param x   Input array (any shape, DOUBLE).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Element-wise `Q(x)`.
/// @see qfuncinv
Value qfunc(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse Q-function: `Q⁻¹(p) = √2·erfc⁻¹(2p)`.
///
/// @param p   Input array (any shape, DOUBLE) with values in (0, 1).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Element-wise `Q⁻¹(p)`.
/// @see qfunc
Value qfuncinv(const Value &p,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Generalised Marcum Q-function
/// (`q = marcumq(a, b, m)`).
///
/// Computed via the modified-Bessel-series. `m = 1` is the standard
/// Marcum Q.
///
/// @param a   Non-centrality parameter (array).
/// @param b   Argument (array).
/// @param m   Order (positive integer, default 1).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Element-wise Marcum Q values, broadcasting `a` & `b`.
Value marcumq(const Value &a, const Value &b, int m,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Analytical bit-error rate over AWGN
/// (`ber = berawgn(EbNo_dB, mod, M)`).
///
/// @param EbNo_dB  Eb/No values in dB (array).
/// @param mod      `"psk"`, `"dpsk"`, `"qam"`, `"pam"`, or `"fsk"`.
/// @param M        Modulation order.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Element-wise BER.
/// @throws Error   Unknown modulation string or invalid `M`.
Value berawgn(const Value &EbNo_dB, const std::string &mod, int M,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Equivalent noise bandwidth of an LTI filter
/// (`bw = noisebw(num, den, Nsamp, fs)`).
///
/// @param num    Numerator coefficients.
/// @param den    Denominator coefficients.
/// @param Nsamp  Number of impulse-response samples used.
/// @param fs     Sample rate in Hz.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Scalar noise bandwidth in Hz.
Value noisebw(const Value &num, const Value &den, int Nsamp,
              double fs,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Clopper–Pearson exact binomial confidence interval for a
/// measured BER (`[ber, ci] = berconfint(numErrs, numBits, level)`).
///
/// @param numErrs  Observed error count.
/// @param numBits  Total bit count.
/// @param level    Confidence level in (0, 1), e.g. 0.95.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Tuple `(ber, ci)` where `ber = numErrs / numBits`
///                 and `ci = [lo, hi]`.
std::tuple<Value, Value>
berconfint(double numErrs, double numBits, double level,
           std::pmr::memory_resource *mr = nullptr);

/// @brief Convert between Eb/No, Es/No, and SNR
/// (`snr_out = convertSNR(snr_in, in_type, out_type, k)`).
///
/// Currently supports k-only conversion via
/// `Es/No = Eb/No + 10·log10(k)`. Sample-rate conversion requires
/// extra arguments and is deferred.
///
/// @param snr_in           Input SNR in dB (array).
/// @param in_type          `"ebno"`, `"esno"`, or `"snr"`.
/// @param out_type         Destination type (same set).
/// @param bits_per_symbol  `k`, bits per symbol.
/// @param mr               Memory resource (nullptr → process default).
/// @return                 Converted SNR (dB) of the same shape as
///                         `snr_in`.
Value convertSNR(const Value &snr_in,
                 const std::string &in_type,
                 const std::string &out_type,
                 int bits_per_symbol,
                 std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::comm
