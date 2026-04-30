// include/vfs.hpp
//
// Minimal filesystem abstraction that lets built-ins like csvread / csvwrite
// route through either the real disk (native/CLI) or an IDE-supplied virtual
// filesystem (IndexedDB in the browser, a mounted folder in Electron).
//
// Design notes:
//
//   * Two implementations ship here: NativeFS (std::filesystem) and
//     CallbackFS (delegates to std::function hooks). The IDE installs a
//     CallbackFS on the Engine at startup and points its hooks at tempFS
//     or the Local Folder backend in JS.
//
//   * The Engine owns a small registry ("native", "temporary", "local"),
//     plus a path resolver that reads NUMKIT_FS / NUMKIT_CWD (via env) and the
//     current script's origin.
//
//   * A backend that isn't registered is simply absent — asking for
//     "temporary" on a CLI build throws. No silent fallback.
//
//   * The interface is deliberately small. Phase 8 added listDir / stat /
//     mkdir / rmdir / unlink / tempArea — needed by the m-file resolver
//     and the dir / mkdir / rmdir / delete / tempdir / tempname / which /
//     exist('foo','file') builtins. All have safe default implementations
//     so older CallbackFS hosts that omit the new hooks still work for
//     legacy read/write traffic.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit {

// ── DirEntry / FileStat — types returned by VFS introspection methods ──

struct DirEntry
{
    std::string name;        // basename only (no path prefix)
    bool isDirectory = false;
};

struct FileStat
{
    int64_t size = 0;        // bytes (0 for directories)
    int64_t mtime = 0;       // last-modified time, seconds since epoch
                             // (0 if backend can't supply one — caller falls
                             // back to content hash for cache invalidation)
    enum class Kind { File, Directory, Other };
    Kind kind = Kind::Other;
};

class VirtualFS
{
public:
    virtual ~VirtualFS() = default;

    virtual std::string readFile(const std::string &path) = 0;
    virtual void writeFile(const std::string &path, const std::string &content) = 0;
    virtual bool exists(const std::string &path) = 0;

    virtual std::string name() const = 0;

    // ── Phase 8 extension — directory ops + introspection ───────────
    // Default implementations throw — backends opt in by overriding.
    // Callers that want a graceful "unsupported" code-path catch
    // std::runtime_error.

    virtual std::vector<DirEntry> listDir(const std::string &path)
    {
        throw std::runtime_error(name() + ": listDir not supported");
    }

    // Returns std::nullopt when the path doesn't exist. Throws on
    // backend errors (perm denied etc.).
    virtual std::optional<FileStat> stat(const std::string &path)
    {
        return std::nullopt;
    }

    virtual void mkdir(const std::string &path)
    {
        throw std::runtime_error(name() + ": mkdir not supported");
    }

    virtual void rmdir(const std::string &path)
    {
        throw std::runtime_error(name() + ": rmdir not supported");
    }

    virtual void unlink(const std::string &path)
    {
        throw std::runtime_error(name() + ": unlink not supported");
    }

    // Returns the backend's temp area prefix (an absolute, possibly
    // VFS-prefixed path). NativeFS returns std::filesystem::temp_directory_path().
    // CallbackFS returns whatever the host provided, or "" if none.
    virtual std::string tempArea()
    {
        return {};
    }

    // Backend's "current directory" — used by pwd/cd before the engine's
    // own cwd is set. NativeFS returns std::filesystem::current_path().
    // Hosted backends (CallbackFS) return whatever the host hooked in,
    // or "" if none.
    virtual std::string cwd()
    {
        return {};
    }
};

// ── NativeFS — std::filesystem / std::ifstream / std::ofstream ──
class NativeFS final : public VirtualFS
{
public:
    std::string readFile(const std::string &path) override;
    void writeFile(const std::string &path, const std::string &content) override;
    bool exists(const std::string &path) override;
    std::string name() const override { return "native"; }

    // Phase 8 — directory + introspection
    std::vector<DirEntry> listDir(const std::string &path) override;
    std::optional<FileStat> stat(const std::string &path) override;
    void mkdir(const std::string &path) override;
    void rmdir(const std::string &path) override;
    void unlink(const std::string &path) override;
    std::string tempArea() override;
    std::string cwd() override;
};

// ── CallbackFS — delegates to std::function hooks (WASM bridge) ──
class CallbackFS final : public VirtualFS
{
public:
    using ReadFunc = std::function<std::string(const std::string &)>;
    using WriteFunc = std::function<void(const std::string &, const std::string &)>;
    using ExistsFunc = std::function<bool(const std::string &)>;

    // Phase 8 hooks. All optional — methods fall back to the
    // VirtualFS-level defaults (throw or empty) when not supplied.
    using ListDirFunc = std::function<std::vector<DirEntry>(const std::string &)>;
    using StatFunc = std::function<std::optional<FileStat>(const std::string &)>;
    using MkdirFunc = std::function<void(const std::string &)>;
    using RmdirFunc = std::function<void(const std::string &)>;
    using UnlinkFunc = std::function<void(const std::string &)>;
    using TempAreaFunc = std::function<std::string()>;
    using CwdFunc = std::function<std::string()>;

    CallbackFS(std::string n, ReadFunc r, WriteFunc w, ExistsFunc e)
        : name_(std::move(n)), read_(std::move(r)), write_(std::move(w)), exists_(std::move(e))
    {}

    std::string readFile(const std::string &path) override
    {
        if (!read_)
            throw std::runtime_error(name_ + ": read hook not installed");
        return read_(path);
    }
    void writeFile(const std::string &path, const std::string &content) override
    {
        if (!write_)
            throw std::runtime_error(name_ + ": write hook not installed");
        write_(path, content);
    }
    bool exists(const std::string &path) override { return exists_ ? exists_(path) : false; }
    std::string name() const override { return name_; }

    // Phase 8 — opt-in setters; absent hook → fall through to default behaviour.
    void setListDir(ListDirFunc f) { listDir_ = std::move(f); }
    void setStat(StatFunc f) { stat_ = std::move(f); }
    void setMkdir(MkdirFunc f) { mkdir_ = std::move(f); }
    void setRmdir(RmdirFunc f) { rmdir_ = std::move(f); }
    void setUnlink(UnlinkFunc f) { unlink_ = std::move(f); }
    void setTempArea(TempAreaFunc f) { tempArea_ = std::move(f); }
    void setCwd(CwdFunc f) { cwd_ = std::move(f); }

    std::vector<DirEntry> listDir(const std::string &path) override
    {
        if (listDir_) return listDir_(path);
        return VirtualFS::listDir(path);
    }
    std::optional<FileStat> stat(const std::string &path) override
    {
        if (stat_) return stat_(path);
        return std::nullopt;
    }
    void mkdir(const std::string &path) override
    {
        if (mkdir_) { mkdir_(path); return; }
        VirtualFS::mkdir(path);
    }
    void rmdir(const std::string &path) override
    {
        if (rmdir_) { rmdir_(path); return; }
        VirtualFS::rmdir(path);
    }
    void unlink(const std::string &path) override
    {
        if (unlink_) { unlink_(path); return; }
        VirtualFS::unlink(path);
    }
    std::string tempArea() override
    {
        return tempArea_ ? tempArea_() : std::string{};
    }
    std::string cwd() override
    {
        return cwd_ ? cwd_() : std::string{};
    }

private:
    std::string name_;
    ReadFunc read_;
    WriteFunc write_;
    ExistsFunc exists_;
    ListDirFunc listDir_;
    StatFunc stat_;
    MkdirFunc mkdir_;
    RmdirFunc rmdir_;
    UnlinkFunc unlink_;
    TempAreaFunc tempArea_;
    CwdFunc cwd_;
};

} // namespace numkit
