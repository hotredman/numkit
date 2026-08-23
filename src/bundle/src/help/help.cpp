// src/bundle/src/help/help.cpp
//
// Help system C++ API and Engine registration adapters for NumKit bundle.

#include <numkit/bundle/help.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace numkit::bundle {

// ════════════════════════════════════════════════════════════════════════
// C++ API
// ════════════════════════════════════════════════════════════════════════

std::string help(const std::string &query)
{
    const auto &catalog = HelpCatalog::instance();
    if (query.empty()) {
        return catalog.formatAllCategories();
    }
    const HelpCategory *cat = catalog.findCategory(query);
    if (cat) {
        return catalog.formatCategory(cat->name);
    }
    const HelpEntry *func = catalog.findFunction(query);
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
    const auto &catalog = HelpCatalog::instance();
    const HelpCategory *cat = catalog.findCategory(category);
    if (cat) {
        return catalog.getCategoryFunctions(cat->name);
    }
    return catalog.getCategoryFunctions(category);
}

Value what(Span<const Value> args, std::pmr::memory_resource *mr)
{
    const auto &catalog = HelpCatalog::instance();
    std::string topic = args.empty() ? "elmat" : args[0].toString();
    const HelpCategory *cat = catalog.findCategory(topic);
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
    const auto &catalog = HelpCatalog::instance();
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
    const auto &catalog = HelpCatalog::instance();
    std::vector<std::string> cats;
    cats.reserve(catalog.categories().size());
    for (const auto &c : catalog.categories()) {
        cats.push_back(c.name);
    }
    return cats;
}

Value categories(std::pmr::memory_resource *mr)
{
    const auto &catalog = HelpCatalog::instance();
    Value cellOut = Value::cell(catalog.categories().size(), 1, mr);
    for (size_t i = 0; i < catalog.categories().size(); ++i) {
        cellOut.cellAt(i) = Value::fromString(catalog.categories()[i].name, mr);
    }
    return cellOut;
}

// ════════════════════════════════════════════════════════════════════════
// Engine Registration Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

static void help_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
    const auto &catalog = HelpCatalog::instance();
    std::string text;

    if (args.empty()) {
        text = catalog.formatAllCategories();
    } else {
        std::string query = args[0].isChar() ? args[0].toString() : "";
        if (query.empty() && args[0].isString()) query = args[0].toString();

        const HelpCategory *cat = catalog.findCategory(query);
        if (cat) {
            text = catalog.formatCategory(cat->name);
        } else {
            const HelpEntry *func = catalog.findFunction(query);
            if (func) {
                text = catalog.formatFunction(func->name);
            } else if (ctx.engine->hasUserFunction(query)) {
                text = query + " is a user-defined function.\n";
            } else if (ctx.engine->hasExternalFunction(query)) {
                text = query + " is a built-in function.\n";
            } else {
                text = "'" + query + "' not found. Type 'help' for a list of topics.\n";
            }
        }
    }

    if (nargout > 0) {
        outs[0] = Value::fromString(text, ctx.engine->resource());
    } else {
        ctx.engine->outputText(text);
        outs[0] = Value();
    }
}

static void what_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
    if (nargout > 0) {
        outs[0] = what(args, ctx.engine->resource());
    } else {
        std::string topic = args.empty() ? "elmat" : (args[0].isChar() ? args[0].toString() : "");
        const auto &catalog = HelpCatalog::instance();
        const HelpCategory *cat = catalog.findCategory(topic);
        std::vector<std::string> funcs = what(topic);
        std::string title = cat ? cat->title : topic;

        std::ostringstream os;
        os << "Functions in " << topic << " (" << title << "):\n\n";
        for (size_t i = 0; i < funcs.size(); ++i) {
            os << std::left << std::setw(16) << funcs[i];
            if ((i + 1) % 4 == 0 || i + 1 == funcs.size()) os << "\n";
        }
        os << "\n";
        ctx.engine->outputText(os.str());
        outs[0] = Value();
    }
}

static void builtins_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
    outs[0] = builtins(args, ctx.engine->resource());
}

static void categories_reg(Span<const Value>, size_t, Span<Value> outs, CallContext &ctx) {
    outs[0] = categories(ctx.engine->resource());
}

static void inmem_reg(Span<const Value>, size_t nargout, Span<Value> outs, CallContext &ctx) {
    std::vector<std::string> userFuncs;
    std::vector<std::string> classes;

    for (const auto &name : ctx.engine->namespaces()) {
        classes.push_back(name);
    }

    Value mCell = Value::cell(userFuncs.size(), 1, ctx.engine->resource());
    for (size_t i = 0; i < userFuncs.size(); ++i) {
        mCell.cellAt(i) = Value::fromString(userFuncs[i], ctx.engine->resource());
    }
    outs[0] = std::move(mCell);

    if (nargout > 1) {
        outs[1] = Value::cell(0, 1, ctx.engine->resource()); // MEX
    }
    if (nargout > 2) {
        Value cCell = Value::cell(classes.size(), 1, ctx.engine->resource());
        for (size_t i = 0; i < classes.size(); ++i) {
            cCell.cellAt(i) = Value::fromString(classes[i], ctx.engine->resource());
        }
        outs[2] = std::move(cCell);
    }
}

} // namespace detail

void HelpLibrary::install(Engine &engine)
{
    engine.registerFunction("help",       &detail::help_reg);
    engine.registerFunction("doc",        &detail::help_reg);
    engine.registerFunction("what",       &detail::what_reg);
    engine.registerFunction("builtins",   &detail::builtins_reg);
    engine.registerFunction("categories", &detail::categories_reg);
    engine.registerFunction("inmem",      &detail::inmem_reg);
}

} // namespace numkit::bundle
