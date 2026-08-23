// src/math/include/numkit/math/complex/complex.hpp
//
// Forwarding header during modular migration to numkit::builtin.
#pragma once

#include <numkit/builtin/elfun.hpp>

namespace numkit::math {

using ::numkit::builtin::real;
using ::numkit::builtin::imag;
using ::numkit::builtin::conj;
using ::numkit::builtin::angle;
using ::numkit::builtin::abs;
using ::numkit::builtin::complex;
using ::numkit::builtin::sign;
using ::numkit::builtin::unwrap;
using ::numkit::builtin::isreal;

} // namespace numkit::math
