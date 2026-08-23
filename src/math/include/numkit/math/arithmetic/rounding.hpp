// src/math/include/numkit/math/arithmetic/rounding.hpp
//
// Forwarding header during modular migration to numkit::builtin.
#pragma once

#include <numkit/builtin/elfun.hpp>

namespace numkit::math {

using ::numkit::builtin::round;
using ::numkit::builtin::roundN;
using ::numkit::builtin::floor;
using ::numkit::builtin::ceil;
using ::numkit::builtin::fix;
using ::numkit::builtin::subplus;
using ::numkit::builtin::sign;
using ::numkit::builtin::abs;

} // namespace numkit::math
