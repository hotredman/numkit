// src/builtin/src/general/catalog.cpp
//
// Catalog introspection and help documentation implementations for numkit::builtin.

#include <numkit/builtin/general.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/runtime/help/help_catalog.hpp>

#include <string>
#include <vector>

namespace numkit::builtin {

std::string help(const std::string &query)
{
    const auto &catalog = runtime::HelpCatalog::instance();
    if (query.empty()) {
        return catalog.formatAllCategories();
    }
    const runtime::HelpCategory *cat = catalog.findCategory(query);
    if (cat) {
        return catalog.formatCategory(cat->name);
    }
    const runtime::HelpEntry *func = catalog.findFunction(query);
    if (func) {
        return catalog.formatFunction(func->name);
    }
    return "'" + query + "' not found. Type 'help' for a list of topics.\n";
}

Value help(Span<const Value> args, std::pmr::memory_resource *mr)
{
    std::string query = args.empty() ? "" : args[0].toString();
    return Value::fromString(help(query), mr);
}

std::vector<std::string> what(const std::string &category)
{
    const auto &catalog = runtime::HelpCatalog::instance();
    const runtime::HelpCategory *cat = catalog.findCategory(category);
    if (cat) {
        return catalog.getCategoryFunctions(cat->name);
    }
    return catalog.getCategoryFunctions(category);
}

Value what(Span<const Value> args, std::pmr::memory_resource *mr)
{
    const auto &catalog = runtime::HelpCatalog::instance();
    std::string topic = args.empty() ? "elmat" : args[0].toString();
    const runtime::HelpCategory *cat = catalog.findCategory(topic);
    std::vector<std::string> funcs = what(topic);

    Value cellM = Value::cell(funcs.size(), 1, mr);
    for (size_t i = 0; i < funcs.size(); ++i) {
        cellM.cellAt(i) = Value::fromString(funcs[i], mr);
    }
    Value st = Value::structure(mr);
    st.structFields()["path"] = Value::fromString(cat ? cat->name : topic, mr);
    st.structFields()["m"] = std::move(cellM);
    st.structFields()["classes"] = Value::cell(0, 1, mr);
    st.structFields()["packages"] = Value::cell(0, 1, mr);
    return st;
}

std::vector<std::string> builtins(const std::string &category)
{
    const auto &catalog = runtime::HelpCatalog::instance();
    if (category.empty()) {
        return catalog.getAllFunctions();
    }
    return catalog.getCategoryFunctions(category);
}

Value builtins(Span<const Value> args, std::pmr::memory_resource *mr)
{
    std::string topic = args.empty() ? "" : args[0].toString();
    std::vector<std::string> funcs = builtins(topic);
    Value cellOut = Value::cell(funcs.size(), 1, mr);
    for (size_t i = 0; i < funcs.size(); ++i) {
        cellOut.cellAt(i) = Value::fromString(funcs[i], mr);
    }
    return cellOut;
}

std::vector<std::string> categories()
{
    const auto &catalog = runtime::HelpCatalog::instance();
    std::vector<std::string> cats;
    cats.reserve(catalog.categories().size());
    for (const auto &c : catalog.categories()) {
        cats.push_back(c.name);
    }
    return cats;
}

Value categories(std::pmr::memory_resource *mr)
{
    std::vector<std::string> cats = categories();
    Value cellOut = Value::cell(cats.size(), 1, mr);
    for (size_t i = 0; i < cats.size(); ++i) {
        cellOut.cellAt(i) = Value::fromString(cats[i], mr);
    }
    return cellOut;
}

} // namespace numkit::builtin
