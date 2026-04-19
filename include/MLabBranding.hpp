// include/MLabBranding.hpp
#pragma once

#include <cstdlib>
#include <string>

#ifdef _WIN32
#  include <stdlib.h>
#endif

namespace mlab {

// Project-name prefix used for user-facing identifiers that must change
// if the project is ever renamed — currently only environment-variable
// names like MLAB_FS, MLAB_CWD. C++ namespaces, CMake targets, file
// names, and docs are handled separately during the rename itself.
inline constexpr const char *kEnvPrefix = "MLAB";

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

} // namespace mlab
