// toolboxes/builtin/src/programming/errors/diagnostics.cpp

#include <numkit/builtin/programming/errors/diagnostics.hpp>
#include <numkit/lang/strings/format.hpp>
#include <numkit/core/engine.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <string>

namespace numkit::builtin {
using namespace numkit::lang;  // C4c (formatOnce)

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

void error(Span<const Value> args)
{
    if (args.empty())
        throw Error("Error");

    // error(MException-struct)
    if (args[0].isStruct()) {
        std::string msg = args[0].hasField("message") ? args[0].field("message").toString()
                                                      : "Error";
        std::string id = args[0].hasField("identifier")
                             ? args[0].field("identifier").toString()
                             : "";
        throw Error(msg, 0, 0, "", "", id);
    }

    std::string first = args[0].toString();

    // error(id, msg, ...) — identifier contains ':'
    if (args.size() >= 2 && first.find(':') != std::string::npos
        && (args[1].isChar() || args[1].isString())) {
        std::string id = first;
        std::string msg = (args.size() > 2)
                              ? formatOnce(args[1].toString(), args, 2)
                              : args[1].toString();
        throw Error(msg, 0, 0, "", "", id);
    }

    // error(msg) or error(msg, arg1, ...)
    std::string msg = (args.size() > 1) ? formatOnce(first, args, 1) : first;
    throw Error(msg);
}

// thread_local last-warning state. Updated by warning() and lastwarnSet,
// read by lastwarnGet. Private storage — TU-local accessor only.
namespace {
thread_local std::string gLastWarnMsg;
thread_local std::string gLastWarnId;
}

void warning(Engine &engine, Span<const Value> args)
{
    if (args.empty())
        return;
    std::string first = args[0].toString();
    std::string msg;
    std::string id;
    if (args.size() >= 2 && first.find(':') != std::string::npos
        && (args[1].isChar() || args[1].isString())) {
        id  = first;
        msg = (args.size() > 2) ? formatOnce(args[1].toString(), args, 2)
                                : args[1].toString();
    } else if (args.size() > 1) {
        msg = formatOnce(first, args, 1);
    } else {
        msg = first;
    }
    gLastWarnMsg = msg;
    gLastWarnId  = id;
    engine.outputText("Warning: " + msg + "\n");
}

LastWarn lastwarnGet()
{
    return { gLastWarnMsg, gLastWarnId };
}

void lastwarnSet(const std::string &msg, const std::string &id)
{
    gLastWarnMsg = msg;
    gLastWarnId  = id;
}

Value mexception(Span<const Value> args, std::pmr::memory_resource *mr)
{
    if (args.size() < 2)
        throw Error("MException requires identifier and message", 0, 0,
                     "MException", "", "numkit:MException:nargin");
    std::string id = args[0].toString();
    std::string msg = (args.size() > 2) ? formatOnce(args[1].toString(), args, 2)
                                        : args[1].toString();
    auto me = Value::structure();
    me.field("identifier") = Value::fromString(id, mr);
    me.field("message") = Value::fromString(msg, mr);
    return me;
}

void rethrowStruct(const Value &me)
{
    if (!me.isStruct())
        throw Error("rethrow requires an MException struct", 0, 0, "rethrow", "",
                     "numkit:rethrow:notStruct");
    std::string msg = me.hasField("message") ? me.field("message").toString() : "Error";
    std::string id =
        me.hasField("identifier") ? me.field("identifier").toString() : "numkit:error";
    throw Error(msg, 0, 0, "", "", id);
}

void assertCond(Span<const Value> args)
{
    if (args.empty())
        throw Error("assert requires at least one argument", 0, 0, "assert", "",
                     "numkit:assert:nargin");
    if (args[0].toBool())
        return; // assertion passed
    if (args.size() == 1)
        throw Error("Assertion failed.", 0, 0, "", "", "numkit:assert");

    // assert(cond, MException struct)
    if (args[1].isStruct()) {
        std::string msg = args[1].hasField("message")
                              ? args[1].field("message").toString()
                              : "Assertion failed.";
        std::string id = args[1].hasField("identifier")
                             ? args[1].field("identifier").toString()
                             : "numkit:assert";
        throw Error(msg, 0, 0, "", "", id);
    }

    std::string first = args[1].toString();

    // assert(cond, id, msg, ...) — id with ':'
    if (args.size() >= 3 && first.find(':') != std::string::npos) {
        std::string id = first;
        std::string msg = (args.size() > 3)
                              ? formatOnce(args[2].toString(), args, 3)
                              : args[2].toString();
        throw Error(msg, 0, 0, "", "", id);
    }

    // assert(cond, msg) / assert(cond, msg, arg1, ...)
    std::string msg = (args.size() > 2) ? formatOnce(first, args, 2) : first;
    throw Error(msg, 0, 0, "", "", "numkit:assert");
}

} // namespace numkit::builtin
