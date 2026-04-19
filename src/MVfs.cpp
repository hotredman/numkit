// src/MVfs.cpp

#include "MVfs.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace numkit::m::m {

std::string NativeFS::readFile(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot open '" + path + "' for reading");
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

void NativeFS::writeFile(const std::string &path, const std::string &content)
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot open '" + path + "' for writing");
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!f)
        throw std::runtime_error("write to '" + path + "' failed");
}

bool NativeFS::exists(const std::string &path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

} // namespace numkit::m::m
