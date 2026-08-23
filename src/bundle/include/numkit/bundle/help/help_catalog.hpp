#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace numkit::bundle {

struct HelpEntry {
    std::string name;
    std::string summary;
    std::string signature;
    std::string doc;
};

struct HelpSection {
    std::string title;
    std::vector<HelpEntry> entries;
};

struct HelpCategory {
    std::string name;        // e.g. "elmat", "elfun", "images", "signal"
    std::string title;       // e.g. "Elementary matrices and matrix manipulation."
    std::vector<HelpSection> sections;
};

class HelpCatalog {
public:
    static const HelpCatalog &instance();

    // List all registered categories in order
    const std::vector<HelpCategory> &categories() const { return categories_; }

    // Find category by name (case-insensitive, alias-aware e.g. "images" -> "image")
    const HelpCategory *findCategory(std::string name) const;

    // Find function help by name (case-insensitive)
    const HelpEntry *findFunction(std::string name) const;

    // Generate formatted MATLAB-style help overview of all categories
    std::string formatAllCategories() const;

    // Generate formatted MATLAB-style help for a specific category (e.g. "help elmat")
    std::string formatCategory(const std::string &catName) const;

    // Generate formatted help for a specific function (e.g. "help sin")
    std::string formatFunction(const std::string &funcName) const;

    // Return list of function names in a given category
    std::vector<std::string> getCategoryFunctions(const std::string &catName) const;

    // Return list of all function names across all categories
    std::vector<std::string> getAllFunctions() const;

private:
    HelpCatalog();
    void initCategories();

    std::vector<HelpCategory> categories_;
    std::unordered_map<std::string, size_t> categoryIndex_;
    std::unordered_map<std::string, HelpEntry> functionIndex_;
};

} // namespace numkit::bundle
