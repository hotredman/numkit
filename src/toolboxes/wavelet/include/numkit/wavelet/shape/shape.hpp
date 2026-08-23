/// @file shape.hpp
/// @ingroup group_wavelet
// toolboxes/wavelet/include/numkit/wavelet/shape/shape.hpp
//
// Continuous wavelet shapes — analytical functions sampled on a grid.
// Each returns (psi, x) where x is the time grid linspace(LB, UB, N)
// and psi is the wavelet sampled at those points.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <tuple>

namespace numkit::wavelet {

/// @addtogroup group_wavelet
/// @{


/// Mexican-hat wavelet sampled on a grid (`[psi, x] = mexihat(LB, UB, N)`).
///
/// @f$ \psi(t) = \frac{2}{\sqrt{3}\,\pi^{1/4}}\,(1 - t^2)\,e^{-t^2/2} @f$.
///
/// @param lb  Left endpoint of the grid.
/// @param ub  Right endpoint.
/// @param N   Number of samples (≥ 0).
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(psi, x)`; bind via `auto [psi, x] = mexihat(lb, ub, N);`.
///
/// @see morlet, meyeraux
std::tuple<Value, Value>
mexihat(double lb, double ub, size_t N,
        std::pmr::memory_resource *mr = nullptr);

/// Real Morlet wavelet (`[psi, x] = morlet(LB, UB, N)`).
///
/// @f$ \psi(t) = e^{-t^2/2}\,\cos(5t) @f$ (centre frequency 5).
///
/// @param lb,ub  Grid endpoints.
/// @param N      Number of samples.
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(psi, x)`.
///
/// @see mexihat, cmorwavf
std::tuple<Value, Value>
morlet(double lb, double ub, size_t N,
       std::pmr::memory_resource *mr = nullptr);

/// Meyer auxiliary function (`y = meyeraux(x)`).
///
/// @f$ y(x) = 35x^4 - 84x^5 + 70x^6 - 20x^7 @f$. The function is
/// defined on @f$ [0, 1] @f$ where it interpolates from 0 to 1, but
/// the polynomial is applied without clipping.
///
/// @param x   Element-wise input.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `y` of the same shape as `x`.
Value meyeraux(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Shannon wavelet (`[psi, x] = shanwavf(LB, UB, N, fb, fc)`).
///
/// Complex-valued:
/// @f$ \psi(t) = \sqrt{f_b}\,\text{sinc}(f_b t)\,e^{2\pi i f_c t} @f$.
///
/// @param lb,ub  Grid endpoints.
/// @param N      Number of samples.
/// @param fb     Bandwidth parameter (> 0).
/// @param fc     Centre frequency (> 0).
/// @param mr     Memory resource (nullptr → process default).
/// @return       `(psi, x)`; `psi` is COMPLEX.
///
/// @see cmorwavf, fbspwavf
std::tuple<Value, Value>
shanwavf(double lb, double ub, size_t N, double fb, double fc,
         std::pmr::memory_resource *mr = nullptr);

/// Complex Morlet wavelet (`[psi, x] = cmorwavf(LB, UB, N, fb, fc)`).
///
/// @f$ \psi(t) = \frac{1}{\sqrt{\pi f_b}}\,e^{2\pi i f_c t}\,e^{-t^2/f_b} @f$.
///
/// @see morlet, shanwavf
std::tuple<Value, Value>
cmorwavf(double lb, double ub, size_t N, double fb, double fc,
         std::pmr::memory_resource *mr = nullptr);

/// Frequency B-spline wavelet (`[psi, x] = fbspwavf(LB, UB, N, m, fb, fc)`).
///
/// @f$ \psi(t) = \sqrt{f_b}\,\text{sinc}^m(f_b t/m)\,e^{2\pi i f_c t},\ m \in \mathbb{N}_{+} @f$.
///
/// @param m      Spline order (positive integer).
/// @param fb,fc  Bandwidth and centre frequency.
///
/// @see cmorwavf, shanwavf
std::tuple<Value, Value>
fbspwavf(double lb, double ub, size_t N, int m, double fb, double fc,
         std::pmr::memory_resource *mr = nullptr);

/// Real Gaussian wavelet of order p (`[psi, x] = gauswavf(LB, UB, N, p)`).
///
/// @f$ \psi_p(t) @f$ is a normalised p-th derivative of @f$ e^{-t^2} @f$
/// (default p = 1). The L² norm is analytical.
///
/// @param p   Derivative order ≥ 1.
///
/// @see cgauwavf
std::tuple<Value, Value>
gauswavf(double lb, double ub, size_t N, int p,
         std::pmr::memory_resource *mr = nullptr);

/// Complex Gaussian wavelet of order p (`[psi, x] = cgauwavf(LB, UB, N, p)`).
///
/// Like @ref gauswavf but complex-valued and normalised by trapezoidal
/// quadrature on the requested grid (a grid-dependent
/// normalisation).
///
/// @see gauswavf
std::tuple<Value, Value>
cgauwavf(double lb, double ub, size_t N, int p,
         std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::wavelet
