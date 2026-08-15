// fs/src/fs_context.cpp
//
// FsContext — VirtualFS registry, script-origin stack, cwd, and path resolver.
// STL-only (fs/ is L0 under the layering guard); throws std::runtime_error
// for unknown/unavailable filesystems. The Engine rethrows those as
// numkit::Error to preserve the MATLAB-visible error type.
#include <numkit/fs/fs_context.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace numkit {

// ── Registry ──────────────────────────────────────────────────

void FsContext::registerVirtualFS(std::unique_ptr<VirtualFS> fs)
{
    if (!fs)
        return;
    auto n = fs->name();
    virtualFs_[n] = std::move(fs);
}

VirtualFS *FsContext::findVirtualFS(const std::string &name) const
{
    auto it = virtualFs_.find(name);
    return (it != virtualFs_.end()) ? it->second.get() : nullptr;
}

// ── Script-origin stack ───────────────────────────────────────

void FsContext::pushScriptOrigin(const std::string &fsName)
{
    scriptOriginStack_.push_back({fsName, std::string{}});
}

void FsContext::pushScriptOrigin(const std::string &fsName, const std::string &scriptDir)
{
    scriptOriginStack_.push_back({fsName, scriptDir});
}

void FsContext::popScriptOrigin()
{
    if (!scriptOriginStack_.empty())
        scriptOriginStack_.pop_back();
}

const std::string *FsContext::currentScriptOrigin() const
{
    return scriptOriginStack_.empty() ? nullptr : &scriptOriginStack_.back().fsName;
}

const std::string *FsContext::currentScriptDir() const
{
    return scriptOriginStack_.empty() ? nullptr : &scriptOriginStack_.back().scriptDir;
}

// ── Path resolution ───────────────────────────────────────────

namespace {

// Split "prefix:rest" into {prefix, rest} if `prefix` is a known FS name,
// otherwise return {"", path}. Two guards against false positives on
// paths that happen to contain ':':
//   • colon must be at index >= 2, so Windows drive letters (C:/foo) and
//     empty prefixes (":foo") never look like a scheme. This forbids
//     single-character FS names by construction — acceptable because all
//     current FS names ('native', 'temporary', 'local') are longer.
//   • the prefix must match a registered FS. So a path like "http://..."
//     or "mailto:..." falls through to the default FS untouched.
std::pair<std::string, std::string> splitFsScheme(const std::string &path,
                                                  const std::unordered_map<std::string, std::unique_ptr<VirtualFS>> &fsMap)
{
    auto colon = path.find(':');
    if (colon == std::string::npos || colon < 2)
        return {"", path};
    std::string scheme = path.substr(0, colon);
    if (fsMap.find(scheme) == fsMap.end())
        return {"", path};
    return {scheme, path.substr(colon + 1)};
}

bool isAbsolutePath(const std::string &p)
{
    if (p.empty())
        return false;
    if (p[0] == '/' || p[0] == '\\')
        return true;
#ifdef _WIN32
    if (p.size() >= 2 && p[1] == ':' && ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')))
        return true;
#endif
    return false;
}

std::string joinPath(const std::string &base, const std::string &rel)
{
    if (base.empty())
        return rel;
    if (rel.empty())
        return base;
    char last = base.back();
    if (last == '/' || last == '\\')
        return base + rel;
    return base + "/" + rel;
}

} // namespace

FsContext::ResolvedPath FsContext::resolvePath(const std::string &userPath) const
{
    // 1. Explicit scheme in the path wins.
    auto [scheme, rest] = splitFsScheme(userPath, virtualFs_);
    if (!scheme.empty()) {
        auto *fs = findVirtualFS(scheme);
        if (!fs)
            throw std::runtime_error("unknown filesystem '" + scheme + "' in path");
        return {fs, rest};
    }

    // 2. NUMKIT_FS env var selects the backend.
    std::string fsName = envGet(envVarName("FS").c_str());
    if (fsName == "auto")
        fsName.clear();

    // 3. Fall back to script origin, then to "native".
    if (fsName.empty()) {
        if (auto *o = currentScriptOrigin())
            fsName = *o;
    }
    if (fsName.empty())
        fsName = "native";

    VirtualFS *fs = findVirtualFS(fsName);
    if (!fs)
        throw std::runtime_error("filesystem '" + fsName + "' is not available");

    // Normalize path: if relative, prepend the cwd. Precedence:
    //   1. cwd_ when set (`cd`/`setCwd` write here — canonical).
    //   2. NUMKIT_CWD env var (host-runtime override; only consulted
    //      when the engine hasn't been told a cwd of its own).
    // No "two sources diverge" risk: cwd_ wins whenever it's non-empty.
    // The env fallback exists so hosts can `setenv NUMKIT_CWD` after
    // engine construction without needing to call setCwd explicitly.
    std::string path = userPath;
    if (!isAbsolutePath(path)) {
        std::string cwd = !cwd_.empty() ? cwd_
                                        : envGet(envVarName("CWD").c_str());
        if (cwd.empty()) {
            if (auto *sdir = currentScriptDir()) {
                if (!sdir->empty()) cwd = *sdir;
            }
        }
        if (!cwd.empty())
            path = joinPath(cwd, path);
    }

    return {fs, path};
}

// ── MATLAB-style file descriptor table ─────────────────────────

int FsContext::openFile(const std::string &userPath, const std::string &modeRaw)
{
    lastFopenError_.clear();

    // Strip Windows-style 't'/'b' suffix ("rt", "wb"). The underlying
    // buffer is bytes anyway; we don't do CRLF translation.
    std::string mode = modeRaw;
    while (!mode.empty() && (mode.back() == 't' || mode.back() == 'b'))
        mode.pop_back();

    // Accept the six MATLAB modes. 'r+'/'w+'/'a+' grant both read and
    // write permission; the base letter still governs seed/truncate/
    // append behaviour.
    bool canRead = false, canWrite = false, appendOnly = false, truncate = false, seedBuffer = false;
    if      (mode == "r")  { canRead = true;  seedBuffer = true; }
    else if (mode == "w")  { canWrite = true; truncate = true; }
    else if (mode == "a")  { canWrite = true; appendOnly = true; seedBuffer = true; }
    else if (mode == "r+") { canRead = true;  canWrite = true; seedBuffer = true; }
    else if (mode == "w+") { canRead = true;  canWrite = true; truncate = true; }
    else if (mode == "a+") { canRead = true;  canWrite = true; appendOnly = true; seedBuffer = true; }
    else {
        lastFopenError_ = "Invalid permission specified";
        return -1;
    }

    ResolvedPath r;
    try {
        r = resolvePath(userPath);
    } catch (const std::exception &e) {
        lastFopenError_ = e.what();
        return -1;
    }

    OpenFile f;
    f.path = r.path;
    f.mode = mode;
    f.fs = r.fs;
    f.forRead = canRead;
    f.forWrite = canWrite;
    f.appendOnly = appendOnly;

    if (seedBuffer) {
        // Plain 'r' and 'r+' demand the file exist (MATLAB: "File must
        // exist"). 'a' / 'a+' tolerate a missing target and start from
        // an empty buffer.
        const bool requireExisting = (mode == "r" || mode == "r+");
        try {
            if (r.fs->exists(r.path))
                f.buffer = r.fs->readFile(r.path);
            else if (requireExisting) {
                lastFopenError_ = "No such file or directory";
                return -1;
            }
        } catch (const std::exception &e) {
            if (requireExisting) {
                lastFopenError_ = e.what();
                return -1;
            }
            f.buffer.clear();
        }
    }
    if (truncate)
        f.buffer.clear();
    if (appendOnly)
        f.cursor = f.buffer.size();

    int fid = nextFid_++;
    openFiles_.emplace(fid, std::move(f));
    return fid;
}

bool FsContext::closeFile(int fid)
{
    auto it = openFiles_.find(fid);
    if (it == openFiles_.end())
        return false;

    bool ok = true;
    // Always commit on close for write modes — MATLAB semantics require
    // fopen('w')+fclose to leave an empty file behind, and 'a' should
    // preserve existing content even when no fprintf happened.
    if (it->second.forWrite) {
        try {
            it->second.fs->writeFile(it->second.path, it->second.buffer);
        } catch (const std::exception &) {
            ok = false;
        }
    }
    openFiles_.erase(it);
    return ok;
}

void FsContext::closeAllFiles()
{
    // Flush every user fid; swallow individual failures — the caller is
    // typically a destructor or a `fclose('all')` where partial success
    // shouldn't abort the rest.
    std::vector<int> fids;
    fids.reserve(openFiles_.size());
    for (auto &kv : openFiles_)
        fids.push_back(kv.first);
    for (int fid : fids)
        closeFile(fid);
}

FsContext::OpenFile *FsContext::findFile(int fid)
{
    auto it = openFiles_.find(fid);
    return (it == openFiles_.end()) ? nullptr : &it->second;
}

std::vector<int> FsContext::openFileIds() const
{
    std::vector<int> ids;
    ids.reserve(openFiles_.size());
    for (auto &kv : openFiles_)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace numkit
