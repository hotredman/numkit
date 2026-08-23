// src/math/include/numkit/math/trig/trigonometry.hpp
//
// Forwarding header during modular migration to numkit::builtin.
#pragma once

#include <numkit/builtin/elfun.hpp>

namespace numkit::math {

using ::numkit::builtin::sin;
using ::numkit::builtin::cos;
using ::numkit::builtin::tan;
using ::numkit::builtin::asin;
using ::numkit::builtin::acos;
using ::numkit::builtin::atan;
using ::numkit::builtin::atan2;
using ::numkit::builtin::sec;
using ::numkit::builtin::csc;
using ::numkit::builtin::cot;
using ::numkit::builtin::asec;
using ::numkit::builtin::acsc;
using ::numkit::builtin::acot;

using ::numkit::builtin::sind;
using ::numkit::builtin::cosd;
using ::numkit::builtin::tand;
using ::numkit::builtin::asind;
using ::numkit::builtin::acosd;
using ::numkit::builtin::atand;
using ::numkit::builtin::atan2d;
using ::numkit::builtin::secd;
using ::numkit::builtin::cscd;
using ::numkit::builtin::cotd;
using ::numkit::builtin::asecd;
using ::numkit::builtin::acscd;
using ::numkit::builtin::acotd;

using ::numkit::builtin::sinh;
using ::numkit::builtin::cosh;
using ::numkit::builtin::tanh;
using ::numkit::builtin::asinh;
using ::numkit::builtin::acosh;
using ::numkit::builtin::atanh;
using ::numkit::builtin::sech;
using ::numkit::builtin::csch;
using ::numkit::builtin::coth;
using ::numkit::builtin::asech;
using ::numkit::builtin::acsch;
using ::numkit::builtin::acoth;

using ::numkit::builtin::sinpi;
using ::numkit::builtin::cospi;
using ::numkit::builtin::deg2rad;
using ::numkit::builtin::rad2deg;
using ::numkit::builtin::cart2pol;
using ::numkit::builtin::pol2cart;
using ::numkit::builtin::cart2sph;
using ::numkit::builtin::sph2cart;

} // namespace numkit::math
