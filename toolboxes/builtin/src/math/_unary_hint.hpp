#pragma once
// _unary_hint.hpp moved to math/src/ as the Phase 3-A step C1 SIMD prerequisite
// (the relocated SIMD math areas reference it via "../_unary_hint.hpp"; it must
// live below them). This forwarding stub keeps the still-in-builtin SIMD compute
// files (exp_log / arithmetic, which include "../_unary_hint.hpp") working until
// they relocate to the math library; removed in the C4 cleanup.
#include "../../../../math/src/_unary_hint.hpp"
