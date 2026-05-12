// libs/signal/include/numkit/signal/filter_design/iir_designs.hpp
//
// Top-level IIR filter designs that compose the analog prototypes
// (cheb1ap / cheb2ap / besselap / ellipap) with the LP→{HP,BP,BS}
// frequency transforms, zp2tf, and bilinear.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <string_view>
#include <tuple>

namespace numkit::signal {

/// Filter response type for IIR designs.
enum class FilterType {
    Lowpass,   ///< Pass below the cutoff, stop above.
    Highpass,  ///< Pass above the cutoff, stop below.
    Bandpass,  ///< Pass between two cutoffs.
    Bandstop   ///< Stop between two cutoffs, pass elsewhere.
};

/// Chebyshev type-I IIR filter design.
///
/// Returns digital (or analog) filter coefficients (b, a) of order N
/// with equiripple passband and monotonic stopband. The order-N filter
/// has Rp dB peak-to-peak ripple in the passband.
///
/// @param N       Filter order, ≥ 1. For bandpass / bandstop the output
///                order is 2N (each transform doubles).
/// @param Rp      Passband ripple in dB, > 0 (typical 0.1–3 dB).
/// @param Wn      Cutoff frequency. Digital: normalised to Nyquist
///                (0 < Wn < 1). Analog: rad/s. Scalar for LP/HP, 2-vector
///                `[lo hi]` for BP/BS.
/// @param ftype   Response type (default Lowpass for scalar Wn,
///                Bandpass for 2-vector Wn).
/// @param analog  `true` → analog-domain design (skip bilinear).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(b, a)` — numerator and denominator coefficients.
///
/// @code
/// auto [b, a] = cheby1(4, 0.5, 0.3);                     // 0.5 dB ripple LP
/// auto [b, a] = cheby1(4, 1.0, {0.3, 0.6}, FilterType::Bandpass);
/// @endcode
///
/// @see cheby2, ellip, butter
std::tuple<Value, Value>
cheby1(int                          N,
       double                       Rp,
       const Value &                Wn,
       FilterType                   ftype  = FilterType::Lowpass,
       bool                         analog = false,
       std::pmr::memory_resource *  mr     = nullptr);

/// Chebyshev type-II (inverse Chebyshev) IIR filter design.
///
/// Equiripple stopband at Rs dB attenuation, monotonic passband.
///
/// @param N       Filter order.
/// @param Rs      Stopband attenuation in dB, > 0 (typical 40–60 dB).
/// @param Wn      Cutoff frequency (see cheby1).
/// @param ftype   Response type.
/// @param analog  Analog-domain flag.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(b, a)`.
///
/// @see cheby1, ellip
std::tuple<Value, Value>
cheby2(int                          N,
       double                       Rs,
       const Value &                Wn,
       FilterType                   ftype  = FilterType::Lowpass,
       bool                         analog = false,
       std::pmr::memory_resource *  mr     = nullptr);

/// Bessel-Thompson IIR filter design.
///
/// Maximally flat group delay in the passband. MATLAB's `besself` is
/// analog-only by default; here `analog = false` enables a digital
/// design via the bilinear transform (the resulting digital filter
/// approximates the analog group-delay characteristic).
///
/// @param N       Filter order.
/// @param Wn      Cutoff frequency (digital: normalised; analog: rad/s).
/// @param ftype   Response type.
/// @param analog  Analog-domain flag.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(b, a)`.
std::tuple<Value, Value>
besself(int                          N,
        const Value &                Wn,
        FilterType                   ftype  = FilterType::Lowpass,
        bool                         analog = false,
        std::pmr::memory_resource *  mr     = nullptr);

/// Elliptic (Cauer) IIR filter design.
///
/// Equiripple in both passband (Rp dB) and stopband (Rs dB). The most
/// frequency-selective IIR design for a given order.
///
/// @param N       Filter order.
/// @param Rp      Passband ripple in dB, > 0.
/// @param Rs      Stopband attenuation in dB, > 0, > Rp.
/// @param Wn      Cutoff frequency (see cheby1).
/// @param ftype   Response type.
/// @param analog  Analog-domain flag.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(b, a)`.
std::tuple<Value, Value>
ellip(int                          N,
      double                       Rp,
      double                       Rs,
      const Value &                Wn,
      FilterType                   ftype  = FilterType::Lowpass,
      bool                         analog = false,
      std::pmr::memory_resource *  mr     = nullptr);

// ─────────────────────────────────────────────────────────────────────
// Order estimators — pick the minimum order to meet given specs.
// ─────────────────────────────────────────────────────────────────────

/// Butterworth filter order estimator.
///
/// Returns the minimum order `N` and natural frequency `Wn` such that a
/// Butterworth filter `butter(N, Wn, …)` has passband ripple ≤ `Rp` dB
/// and stopband attenuation ≥ `Rs` dB.
///
/// @param Wp      Passband edge(s). Scalar for LP/HP; 2-vector for BP/BS.
///                Digital: normalised to Nyquist (0..1).
/// @param Ws      Stopband edge(s), same shape as `Wp`.
/// @param Rp      Maximum allowed passband ripple in dB, > 0.
/// @param Rs      Minimum required stopband attenuation in dB, > 0.
/// @param analog  Analog-domain flag.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Tuple `(N, Wn)` — order and cutoff(s) for `butter`.
///
/// @code
/// auto [N, Wn] = buttord(0.2, 0.3, 1.0, 30.0);   // LP: 1 dB pass / 30 dB stop
/// auto [b, a]  = butter(N, Wn);
/// @endcode
///
/// @see butter, cheb1ord, cheb2ord, ellipord
std::tuple<int, Value>
buttord(const Value &                Wp,
        const Value &                Ws,
        double                       Rp,
        double                       Rs,
        bool                         analog = false,
        std::pmr::memory_resource *  mr     = nullptr);

/// Chebyshev type-I order estimator. Returns `(N, Wn)` where `Wn` is
/// the passband edge usable directly with `cheby1`.
/// @copydoc buttord
std::tuple<int, Value>
cheb1ord(const Value &                Wp,
         const Value &                Ws,
         double                       Rp,
         double                       Rs,
         bool                         analog = false,
         std::pmr::memory_resource *  mr     = nullptr);

/// Chebyshev type-II order estimator. Returns `(N, Wn)` where `Wn` is
/// the stopband edge usable directly with `cheby2`.
/// @copydoc buttord
std::tuple<int, Value>
cheb2ord(const Value &                Wp,
         const Value &                Ws,
         double                       Rp,
         double                       Rs,
         bool                         analog = false,
         std::pmr::memory_resource *  mr     = nullptr);

/// Elliptic (Cauer) order estimator. Returns `(N, Wn)` for `ellip`.
///
/// @note  KNOWN GAP: bandstop case (Wp 2-vec with `Wp[0] > Ws[0]`) is
///        deferred. Lowpass / highpass / bandpass supported.
/// @copydoc buttord
std::tuple<int, Value>
ellipord(const Value &                Wp,
         const Value &                Ws,
         double                       Rp,
         double                       Rs,
         bool                         analog = false,
         std::pmr::memory_resource *  mr     = nullptr);

/// Parks-McClellan FIR order estimator.
///
/// Returns `(N, ff, aa, wts)` — order plus the band-edge, amplitude,
/// and weight vectors suitable for direct use with `firpm`.
///
/// @param F    Vector of band-edge frequencies in Hz (length `2·numel(A) - 2`).
/// @param A    Vector of binary amplitudes per band (0 = stopband, 1 = passband).
/// @param dev  Maximum allowed linear deviation per band.
/// @param fs   Sample rate. Default 2.0 → F is normalised to Nyquist.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Tuple `(N, ff, aa, wts)`.
///
/// @code
/// auto [N, ff, aa, wts] = firpmord({1500, 2000}, {1, 0}, {0.01, 0.05}, 8000);
/// auto [h, err] = firpm(N, ff, aa, wts);
/// @endcode
///
/// @see firpm
std::tuple<int, Value, Value, Value>
firpmord(const Value &                F,
         const Value &                A,
         const Value &                dev,
         double                       fs = 2.0,
         std::pmr::memory_resource *  mr = nullptr);

/// Kaiser-window FIR order estimator.
///
/// Returns `(N, Wn, beta, ftype)` such that
/// `fir1(N, Wn, ftype, kaiser(N+1, beta), 'noscale')` meets the design specs.
///
/// @param F    Transition-band edge frequencies in Hz.
/// @param A    Binary amplitudes per band (0 / 1).
/// @param dev  Maximum linear deviation per band.
/// @param fs   Sample rate (default 2 → normalised).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Tuple `(N, Wn, beta, ftype)`. `ftype` is one of
///             `"low"`, `"high"`, `"stop"`, `"bandpass"`, `"DC-0"`, `"DC-1"`.
///
/// @see fir1, kaiser
std::tuple<int, Value, double, std::string>
kaiserord(const Value &                F,
          const Value &                A,
          const Value &                dev,
          double                       fs = 2.0,
          std::pmr::memory_resource *  mr = nullptr);

} // namespace numkit::signal
