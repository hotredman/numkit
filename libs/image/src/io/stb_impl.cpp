// libs/image/src/io/stb_impl.cpp
//
// Single TU that emits the implementations for stb_image and
// stb_image_write. The headers themselves live under
// third_party/stb/ — vendored verbatim from
// https://github.com/nothings/stb (public domain / MIT).
//
// Other TUs (io.cpp, …) just include the headers without the
// _IMPLEMENTATION macro to pick up the function declarations only.

// Trim some MSVC warnings stb_image is known to trip:
#if defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS
#  pragma warning(push)
#  pragma warning(disable : 4244 4456 4457 4505 4996)
#endif

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
