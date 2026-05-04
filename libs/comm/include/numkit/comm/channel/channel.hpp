// libs/comm/include/numkit/comm/channel/channel.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>

namespace numkit::comm {

/// awgn(signal, snr_db[, sigpower_db]) — add complex Gaussian noise so
/// that the resulting SNR is `snr_db` dB. `sigpower_db` (default
/// "measured", -1 sentinel) lets the caller specify the signal power
/// in dBW; otherwise we estimate it.
Value awgn(std::pmr::memory_resource *mr, const Value &x,
           double snr_db, double sigpower_db);

/// wgn(m, n, p[, type]) — m×n matrix of WGN with power `p` (interpreted
/// per `type`: "dBW" (default), "dBm", "linear").
Value wgn(std::pmr::memory_resource *mr, int m, int n,
          double p, const std::string &type, bool complex_out);

/// bsc(input, p) — binary symmetric channel. `input` is logical / 0-1
/// numeric; flips each bit with probability p. Returns same shape +
/// errors mask if requested by caller.
Value bsc(std::pmr::memory_resource *mr, const Value &x, double p);

/// qfunc(x) — Q(x) = 0.5·erfc(x/√2).
Value qfunc(std::pmr::memory_resource *mr, const Value &x);

/// qfuncinv(p) — inverse Q. Q⁻¹(p) = √2·erfc⁻¹(2p).
Value qfuncinv(std::pmr::memory_resource *mr, const Value &p);

/// marcumq(a, b[, m, niter]) — Marcum Q-function. m=1 is the standard
/// Marcum Q. Computed via the modified-Bessel-series.
Value marcumq(std::pmr::memory_resource *mr, const Value &a, const Value &b, int m);

/// berawgn(EbNo_dB, mod, M, [coding, dataenc])
///   mod: "psk", "dpsk", "qam", "pam", "fsk"
/// Returns analytical bit-error rate for the given modulation over AWGN.
Value berawgn(std::pmr::memory_resource *mr, const Value &EbNo_dB,
              const std::string &mod, int M);

/// noisebw(num, den, Nsamp, fs) — equivalent noise bandwidth of the
/// LTI filter (num/den) sampled at fs.
Value noisebw(std::pmr::memory_resource *mr, const Value &num, const Value &den,
              int Nsamp, double fs);

/// convertSNR(snr_in_dB, in_type, 'BitsPerSymbol', k[, options])
/// Convert between Eb/No, Es/No, and SNR. Without options just returns
/// snr_in_dB unchanged. Standard relations:
///   Es/No = Eb/No + 10·log10(k)
///   SNR   = Es/No + 10·log10(Rs/fs) (depends on samples-per-symbol)
/// We support the common k-only conversion; sample-rate conversion
/// requires extra args (defer).
Value convertSNR(std::pmr::memory_resource *mr, const Value &snr_in,
                 const std::string &in_type, const std::string &out_type,
                 int bits_per_symbol);

} // namespace numkit::comm
