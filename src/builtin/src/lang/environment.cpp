// src/builtin/src/lang/environment.cpp
//
// Environment variable utilities for numkit::builtin.

#include <numkit/builtin/lang.hpp>
#include <numkit/fs/branding.hpp>
#include <numkit/fs/branding.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace numkit::builtin {

void setenv(const std::string &name, const std::string &value)
{
    if (name.empty())
        throw std::runtime_error("setenv: variable name cannot be empty");
    if (name.find('=') != std::string::npos)
        throw std::runtime_error("setenv: variable name cannot contain '='");
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    ::setenv(name.c_str(), value.c_str(), 1);
#endif
}

std::string getenv(const std::string &name)
{
#ifdef _WIN32
    char *buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, name.c_str()) == 0 && buf != nullptr) {
        std::string res(buf);
        free(buf);
        return res;
    }
    return "";
#else
    const char *val = ::getenv(name.c_str());
    return val ? std::string(val) : std::string();
#endif
}

Value getenv(const Value &name, std::pmr::memory_resource *mr)
{
    if (!name.isChar() && !name.isString())
        throw std::runtime_error("getenv: argument must be a variable name");
    return Value::fromString(getenv(name.toString()), mr);
}

} // namespace numkit::builtin
