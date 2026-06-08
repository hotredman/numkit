// core-libs/src/runtime/workspace.cpp
//
// Language-runtime workspace builtins, extracted verbatim from toolboxes/builtin's
// registerWorkspaceBuiltins into the core-libs layer (L2, engine-coupled scripting
// runtime — NOT a math/io toolbox). Behaviour unchanged. registerWorkspaceRuntime
// is composed by installRuntimeLibrary (runtime.cpp).
//
// Cluster so far: assignin / inputname. (clear / clearvars / who / whos / exist
// follow as the extraction proceeds.)
#include <numkit/corelibs/runtime.hpp>

#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <stdexcept>
#include <string>

namespace numkit::corelibs {

void registerWorkspaceRuntime(Engine &engine)
{
    // ── assignin ──────────────────────────────────────────────
    // assignin(workspace, name, val) — write `name = val` in
    // `workspace`, where `workspace` is 'base' (top-level) or 'caller'
    // (the workspace of the function that called the function
    // containing this assignin). VM mode also write-throughs to the
    // target frame's register if `name` is statically allocated, so
    // subsequent register-based reads in the target pick up the value.
    engine.registerFunction(
        "assignin", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() != 3)
                throw std::runtime_error("assignin: requires 3 arguments (workspace, name, value)");
            if (!args[0].isChar())
                throw std::runtime_error("assignin: workspace must be 'base' or 'caller'");
            if (!args[1].isChar())
                throw std::runtime_error("assignin: name must be a string");
            std::string where = args[0].toString();
            std::string name = args[1].toString();
            if (name.empty())
                throw std::runtime_error("assignin: name must be non-empty");
            if (where == "base") {
                ctx.engine->workspaceEnv().set(name, args[2]);
            } else if (where == "caller") {
                // 'caller' is invalid when assignin is called directly
                // from the base workspace — there's no enclosing
                // function to take a caller of (matches MATLAB's
                // "ASSIGNIN cannot have 'caller' as a workspace when
                // used in the base workspace").
                if (ctx.engine->callerDepth() < 1)
                    throw std::runtime_error(
                        "assignin: 'caller' is not valid in the base workspace");
                // Depth 1 = the function that called the function
                // containing this assignin call. Depth 0 would be the
                // assignin-containing function itself.
                ctx.engine->assignToCaller(1, name, args[2]);
            } else {
                throw std::runtime_error(
                    "assignin: workspace must be 'base' or 'caller', got '" + where + "'");
            }
            outs[0] = Value();
        });

    // ── inputname ────────────────────────────────────────────
    // inputname(k) returns the variable name of the k-th input arg as
    // written at the call site of the function containing this call.
    // Empty string if the arg was a literal / expression / non-identifier.
    // Throws when called from outside a function or for k < 1.
    engine.registerFunction(
        "inputname", [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() != 1)
                throw std::runtime_error("inputname: requires one argument (k)");
            double kd = args[0].toScalar();
            int k = static_cast<int>(kd);
            if (static_cast<double>(k) != kd || k < 1)
                throw std::runtime_error("inputname: k must be a positive integer");
            std::string name = ctx.engine->inputName(k);
            outs[0] = Value::fromString(name, ctx.engine->resource());
        });
}

} // namespace numkit::corelibs
