// fs/include/numkit/fs/fs_context.hpp
//
// FsContext — the engine-agnostic filesystem session: the VirtualFS registry,
// the script-origin stack, the current working directory, and the path
// resolver that ties them together.
//
// Lives in fs/ (L0) so path resolution is reusable WITHOUT the Engine — the
// core-free C++ API and the toolboxes resolve user paths through an FsContext
// reference rather than through Engine. The layering guard pins fs:{fs}, so
// this is STL-only: resolvePath() therefore throws std::runtime_error rather
// than numkit::Error (which lives in value/). The Engine owns an FsContext,
// forwards its filesystem API to it, and rethrows resolvePath's
// std::runtime_error as numkit::Error so the MATLAB-visible error type is
// unchanged for in-engine callers.
#pragma once

#include <numkit/fs/branding.hpp>   // envGet / envVarName (NUMKIT_FS / NUMKIT_CWD)
#include <numkit/fs/vfs.hpp>        // VirtualFS

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace numkit {

class FsContext
{
public:
    // ── Virtual filesystem registry ───────────────────────────
    //
    // Registry of named filesystems ("native", "temporary", "local", …).
    // A native FS is pre-registered by the Engine constructor; the IDE
    // installs "temporary" / "local" via CallbackFS hooks at startup.
    void registerVirtualFS(std::unique_ptr<VirtualFS> fs);
    VirtualFS *findVirtualFS(const std::string &name) const;

    // ── Script-origin stack ───────────────────────────────────
    //
    // Each frame carries (fsName, scriptDir):
    //   * fsName     — VFS that the script came from. Used by resolvePath
    //                  as the implicit FS for relative paths inside the
    //                  script.
    //   * scriptDir  — directory containing the script. Used by the m-file
    //                  resolver as the implicit search dir, so sibling .m
    //                  files resolve without addpath.
    // The 1-arg overload pushes an empty scriptDir — kept for callers that
    // only know the FS (IDE running an unsaved buffer, tests).
    void pushScriptOrigin(const std::string &fsName);
    void pushScriptOrigin(const std::string &fsName, const std::string &scriptDir);
    void popScriptOrigin();
    const std::string *currentScriptOrigin() const;   // fsName, may be null
    const std::string *currentScriptDir() const;      // dir, may be null/empty

    // ── Current working directory ─────────────────────────────
    //
    //   * `cd` / setCwd write here — canonical when non-empty.
    //   * pwd reports this (with backend-cwd fallback when empty).
    //   * resolvePath consults this first, then NUMKIT_CWD env var.
    // The two-tier model lets hosts seed cwd via `setenv NUMKIT_CWD`
    // without having to call setCwd, while still letting in-engine `cd`
    // calls take precedence once they happen.
    const std::string &cwd() const { return cwd_; }
    void setCwd(const std::string &p) { cwd_ = p; }

    // ── Path resolution ───────────────────────────────────────
    //
    // resolvePath() picks the right backend by this order of precedence:
    //   1. explicit prefix in the path ("temporary:/foo", "local:/foo",
    //      "native:/foo") — wins over everything
    //   2. env var NUMKIT_FS, if it names a registered backend
    //   3. the current script's origin (set by the IDE before eval)
    //   4. "native" if registered
    // Relative paths are joined with `cwd_` when set; otherwise with
    // NUMKIT_CWD as a host-runtime fallback. cwd_ takes precedence — once
    // `cd` / setCwd writes to it, the env var is ignored.
    struct ResolvedPath
    {
        VirtualFS *fs;
        std::string path;
    };
    // Throws std::runtime_error when the requested filesystem is unknown or
    // unavailable. (The Engine rethrows these as numkit::Error.)
    ResolvedPath resolvePath(const std::string &userPath) const;

    // ── MATLAB-style file descriptor table ────────────────────
    //
    // fopen / fclose / fprintf(fid, …) machinery. File IDs 0, 1, 2 are
    // reserved for stdin/stdout/stderr (the fprintf builtin routes 1/2 to the
    // engine output sink); user files get 3, 4, … from nextFid_. For 'r' the
    // file is read into `buffer` on open and `cursor` advances as reading
    // builtins consume it; for 'w' `buffer` accumulates fprintf output and is
    // flushed via VirtualFS::writeFile on fclose; 'a' is 'w' seeded with the
    // existing content. All writes are buffered in memory until close — the
    // sync-mirror VirtualFS can't do partial writes efficiently, and MATLAB
    // only guarantees visibility on close. Lives here (fs/, L0) so the
    // stateful fopen-family is Engine-free; the Engine forwards to it.
    struct OpenFile
    {
        std::string path;
        std::string mode;
        VirtualFS *fs = nullptr;
        std::string buffer;
        std::size_t cursor = 0;
        // forRead and forWrite can BOTH be true ('r+'/'w+'/'a+'). appendOnly
        // snaps the cursor to end-of-buffer before each write ('a'/'a+').
        bool forRead = false;
        bool forWrite = false;
        bool appendOnly = false;
        // Last soft-failure text for ferror(fid); cleared by ferror(fid,'clear').
        std::string lastError;
    };

    int openFile(const std::string &userPath, const std::string &mode);
    bool closeFile(int fid);
    void closeAllFiles();
    OpenFile *findFile(int fid);
    // Sorted list of user-opened fids (>= 3). Powers `fopen('all')`.
    std::vector<int> openFileIds() const;
    // Error text from the most recent openFile() call ([fid, errmsg] = fopen).
    const std::string &lastFopenError() const { return lastFopenError_; }

private:
    std::unordered_map<std::string, std::unique_ptr<VirtualFS>> virtualFs_;
    struct ScriptOriginEntry
    {
        std::string fsName;
        std::string scriptDir;
    };
    std::vector<ScriptOriginEntry> scriptOriginStack_;
    std::string cwd_;

    // open-file table (see OpenFile above) — the fopen-family state, moved
    // out of the Engine so the stateful file builtins are Engine-free.
    std::unordered_map<int, OpenFile> openFiles_;
    int nextFid_ = 3;
    std::string lastFopenError_;
};

} // namespace numkit
