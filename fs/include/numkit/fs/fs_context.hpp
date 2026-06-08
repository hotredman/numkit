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

private:
    std::unordered_map<std::string, std::unique_ptr<VirtualFS>> virtualFs_;
    struct ScriptOriginEntry
    {
        std::string fsName;
        std::string scriptDir;
    };
    std::vector<ScriptOriginEntry> scriptOriginStack_;
    std::string cwd_;
};

} // namespace numkit
