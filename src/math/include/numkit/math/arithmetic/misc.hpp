// src/math/include/numkit/math/arithmetic/misc.hpp
//
// Forwarding header during modular migration to numkit::builtin.
#pragma once

#include <numkit/builtin/elfun.hpp>

namespace numkit::math {

using ::numkit::builtin::deg2rad;
using ::numkit::builtin::rad2deg;
using ::numkit::builtin::wrapToPi;
using ::numkit::builtin::wrapTo2Pi;
using ::numkit::builtin::wrapTo180;
using ::numkit::builtin::wrapTo360;
using ::numkit::builtin::mod;
using ::numkit::builtin::rem;
using ::numkit::builtin::hypot;
using ::numkit::builtin::nthroot;

} // namespace numkit::math
