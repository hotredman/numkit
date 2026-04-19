// include/MVfs.hpp
//
// Minimal filesystem abstraction that lets built-ins like csvread / csvwrite
// route through either the real disk (native/CLI) or an IDE-supplied virtual
// filesystem (IndexedDB in the browser, a mounted folder in Electron).
//
// Design notes:
//
//   * The interface is deliberately small — read/write/exists for strings.
//     Binary I/O and directory operations get added as built-ins need them.
//
//   * Two implementations ship here: NativeFS (std::filesystem) and
//     CallbackFS (delegates to std::function hooks). The IDE installs a
//     CallbackFS on the Engine at startup and points its hooks at tempFS
//     or the Local Folder backend in JS.
//
//   * The Engine owns a small registry ("native", "temporary", "local"),
//     plus a path resolver that reads MLAB_FS / MLAB_CWD (via env) and the
//     current script's origin.
//
//   * A backend that isn't registered is simply absent — asking for
//     "temporary" on a CLI build throws. No silent fallback.

#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace numkit {

class VirtualFS
{
public:
    virtual ~VirtualFS() = default;

    virtual std::string readFile(const std::string &path) = 0;
    virtual void writeFile(const std::string &path, const std::string &content) = 0;
    virtual bool exists(const std::string &path) = 0;

    virtual std::string name() const = 0;
};

// ── NativeFS — std::filesystem / std::ifstream / std::ofstream ──
class NativeFS final : public VirtualFS
{
public:
    std::string readFile(const std::string &path) override;
    void writeFile(const std::string &path, const std::string &content) override;
    bool exists(const std::string &path) override;
    std::string name() const override { return "native"; }
};

// ── CallbackFS — delegates to std::function hooks (WASM bridge) ──
class CallbackFS final : public VirtualFS
{
public:
    using ReadFunc = std::function<std::string(const std::string &)>;
    using WriteFunc = std::function<void(const std::string &, const std::string &)>;
    using ExistsFunc = std::function<bool(const std::string &)>;

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

private:
    std::string name_;
    ReadFunc read_;
    WriteFunc write_;
    ExistsFunc exists_;
};

} // namespace numkit
