// libs/builtin/src/language/operators/la_solve.hpp
//
// Compatibility shim — the real declaration moved to a public header
// (libs/builtin/include/numkit/builtin/internal/la_solve.hpp) so that
// libs/linalg can include it without reaching into builtin's src/
// tree. Existing in-builtin code keeps the old include path through
// this shim.
//
// New code: include <numkit/builtin/internal/la_solve.hpp> directly.

#pragma once

#include <numkit/builtin/internal/la_solve.hpp>
