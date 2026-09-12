// toolboxes/builtin/src/version_string.cpp
//
// Tiny isolated TU that owns the build-timestamp string. Lives in
// its own file (not inline in library.cpp's gigantic registration
// block) so the CMake `nk_build_info` regen-on-every-build
// target's OBJECT_DEPENDS only forces THIS TU to recompile, not the
// full library.cpp — saves a few seconds per WASM iteration while
// still giving the `version` builtin a fresh ISO-8601 timestamp.

#include <nk/core/build_info.hpp>

namespace nk {

/** Build-time wall-clock timestamp as "YYYY-MM-DD HH:MM:SS" UTC.
 *  Populated by cmake/gen_build_info.cmake on every build. */
const char *buildTimestamp()
{
    return NK_BUILD_TIMESTAMP;
}

} // namespace nk
