#include <emscripten/bind.h>
#include <string>
#include <sstream>
#include <memory>
#include <iostream>
#include <vector>
#include <set>
#include <cmath>

#include "MLabEngine.hpp"
#include "MLabStdLibrary.hpp"
#include "MLabDebugSession.hpp"

// ════════════════════════════════════════════════════════════════
// Helper: format MValue for variable preview
// ════════════════════════════════════════════════════════════════
static std::string valuePreview(const mlab::MValue &val) {
    using mlab::MType;
    try {
        if (val.isScalar()) {
            if (val.type() == MType::DOUBLE) {
                double v = val.toScalar();
                if (std::isnan(v)) return "NaN";
                if (std::isinf(v)) return v > 0 ? "Inf" : "-Inf";
                if (v == static_cast<int64_t>(v) && std::abs(v) < 1e15)
                    return std::to_string(static_cast<int64_t>(v));
                std::ostringstream os; os << v; return os.str();
            }
            if (val.type() == MType::LOGICAL)
                return val.toBool() ? "true" : "false";
            if (val.type() == MType::COMPLEX) {
                auto c = val.toComplex();
                std::ostringstream os;
                os << c.real();
                if (c.imag() >= 0) os << "+";
                os << c.imag() << "i";
                return os.str();
            }
        }
        if (val.type() == MType::CHAR)
            return "'" + val.toString() + "'";
        auto &d = val.dims();
        std::ostringstream os;
        os << "[" << d.rows() << "x" << d.cols();
        if (d.is3D()) os << "x" << d.pages();
        os << " " << mlab::mtypeName(val.type()) << "]";
        if (val.type() == MType::DOUBLE && val.numel() <= 10) {
            os << " [";
            for (size_t i = 0; i < val.numel(); ++i) {
                if (i) os << " ";
                double v = val.doubleData()[i];
                if (v == static_cast<int64_t>(v) && std::abs(v) < 1e15)
                    os << static_cast<int64_t>(v);
                else
                    os << v;
            }
            os << "]";
        }
        return os.str();
    } catch (...) {
        return "<error>";
    }
}

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

// ════════════════════════════════════════════════════════════════
// ReplSession
// ════════════════════════════════════════════════════════════════
class ReplSession {
public:
    ReplSession() {
        engine_ = std::make_unique<mlab::Engine>();
        restoreOutputFunc();
    }

    std::string execute(const std::string& code) {
        // During active debug session, evaluate in the current frame context
        if (debugSession_ && debugSession_->isActive()) {
            return debugEval(code);
        }

        outputBuf_.clear();

        auto r = engine_->evalSafe(code);

        std::string output = outputBuf_;

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

    // Evaluate expression in debug context (saves/restores VM paused state)
    std::string debugEval(const std::string &code) {
        return debugSession_->eval(code);
    }

    // ── Debug API (clean, no replay) ──

    std::string debugStart(const std::string &code) {
        debugSession_ = std::make_unique<mlab::DebugSession>(*engine_);

        // Set breakpoints from saved list
        debugSession_->setBreakpoints(breakpointLines_);

        auto status = debugSession_->start(code);
        return buildDebugResult(status);
    }

    std::string debugResume(int action) {
        if (!debugSession_ || !debugSession_->isActive())
            return "{\"status\":\"completed\"}";

        auto da = static_cast<mlab::DebugAction>(action);
        auto status = debugSession_->resume(da);
        return buildDebugResult(status);
    }

    void debugStop() {
        if (debugSession_)
            debugSession_->stop();
        debugSession_.reset();
        restoreOutputFunc();
    }

    void setBreakpoints(const std::string &linesJson) {
        breakpointLines_.clear();

        // Parse simple JSON array: [1, 5, 10]
        std::string s = linesJson;
        size_t start = s.find('[');
        size_t end = s.rfind(']');
        if (start == std::string::npos || end == std::string::npos) return;
        s = s.substr(start + 1, end - start - 1);

        std::istringstream iss(s);
        std::string token;
        while (std::getline(iss, token, ',')) {
            int line = 0;
            try { line = std::stoi(token); } catch (...) { continue; }
            if (line > 0) breakpointLines_.push_back(static_cast<uint16_t>(line));
        }

        // Also update engine's breakpoint manager for legacy path
        auto &bpm = engine_->breakpointManager();
        bpm.clearAll();
        for (auto line : breakpointLines_)
            bpm.addBreakpoint(line);
    }

    void reset() {
        debugSession_.reset();
        engine_ = std::make_unique<mlab::Engine>();
        restoreOutputFunc();
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

    void restoreOutputFunc() {
        engine_->setOutputFunc([this](const std::string &s) { outputBuf_ += s; });
    }

private:
    std::unique_ptr<mlab::Engine> engine_;
    std::string outputBuf_;
    std::unique_ptr<mlab::DebugSession> debugSession_;
    std::vector<uint16_t> breakpointLines_;

    std::string buildDebugResult(mlab::ExecStatus status) {
        std::string output = debugSession_ ? debugSession_->takeOutput() : "";
        std::string result;

        if (status == mlab::ExecStatus::Paused) {
            auto snap = debugSession_->snapshot();

            // Determine pause reason: breakpoint or step
            bool onBreakpoint = engine_->breakpointManager().shouldBreak(snap.line);
            const char *reason = onBreakpoint ? "breakpoint" : "step";

            result = "{\"status\":\"paused\",\"pauseState\":{";
            result += "\"line\":" + std::to_string(snap.line);
            result += ",\"col\":" + std::to_string(snap.col);
            result += ",\"function\":\"" + escapeJSON(snap.functionName) + "\"";
            result += ",\"reason\":\"" + std::string(reason) + "\"";

            // Variables
            result += ",\"variables\":{";
            bool first = true;
            for (auto &v : snap.variables) {
                if (!v.value) continue;
                if (!first) result += ",";
                result += "\"" + escapeJSON(v.name) + "\":\"" + escapeJSON(valuePreview(*v.value)) + "\"";
                first = false;
            }
            result += "}";

            // Call stack
            result += ",\"callStack\":[";
            for (size_t i = 0; i < snap.callStack.size(); ++i) {
                if (i > 0) result += ",";
                auto &sf = snap.callStack[i];
                result += "{\"function\":\"" + escapeJSON(sf.functionName) + "\"";
                result += ",\"line\":" + std::to_string(sf.line) + "}";
            }
            result += "]";

            result += "}";
            if (!output.empty())
                result += ",\"output\":\"" + escapeJSON(output) + "\"";
            result += "}";
        } else {
            // Completed or error
            if (debugSession_ && !debugSession_->errorMessage().empty()) {
                result = "{\"status\":\"error\",\"message\":\"" +
                         escapeJSON(debugSession_->errorMessage()) + "\"";
                if (debugSession_->errorLine() > 0)
                    result += ",\"line\":" + std::to_string(debugSession_->errorLine());
                if (!output.empty())
                    result += ",\"output\":\"" + escapeJSON(output) + "\"";
                result += "}";
            } else {
                result = "{\"status\":\"completed\"";
                if (!output.empty())
                    result += ",\"output\":\"" + escapeJSON(output) + "\"";
                result += "}";
            }
            debugSession_.reset();
            restoreOutputFunc();
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

// ── Debug API (clean — no replay) ──

void repl_debug_set_breakpoints(const std::string &linesJson) {
    if (!g_session) repl_init();
    g_session->setBreakpoints(linesJson);
}

std::string repl_debug_start(const std::string &code) {
    if (!g_session) repl_init();
    return g_session->debugStart(code);
}

std::string repl_debug_resume(int action) {
    if (!g_session) return "{\"status\":\"completed\"}";
    return g_session->debugResume(action);
}

void repl_debug_stop() {
    if (g_session) g_session->debugStop();
}

// Legacy API (kept for backward compat, delegates to new API)
std::string repl_debug_execute(const std::string &code, int skipBp) {
    if (!g_session) repl_init();
    if (skipBp == 0) {
        // Fresh start
        return g_session->debugStart(code);
    } else {
        // Continue (legacy: skipBp > 0 means "continue from last pause")
        return g_session->debugResume(0); // 0 = Continue
    }
}

EMSCRIPTEN_BINDINGS(mlab_repl) {
    emscripten::function("repl_init",      &repl_init);
    emscripten::function("repl_execute",   &repl_execute);
    emscripten::function("repl_complete",  &repl_complete);
    emscripten::function("repl_reset",     &repl_reset);
    emscripten::function("repl_workspace", &repl_workspace);
    emscripten::function("repl_get_vars",  &repl_get_vars);
    emscripten::function("repl_debug_set_breakpoints", &repl_debug_set_breakpoints);
    emscripten::function("repl_debug_start",           &repl_debug_start);
    emscripten::function("repl_debug_resume",          &repl_debug_resume);
    emscripten::function("repl_debug_stop",            &repl_debug_stop);
    // Legacy (kept for backward compat)
    emscripten::function("repl_debug_execute",         &repl_debug_execute);
}
