// toolboxes/io/src/library.cpp
//
// Registration hub for the Data Import and Export library.
// Namespace layout (namespace_design.md §5, §9.4):
//   file_io/   → io.file_io.<fn>   (fopen, fclose, fread, fwrite, ...)
//   text/      → io.text.<fn>      (csvread, csvwrite, ...)
//   paths/     → io.paths.<fn>     (filesep, fullfile, fileparts, ...)
// (save / load moved to runtime — they are workspace runtime,
//  not data import/export, and register bare via registerWorkspaceRuntime.)
// Each function is also aliased into `compat.<fn>` so MATLAB-style
// scripts call them flat via the bare-name resolver.

#include <numkit/io/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

namespace numkit::io::detail {
// text/csv.cpp
void csvread_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void csvwrite_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// file_io/fileio.cpp
void fopen_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fclose_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void fgetl_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fgets_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void feof_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void ferror_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void ftell_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fseek_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void frewind_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void fread_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fwrite_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// text/extras.cpp (C1 — modern text helpers)
void fileread_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void readlines_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void writelines_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void readmatrix_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void writematrix_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void type_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);

// paths/paths.cpp (C2 — file-name construction)
void filesep_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void fullfile_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void fileparts_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void tempdir_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void tempname_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void genpath_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::io::detail

namespace numkit {

void IoLibrary::install(Engine &engine)
{
    // io is a MATLAB-mirror library — every function dual-registered
    // (io.<sub>.<name> + compat.<name>).
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("io.") + sub, name, fn);
        // compat.* registration removed — bare-name resolver replaces it
    };

    reg("file_io", "fopen",   &io::detail::fopen_reg);
    reg("file_io", "fclose",  &io::detail::fclose_reg);
    reg("file_io", "fgetl",   &io::detail::fgetl_reg);
    reg("file_io", "fgets",   &io::detail::fgets_reg);
    reg("file_io", "feof",    &io::detail::feof_reg);
    reg("file_io", "ferror",  &io::detail::ferror_reg);
    reg("file_io", "ftell",   &io::detail::ftell_reg);
    reg("file_io", "fseek",   &io::detail::fseek_reg);
    reg("file_io", "frewind", &io::detail::frewind_reg);
    reg("file_io", "fread",   &io::detail::fread_reg);
    reg("file_io", "fwrite",  &io::detail::fwrite_reg);

    reg("text", "csvread",     &io::detail::csvread_reg);
    reg("text", "csvwrite",    &io::detail::csvwrite_reg);
    reg("text", "fileread",    &io::detail::fileread_reg);
    reg("text", "readlines",   &io::detail::readlines_reg);
    reg("text", "writelines",  &io::detail::writelines_reg);
    reg("text", "readmatrix",  &io::detail::readmatrix_reg);
    reg("text", "writematrix", &io::detail::writematrix_reg);
    reg("text", "type",        &io::detail::type_reg);

    reg("paths", "filesep",   &io::detail::filesep_reg);
    reg("paths", "fullfile",  &io::detail::fullfile_reg);
    reg("paths", "fileparts", &io::detail::fileparts_reg);
    reg("paths", "tempdir",   &io::detail::tempdir_reg);
    reg("paths", "tempname",  &io::detail::tempname_reg);
    reg("paths", "genpath",   &io::detail::genpath_reg);
}

} // namespace numkit
