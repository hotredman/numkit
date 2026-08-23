/// @file help.hpp
/// @ingroup group_matlab
#pragma once

#include <numkit/bundle/help/help_catalog.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <memory_resource>
#include <string>
#include <vector>

namespace numkit {
class Engine;
}

namespace numkit::bundle {

class HelpLibrary {
public:
    static void install(Engine &engine);
};

inline void registerHelpLibrary(Engine &engine)
{
    HelpLibrary::install(engine);
}

// C++ API
std::string help(const std::string &query = "");
Value help(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

std::vector<std::string> what(const std::string &category = "elmat");
Value what(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

std::vector<std::string> builtins(const std::string &category = "");
Value builtins(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

std::vector<std::string> categories();
Value categories(std::pmr::memory_resource *mr);

} // namespace numkit::bundle
