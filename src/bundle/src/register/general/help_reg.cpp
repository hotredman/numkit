#include <numkit/core/engine.hpp>
#include <numkit/runtime/help/help_catalog.hpp>
#include <numkit/runtime/language/structures/struct.hpp>
#include <numkit/runtime/language/cells/cell.hpp>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

namespace numkit::bundle::detail {

using namespace numkit::runtime;

void help_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
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

void what_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx) {
    const auto &catalog = HelpCatalog::instance();
    std::string topic = args.empty() ? "elmat" : (args[0].isChar() ? args[0].toString() : "");

    const HelpCategory *cat = catalog.findCategory(topic);
    std::vector<std::string> funcs;
    std::string title = topic;
    if (cat) {
        title = cat->title;
        funcs = catalog.getCategoryFunctions(cat->name);
    } else {
        funcs = catalog.getCategoryFunctions(topic);
    }

    if (nargout > 0) {
        // Return struct with field 'm' containing cellstr
        Value cellM = Value::cell(funcs.size(), 1, ctx.engine->resource());
        for (size_t i = 0; i < funcs.size(); ++i) {
            cellM.cellAt(i) = Value::fromString(funcs[i], ctx.engine->resource());
        }
        Value st = Value::structure(ctx.engine->resource());
        st.structFields()["path"] = Value::fromString(cat ? cat->name : topic, ctx.engine->resource());
        st.structFields()["m"] = std::move(cellM);
        st.structFields()["classes"] = Value::cell(0, 1, ctx.engine->resource());
        st.structFields()["packages"] = Value::cell(0, 1, ctx.engine->resource());
        outs[0] = std::move(st);
    } else {
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

void builtins_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx) {
    const auto &catalog = HelpCatalog::instance();
    std::vector<std::string> funcs;

    if (args.empty()) {
        funcs = catalog.getAllFunctions();
    } else {
        std::string topic = args[0].isChar() ? args[0].toString() : "";
        funcs = catalog.getCategoryFunctions(topic);
    }

    Value cellOut = Value::cell(funcs.size(), 1, ctx.engine->resource());
    for (size_t i = 0; i < funcs.size(); ++i) {
        cellOut.cellAt(i) = Value::fromString(funcs[i], ctx.engine->resource());
    }
    outs[0] = std::move(cellOut);
}

void inmem_reg(Span<const Value> /*args*/, size_t nargout, Span<Value> outs, CallContext &ctx) {
    // [M, MEX, C] = inmem
    // M: user-defined / loaded functions
    // MEX: MEX files (empty in Numkit)
    // C: classes
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

void registerHelpLibrary(Engine &engine) {
    engine.registerFunction("help", &help_reg);
    engine.registerFunction("doc", &help_reg);
    engine.registerFunction("what", &what_reg);
    engine.registerFunction("builtins", &builtins_reg);
    engine.registerFunction("inmem", &inmem_reg);
}

} // namespace numkit::bundle::detail
