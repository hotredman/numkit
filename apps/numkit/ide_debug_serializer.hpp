// apps/numkit/ide_debug_serializer.hpp
//
// Serialize numkit::DebugSession state for the native IDE pipe sub-protocol.
// Mirrors buildDebugResult() from wasm/src/repl_bindings.cpp so the desktop
// debugger and the WASM debugger produce identical JSON shapes.
//
// Protocol markers emitted to stdout:
//
//   When execution hits a breakpoint / step:
//     __BREAKPOINT__:{...pauseState JSON...}
//     __END_OF_STEP__
//
//   On normal completion or runtime error:
//     [captured stdout — may contain __FIGURE_DATA__: etc.]
//     __VARS__:{workspaceJSON}
//     __DEBUG_RESULT__:{"status":"completed"|"error",...}
//     __DEBUG_END__
//     __END_OF_RUN__
//
//   On explicit stop (__DEBUG_CMD__:stop):
//     __DEBUG_STOPPED__
//     __END_OF_RUN__
//
// pauseState JSON shape matches IDE.jsx handleDebugResult() expectations.
#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <numkit/core/engine.hpp>
#include <numkit/core/debug_session.hpp>
#include "ide_serializer.hpp"    // detail::escapeJSON, detail::valuePreview

namespace numkit { namespace ide {

// ─────────────────────────────────────────────────────────────────────────────
// Build the JSON payload for __BREAKPOINT__:
// Returns the full object string (without the marker prefix).
// ─────────────────────────────────────────────────────────────────────────────
inline std::string buildBreakpointJSON(numkit::DebugSession& session,
                                        numkit::Engine&       engine)
{
    auto snap    = session.snapshot();
    bool atError = session.atErrorPause();
    bool onBp    = engine.debug().breakpoints().shouldBreak(snap.line);
    const char* reason = atError ? "error" : (onBp ? "breakpoint" : "step");

    std::string r;
    r  = "{\"status\":\"paused\",\"pauseState\":{";
    r += "\"line\":"           + std::to_string(snap.line);
    r += ",\"col\":"           + std::to_string(snap.col);
    r += ",\"function\":\""    + detail::escapeJSON(snap.functionName) + "\"";
    r += ",\"reason\":\""      + std::string(reason) + "\"";
    if (atError)
        r += ",\"errorMessage\":\"" + detail::escapeJSON(session.errorPauseMessage()) + "\"";
    r += ",\"selectedFrame\":"  + std::to_string(session.selectedFrame());
    r += ",\"frameCount\":"     + std::to_string(session.frameCount());

    // Variables — same shape as workspaceJSON entries (type / size / preview)
    r += ",\"variables\":{";
    bool first = true;
    for (auto& v : snap.variables) {
        if (!v.value) continue;
        if (v.name == "nargin" || v.name == "nargout") continue;
        if (!first) r += ",";
        auto& val = *v.value;
        r += "\"" + detail::escapeJSON(v.name) + "\":{";
        r += "\"type\":\""   + std::string(numkit::mtypeName(val.type())) + "\"";
        auto& d = val.dims();
        r += ",\"size\":\"" + std::to_string(d.rows()) + "x" + std::to_string(d.cols());
        if (d.is3D()) r += "x" + std::to_string(d.pages());
        r += "\"";
        r += ",\"preview\":\"" + detail::escapeJSON(detail::valuePreview(val)) + "\"";
        r += "}";
        first = false;
    }
    r += "}";

    // Call stack — deepest (current) frame first
    r += ",\"callStack\":[";
    for (size_t i = 0; i < snap.callStack.size(); ++i) {
        if (i) r += ",";
        auto& sf = snap.callStack[i];
        r += "{\"function\":\"" + detail::escapeJSON(sf.functionName) + "\"";
        r += ",\"line\":"       + std::to_string(sf.line) + "}";
    }
    r += "]";

    // Watch expressions re-evaluated in the focused frame
    auto watches = session.evalWatches();
    if (!watches.empty()) {
        r += ",\"watches\":[";
        for (size_t i = 0; i < watches.size(); ++i) {
            if (i) r += ",";
            r += "{\"expr\":\""  + detail::escapeJSON(watches[i].expr)   + "\"";
            r += ",\"value\":\"" + detail::escapeJSON(watches[i].result) + "\"}";
        }
        r += "]";
    }

    r += "}";   // close pauseState

    // Output captured since the last start/resume (may contain __FIGURE_DATA__:).
    // The renderer enriches this string through extractMarkers() the same way
    // WASM does via enrichDebugResult().
    std::string output = session.takeOutput();
    if (!output.empty())
        r += ",\"output\":\"" + detail::escapeJSON(output) + "\"";

    r += "}";   // close root object
    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// Emit  __BREAKPOINT__:{...}\n__END_OF_STEP__\n  to stdout.
// ─────────────────────────────────────────────────────────────────────────────
inline void emitBreakpoint(numkit::DebugSession& session, numkit::Engine& engine)
{
    std::cout << "__BREAKPOINT__:" << buildBreakpointJSON(session, engine)
              << "\n__END_OF_STEP__\n";
    std::cout.flush();
}

// ─────────────────────────────────────────────────────────────────────────────
// Emit completion markers after a debug session ends (normally or on error):
//
//   [captured output]
//   __VARS__:{workspaceJSON}
//   __DEBUG_RESULT__:{"status":"completed"|"error",...}
//   __DEBUG_END__
//   __END_OF_RUN__
// ─────────────────────────────────────────────────────────────────────────────
inline void emitDebugCompletion(numkit::DebugSession& session,
                                 numkit::ExecStatus    /*status*/,
                                 numkit::Engine&       engine)
{
    // Any output produced during the final resume (may include figure markers).
    std::string output = session.takeOutput();
    if (!output.empty()) std::cout << output;

    // Workspace variables after the run
    std::cout << "__VARS__:" << engine.workspaceJSON() << "\n";

    // Status JSON — mirrors WASM buildDebugResult for completed/error cases
    std::string result;
    if (!session.errorMessage().empty()) {
        result  = "{\"status\":\"error\",\"message\":\"";
        result += detail::escapeJSON(session.errorMessage()) + "\"";
        if (session.errorLine() > 0)
            result += ",\"line\":" + std::to_string(session.errorLine());
        result += "}";
    } else {
        result = "{\"status\":\"completed\"}";
    }

    std::cout << "__DEBUG_RESULT__:" << result
              << "\n__DEBUG_END__\n__END_OF_RUN__\n";
    std::cout.flush();
}

} // namespace ide
} // namespace numkit
