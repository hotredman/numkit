// src/math/include/numkit/math/exp_log/exponents.hpp
//
// Forwarding header during modular migration to numkit::builtin.
#pragma once

#include <numkit/builtin/elfun.hpp>

namespace numkit::math {

using ::numkit::builtin::exp;
using ::numkit::builtin::expm1;
using ::numkit::builtin::log;
using ::numkit::builtin::log10;
using ::numkit::builtin::log2;
using ::numkit::builtin::log1p;
using ::numkit::builtin::pow2;
using ::numkit::builtin::sqrt;
using ::numkit::builtin::realsqrt;
using ::numkit::builtin::reallog;
using ::numkit::builtin::cbrt;
using ::numkit::builtin::nthroot;
using ::numkit::builtin::hypot;
using ::numkit::builtin::pow;
using ::numkit::builtin::nextpow2;

} // namespace numkit::math
