// toolboxes/io/include/numkit/io/library.hpp
//
// Data import / export builtins. Houses:
//   * Low-level file I/O (fopen/fclose/fread/fwrite/fprintf/...)
//   * Text files (csvread, csvwrite, ...)
//   * Workspace save/load
//
// Functions are dual-registered: in their natural sub-namespace
// (io.file_io.* / io.text.* / io.workspace.*) AND aliased into compat.*
// so that `import compat.*` flattens them to short names.

#pragma once

#include <numkit/core/engine.hpp>

namespace numkit {

class IoLibrary
{
public:
    static void install(Engine &engine);
};

} // namespace numkit
