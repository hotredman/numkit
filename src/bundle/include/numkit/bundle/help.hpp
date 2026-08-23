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

/// @brief Installs MATLAB-compatible help and documentation commands into the script Engine.
class HelpLibrary {
public:
    /// @brief Registers `help`, `doc`, `what`, `builtins`, `categories`, and `inmem` in the given engine.
    /// @param engine Engine instance.
    static void install(Engine &engine);
};

/// @brief Convenience alias to install help system into an engine.
/// @param engine Engine instance.
inline void registerHelpLibrary(Engine &engine)
{
    HelpLibrary::install(engine);
}

// ── C++ Public API ──────────────────────────────────────────────────────────

/// @brief Formats help text or catalog index for given function or topic name (`help query`).
/// @param query Function, category, or empty string to display root catalog index.
/// @return Formatted help string.
/// @see what, builtins, categories
std::string help(const std::string &query = "");

/// @brief MATLAB script engine wrapper for `help`.
/// @param args Command-line arguments.
/// @param mr Memory resource for output allocation.
/// @return Help string value.
Value help(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Lists all functions in specified category (`what category`).
/// @param category Category name (e.g. `"elmat"`, `"signal"`, `"stats"`).
/// @return Vector of function names in category.
/// @see help, builtins, categories
std::vector<std::string> what(const std::string &category = "elmat");

/// @brief MATLAB script engine wrapper for `what`.
/// @param args Category argument.
/// @param mr Memory resource for output allocation.
/// @return Cell array of strings.
Value what(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Lists all built-in functions registered in the system or specified category (`builtins`).
/// @param category Category name (or empty string for all built-ins).
/// @return Vector of registered builtin function names.
/// @see help, what, categories
std::vector<std::string> builtins(const std::string &category = "");

/// @brief MATLAB script engine wrapper for `builtins`.
/// @param args Category argument.
/// @param mr Memory resource for output allocation.
/// @return Cell array of strings.
Value builtins(Span<const Value> args, std::pmr::memory_resource *mr = nullptr);

/// @brief Lists all registered documentation categories (`categories`).
/// @return Vector of category identifiers (e.g. `"elmat"`, `"elfun"`, `"signal"`, ...).
/// @see help, what, builtins
std::vector<std::string> categories();

/// @brief MATLAB script engine wrapper for `categories`.
/// @param mr Memory resource for output allocation.
/// @return Cell array of category name strings.
Value categories(std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::bundle
