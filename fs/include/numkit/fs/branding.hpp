// fs/include/numkit/fs/branding.hpp
//
// Lives in fs/ (L0) because that is the lowest layer reachable by every
// consumer under the layering guard (value:{value}, fs:{fs}): FsContext (fs/)
// needs envGet for the NUMKIT_FS / NUMKIT_CWD path-resolution fallbacks, and
// core/ + toolboxes may include fs/. Header-only (inline/constexpr), no deps
// beyond <cstdlib>/<string>. The NUMKIT_FS/NUMKIT_CWD vars it serves are
// themselves filesystem config, so fs/ is a natural home.
#pragma once

#include <cstdlib>
#include <string>

#ifdef _WIN32
#  include <stdlib.h>
#endif

namespace numkit {

// Project-name prefix for user-facing identifiers that must change if
// the project is ever renamed. Intentionally narrow — covers only the
// environment-variable namespace (NUMKIT_FS, NUMKIT_CWD, …) that
// end-user scripts `setenv`/`getenv` touch and would break on rename.
//
// NOT covered here (rename these manually alongside the prefix update):
//   • C++ namespace `numkit`, target `numkit`
//   • Error-identifier strings such as "numkit:assert" (defined across
//     toolboxes/builtin/src and toolboxes/signal/src)
//   • Documentation, README, CMake project name
inline constexpr const char *kEnvPrefix = "NUMKIT";

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

} // namespace numkit
