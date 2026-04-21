// libs/builtin/src/MStdDiagnostics.cpp

#include <numkit/m/builtin/MStdDiagnostics.hpp>
#include <numkit/m/builtin/MStdFormat.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include <string>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

void error(Span<const MValue> args)
{
    if (args.empty())
        throw MError("Error");

    // error(MException-struct)
    if (args[0].isStruct()) {
        std::string msg = args[0].hasField("message") ? args[0].field("message").toString()
                                                      : "Error";
        std::string id = args[0].hasField("identifier")
                             ? args[0].field("identifier").toString()
                             : "";
        throw MError(msg, 0, 0, "", "", id);
    }

    std::string first = args[0].toString();

    // error(id, msg, ...) — identifier contains ':'
    if (args.size() >= 2 && first.find(':') != std::string::npos
        && (args[1].isChar() || args[1].isString())) {
        std::string id = first;
        std::string msg = (args.size() > 2)
                              ? formatOnce(args[1].toString(), args, 2)
                              : args[1].toString();
        throw MError(msg, 0, 0, "", "", id);
    }

    // error(msg) or error(msg, arg1, ...)
    std::string msg = (args.size() > 1) ? formatOnce(first, args, 1) : first;
    throw MError(msg);
}

void warning(Engine &engine, Span<const MValue> args)
{
    if (args.empty())
        return;
    std::string first = args[0].toString();
    std::string msg;
    if (args.size() >= 2 && first.find(':') != std::string::npos
        && (args[1].isChar() || args[1].isString())) {
        msg = (args.size() > 2) ? formatOnce(args[1].toString(), args, 2)
                                : args[1].toString();
    } else if (args.size() > 1) {
        msg = formatOnce(first, args, 1);
    } else {
        msg = first;
    }
    engine.outputText("Warning: " + msg + "\n");
}

MValue mexception(Allocator &alloc, Span<const MValue> args)
{
    if (args.size() < 2)
        throw MError("MException requires identifier and message", 0, 0,
                     "MException", "", "m:MException:nargin");
    std::string id = args[0].toString();
    std::string msg = (args.size() > 2) ? formatOnce(args[1].toString(), args, 2)
                                        : args[1].toString();
    auto me = MValue::structure();
    me.field("identifier") = MValue::fromString(id, &alloc);
    me.field("message") = MValue::fromString(msg, &alloc);
    return me;
}

void rethrowStruct(const MValue &me)
{
    if (!me.isStruct())
        throw MError("rethrow requires an MException struct", 0, 0, "rethrow", "",
                     "m:rethrow:notStruct");
    std::string msg = me.hasField("message") ? me.field("message").toString() : "Error";
    std::string id =
        me.hasField("identifier") ? me.field("identifier").toString() : "m:error";
    throw MError(msg, 0, 0, "", "", id);
}

void assertCond(Span<const MValue> args)
{
    if (args.empty())
        throw MError("assert requires at least one argument", 0, 0, "assert", "",
                     "m:assert:nargin");
    if (args[0].toBool())
        return; // assertion passed
    if (args.size() == 1)
        throw MError("Assertion failed.", 0, 0, "", "", "m:assert");

    // assert(cond, MException struct)
    if (args[1].isStruct()) {
        std::string msg = args[1].hasField("message")
                              ? args[1].field("message").toString()
                              : "Assertion failed.";
        std::string id = args[1].hasField("identifier")
                             ? args[1].field("identifier").toString()
                             : "m:assert";
        throw MError(msg, 0, 0, "", "", id);
    }

    std::string first = args[1].toString();

    // assert(cond, id, msg, ...) — id with ':'
    if (args.size() >= 3 && first.find(':') != std::string::npos) {
        std::string id = first;
        std::string msg = (args.size() > 3)
                              ? formatOnce(args[2].toString(), args, 3)
                              : args[2].toString();
        throw MError(msg, 0, 0, "", "", id);
    }

    // assert(cond, msg) / assert(cond, msg, arg1, ...)
    std::string msg = (args.size() > 2) ? formatOnce(first, args, 2) : first;
    throw MError(msg, 0, 0, "", "", "m:assert");
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void error_reg(Span<const MValue> args, size_t, Span<MValue>, CallContext &)
{
    error(args);
}

void warning_reg(Span<const MValue> args, size_t, Span<MValue>, CallContext &ctx)
{
    warning(*ctx.engine, args);
}

void MException_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    outs[0] = mexception(ctx.engine->allocator(), args);
}

void rethrow_reg(Span<const MValue> args, size_t, Span<MValue>, CallContext &)
{
    if (args.empty())
        throw MError("rethrow requires an MException struct", 0, 0, "rethrow", "",
                     "m:rethrow:nargin");
    rethrowStruct(args[0]);
}

void throw_reg(Span<const MValue> args, size_t, Span<MValue>, CallContext &)
{
    if (args.empty())
        throw MError("throw requires an MException struct", 0, 0, "throw", "",
                     "m:throw:nargin");
    rethrowStruct(args[0]);
}

void assert_reg(Span<const MValue> args, size_t, Span<MValue>, CallContext &)
{
    assertCond(args);
}

} // namespace detail

} // namespace numkit::m::builtin
