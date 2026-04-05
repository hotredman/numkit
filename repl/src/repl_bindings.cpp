#include <emscripten/bind.h>
#include <string>
#include <sstream>
#include <memory>
#include <iostream>
#include <vector>
#include <set>

#include "MLabEngine.hpp"
#include "MLabStdLibrary.hpp"
#include "MLabDebugger.hpp"

// ════════════════════════════════════════════════════════════════
// WasmDebugObserver — captures pause state for JS consumption
// ════════════════════════════════════════════════════════════════
class WasmDebugObserver : public mlab::DebugObserver {
public:
    // State captured when execution pauses
    struct PauseState {
        uint16_t line = 0;
        uint16_t col = 0;
        std::string functionName;
        std::string reason; // "breakpoint", "step", "entry"
        std::vector<std::pair<std::string, std::string>> variables; // name → preview
    };

    bool paused = false;
    PauseState pauseState;
    int breakpointHitCount = 0; // how many breakpoints we've hit
    int skipBreakpoints = 0;    // how many breakpoints to skip (for continue)

    mlab::DebugAction onLine(const mlab::DebugContext &ctx) override {
        // In run-to-breakpoint mode: don't stop on regular lines.
        // The initial onLine (from reset(StepInto)) just sets us to Continue.
        (void)ctx;
        return mlab::DebugAction::Continue;
    }

    mlab::DebugAction onBreakpoint(const mlab::DebugContext &ctx) override {
        breakpointHitCount++;
        if (breakpointHitCount <= skipBreakpoints) {
            return mlab::DebugAction::Continue;
        }
        capturePauseState(ctx, "breakpoint");
        paused = true;
        return mlab::DebugAction::Stop;
    }

    void onError(const mlab::DebugContext &ctx, const std::string &msg) override {
        (void)ctx; (void)msg;
    }

    void onFunctionEntry(const mlab::DebugContext &ctx) override {
        (void)ctx;
    }

    void onFunctionExit(const mlab::DebugContext &ctx) override {
        (void)ctx;
    }

    std::string pauseStateJSON() const {
        std::ostringstream ss;
        ss << "{\"line\":" << pauseState.line
           << ",\"col\":" << pauseState.col
           << ",\"function\":\"" << escapedString(pauseState.functionName) << "\""
           << ",\"reason\":\"" << pauseState.reason << "\""
           << ",\"variables\":{";
        bool first = true;
        for (auto &[name, preview] : pauseState.variables) {
            if (!first) ss << ",";
            ss << "\"" << escapedString(name) << "\":\"" << escapedString(preview) << "\"";
            first = false;
        }
        ss << "}}";
        return ss.str();
    }

private:
    void capturePauseState(const mlab::DebugContext &ctx, const std::string &reason) {
        pauseState.line = ctx.line;
        pauseState.col = ctx.col;
        pauseState.functionName = ctx.functionName ? *ctx.functionName : "<unknown>";
        pauseState.reason = reason;
        pauseState.variables.clear();

        // Capture variables from current frame
        if (auto *frame = ctx.currentFrame()) {
            auto vars = frame->variables();
            for (auto &v : vars) {
                if (v.value) {
                    pauseState.variables.emplace_back(v.name, v.value->toString());
                }
            }
        }
    }

    static std::string escapedString(const std::string &s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c;
            }
        }
        return result;
    }
};

// ════════════════════════════════════════════════════════════════
// ReplSession
// ════════════════════════════════════════════════════════════════
class ReplSession {
public:
    ReplSession() {
        engine_ = std::make_unique<mlab::Engine>();
        engine_->setOutputFunc([this](const std::string &s) {
            outputBuf_ += s;
        });
    }

    std::string execute(const std::string& code) {
        outputBuf_.clear();

        std::ostringstream coutCapture;
        auto oldCout = std::cout.rdbuf(coutCapture.rdbuf());

        auto r = engine_->evalSafe(code);

        std::cout.rdbuf(oldCout);
        std::string output = outputBuf_;
        std::string coutStr = coutCapture.str();
        if (!coutStr.empty()) {
            if (!output.empty() && output.back() != '\n') output += '\n';
            output += coutStr;
        }

        if (r.ok) {
            while (!output.empty() &&
                   (output.back() == '\n' || output.back() == ' '))
                output.pop_back();
            return output;
        }

        if (!output.empty() && output.back() != '\n')
            output += '\n';
        if (r.errorLine > 0) {
            output += "__ERROR_LINE__:" + std::to_string(r.errorLine) + "\n";
            output += "Error (line " + std::to_string(r.errorLine) + "): " + r.errorMessage;
            if (!r.errorContext.empty())
                output += " (" + r.errorContext + ")";
        } else {
            output += "Error: " + r.errorMessage;
            if (!r.errorContext.empty())
                output += " (" + r.errorContext + ")";
        }
        return output;
    }

    // ── Debug execution ──
    // Returns JSON: { "status": "paused"|"completed"|"error", "line": N, ... }
    std::string debugExecute(const std::string &code, int skipBp = 0) {
        outputBuf_.clear();

        auto observer = std::make_shared<WasmDebugObserver>();
        observer->skipBreakpoints = skipBp;
        observer->breakpointHitCount = 0;
        observer->paused = false;

        engine_->setDebugObserver(observer);

        // Set initial action: if breakpoints exist, Continue (run to bp).
        // If no breakpoints, we shouldn't stop on every line.
        // The controller defaults to StepInto, but our observer returns Stop on onLine.
        // So we need Continue mode — breakpoints only.
        // We achieve this by having reset() called with Continue inside the engine.

        std::ostringstream coutCapture;
        auto oldCout = std::cout.rdbuf(coutCapture.rdbuf());

        auto collectOutput = [&]() -> std::string {
            std::cout.rdbuf(oldCout);
            std::string output = outputBuf_;
            std::string coutStr = coutCapture.str();
            if (!coutStr.empty()) {
                if (!output.empty() && output.back() != '\n') output += '\n';
                output += coutStr;
            }
            return output;
        };

        auto r = engine_->evalSafe(code);
        std::string output = collectOutput();
        std::string result;

        if (r.ok) {
            result = "{\"status\":\"completed\"";
            if (!output.empty()) {
                result += ",\"output\":\"" + escapeJSON(output) + "\"";
            }
            result += "}";
        } else if (r.debugStop) {
            if (observer->paused) {
                result = "{\"status\":\"paused\","
                         "\"pauseState\":" + observer->pauseStateJSON() + ","
                         "\"breakpointHitCount\":" + std::to_string(observer->breakpointHitCount);
                if (!output.empty()) {
                    result += ",\"output\":\"" + escapeJSON(output) + "\"";
                }
                result += "}";
            } else {
                result = "{\"status\":\"stopped\"}";
            }
        } else {
            result = "{\"status\":\"error\",\"message\":\"" + escapeJSON(r.errorMessage) + "\"";
            if (r.errorLine > 0) {
                result += ",\"line\":" + std::to_string(r.errorLine);
                result += ",\"col\":" + std::to_string(r.errorCol);
            }
            if (!r.errorContext.empty()) {
                result += ",\"context\":\"" + escapeJSON(r.errorContext) + "\"";
            }
            if (!output.empty()) {
                result += ",\"output\":\"" + escapeJSON(output) + "\"";
            }
            result += "}";
        }

        // Detach observer after execution
        engine_->setDebugObserver(nullptr);
        return result;
    }

    void setBreakpoints(const std::string &linesJson) {
        auto &bpm = engine_->breakpointManager();
        bpm.clearAll();

        // Parse simple JSON array: [1, 5, 10]
        std::string s = linesJson;
        // Remove [ ]
        size_t start = s.find('[');
        size_t end = s.rfind(']');
        if (start == std::string::npos || end == std::string::npos) return;
        s = s.substr(start + 1, end - start - 1);

        // Split by comma
        std::istringstream iss(s);
        std::string token;
        while (std::getline(iss, token, ',')) {
            int line = 0;
            try { line = std::stoi(token); } catch (...) { continue; }
            if (line > 0) bpm.addBreakpoint(static_cast<uint16_t>(line));
        }
    }

    void reset() {
        engine_ = std::make_unique<mlab::Engine>();
        engine_->setOutputFunc([this](const std::string &s) {
            outputBuf_ += s;
        });
    }

    std::string getWorkspace() {
        outputBuf_.clear();
        try {
            engine_->eval("whos");
            std::string out = outputBuf_;
            if (!out.empty()) return out;
        } catch (...) {}
        return "No variables in workspace.";
    }

    std::string getWorkspaceJSON() {
        try {
            return engine_->workspaceJSON();
        } catch (...) {
            return "{}";
        }
    }

    std::string complete(const std::string& partial) {
        if (partial.empty()) return "";
        static const char* keywords[] = {
            "break","case","catch","continue","else","elseif","end",
            "for","function","global","if","otherwise","return",
            "switch","try","while",
            "zeros","ones","eye","rand","randn","linspace","logspace",
            "reshape","meshgrid","size","length","numel",
            "sin","cos","tan","asin","acos","atan","atan2",
            "exp","log","log2","log10","sqrt","abs","sign",
            "floor","ceil","round","mod","rem","pow",
            "min","max","sum","prod","mean","cumsum","sort",
            "real","imag","conj","deg2rad","rad2deg",
            "upper","lower","strcmp","strcmpi","strcat","strsplit",
            "disp","fprintf","sprintf","num2str",
            "clear","clc","who","whos",
            "true","false","pi","inf","nan","eps",
            "isempty","isnumeric","ischar",
            "plot","bar","scatter","hist","figure","subplot",
            "title","xlabel","ylabel","zlabel","legend",
            "grid","hold","axis","view","close","help",
            nullptr
        };
        std::string result;
        for (int i = 0; keywords[i]; ++i) {
            const char* kw = keywords[i];
            if (std::string(kw).substr(0, partial.size()) == partial) {
                if (!result.empty()) result += ',';
                result += kw;
            }
        }
        return result;
    }

private:
    std::unique_ptr<mlab::Engine> engine_;
    std::string outputBuf_;

    static std::string escapeJSON(const std::string &s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) {
            switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
            }
        }
        return result;
    }
};

static std::unique_ptr<ReplSession> g_session;

std::string repl_init() {
    g_session = std::make_unique<ReplSession>();
    return "MLab Interpreter v2.2\nType commands below.";
}

std::string repl_execute(const std::string& input) {
    if (!g_session) repl_init();
    size_t start = input.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = input.find_last_not_of(" \t\n\r");
    std::string trimmed = input.substr(start, end - start + 1);
    if (trimmed.empty()) return "";
    if (trimmed == "clc") return "__CLEAR__";
    if (trimmed == "help") {
        return "Commands: clc, clear, who, whos, help\n"
               "Keys: Enter=exec, Shift+Enter=newline, Tab=autocomplete";
    }
    return g_session->execute(trimmed);
}

std::string repl_complete(const std::string& partial) {
    if (!g_session) return "";
    return g_session->complete(partial);
}

std::string repl_reset() {
    if (g_session) g_session->reset();
    return "Workspace cleared.";
}

std::string repl_workspace() {
    if (!g_session) return "No active session.";
    return g_session->getWorkspace();
}

std::string repl_get_vars() {
    if (!g_session) return "{}";
    return "__VARS__:" + g_session->getWorkspaceJSON();
}

// ── Debug API ──

void repl_debug_set_breakpoints(const std::string &linesJson) {
    if (!g_session) repl_init();
    g_session->setBreakpoints(linesJson);
}

std::string repl_debug_execute(const std::string &code, int skipBp) {
    if (!g_session) repl_init();
    return g_session->debugExecute(code, skipBp);
}

EMSCRIPTEN_BINDINGS(mlab_repl) {
    emscripten::function("repl_init",      &repl_init);
    emscripten::function("repl_execute",   &repl_execute);
    emscripten::function("repl_complete",  &repl_complete);
    emscripten::function("repl_reset",     &repl_reset);
    emscripten::function("repl_workspace", &repl_workspace);
    emscripten::function("repl_get_vars",  &repl_get_vars);
    emscripten::function("repl_debug_set_breakpoints", &repl_debug_set_breakpoints);
    emscripten::function("repl_debug_execute",         &repl_debug_execute);
}
