// include/MBranding.hpp
#pragma once

#include <cstdlib>
#include <string>

#ifdef _WIN32
#  include <stdlib.h>
#endif

namespace numkit::m {

// Project-name prefix for user-facing identifiers that must change if
// the project is ever renamed. Intentionally narrow — covers only the
// environment-variable namespace (NUMKIT_M_FS, NUMKIT_M_CWD, …) that
// end-user scripts `setenv`/`getenv` touch and would break on rename.
//
// NOT covered here (rename these manually alongside the prefix update):
//   • C++ namespace `numkit::m`, target `numkit::m`
//   • File names `M*.hpp` / `M*.cpp`
//   • Error-identifier strings such as "m:assert" (defined across
//     libs/builtin/src/MStd*.cpp and libs/dsp/src/MDsp*.cpp)
//   • Documentation, README, CMake project name
inline constexpr const char *kEnvPrefix = "NUMKIT_M";

inline std::string envVarName(const char *suffix)
{
    return std::string(kEnvPrefix) + "_" + suffix;
}

// Cross-platform environment-variable read: returns "" when unset.
// On MSVC std::getenv is deprecated; use _dupenv_s for the same behaviour
// without the C4996 warning.
inline std::string envGet(const char *name)
{
#ifdef _WIN32
    char *buf = nullptr;
    size_t len = 0;
    _dupenv_s(&buf, &len, name);
    std::string s = buf ? buf : "";
    std::free(buf);
    return s;
#else
    const char *v = std::getenv(name);
    return v ? v : "";
#endif
}

} // namespace numkit::m
