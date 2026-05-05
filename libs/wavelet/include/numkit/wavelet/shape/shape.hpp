// libs/wavelet/include/numkit/wavelet/shape/shape.hpp
//
// Continuous wavelet shapes — analytical functions sampled on a grid.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <tuple>

namespace numkit::wavelet {

/// mexihat(LB, UB, N) — Mexican-hat wavelet sampled at N points over
/// [LB, UB]. Formula: ψ(t) = (2/√3) · π^(-1/4) · (1 - t²) · exp(-t²/2).
std::tuple<Value, Value>
mexihat(std::pmr::memory_resource *mr, double lb, double ub, size_t N);

/// morlet(LB, UB, N) — real Morlet wavelet sampled at N points over
/// [LB, UB]. Formula: ψ(t) = exp(-t²/2) · cos(5t).
std::tuple<Value, Value>
morlet(std::pmr::memory_resource *mr, double lb, double ub, size_t N);

/// meyeraux(x) — auxiliary function for the Meyer wavelet.
/// y(x) = 35·x⁴ - 84·x⁵ + 70·x⁶ - 20·x⁷, defined for x ∈ [0, 1] but
/// applied element-wise without clipping (MATLAB extrapolates the
/// polynomial verbatim).
Value meyeraux(std::pmr::memory_resource *mr, const Value &x);

/// shanwavf(LB, UB, N, fb, fc) — Shannon wavelet (complex).
///   ψ(t) = √fb · sinc(fb·t) · exp(2π·i·fc·t)
std::tuple<Value, Value>
shanwavf(std::pmr::memory_resource *mr, double lb, double ub, size_t N,
         double fb, double fc);

/// cmorwavf(LB, UB, N, fb, fc) — complex Morlet wavelet.
///   ψ(t) = (1/√(π·fb)) · exp(2π·i·fc·t) · exp(−t²/fb)
std::tuple<Value, Value>
cmorwavf(std::pmr::memory_resource *mr, double lb, double ub, size_t N,
         double fb, double fc);

/// fbspwavf(LB, UB, N, m, fb, fc) — frequency B-spline wavelet.
///   ψ(t) = √fb · (sinc(fb·t/m))^m · exp(2π·i·fc·t),  m ∈ ℕ⁺
std::tuple<Value, Value>
fbspwavf(std::pmr::memory_resource *mr, double lb, double ub, size_t N,
         int m, double fb, double fc);

} // namespace numkit::wavelet
