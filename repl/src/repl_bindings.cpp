#include <emscripten/bind.h>
#include <string>
#include <sstream>
#include <memory>
#include <iostream>

#include "MLabEngine.hpp"
#include "MLabStdLibrary.hpp"

class ReplSession {
public:
    ReplSession() {
        engine_ = std::make_unique<mlab::Engine>();
        mlab::StdLibrary::install(*engine_);
        engine_->setOutputFunc([this](const std::string &s) {
            outputBuf_ += s;
        });
    }

    std::string execute(const std::string& code) {
        outputBuf_.clear();

        // Capture stdout for __PLOT_DATA__ markers
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

        try {
            engine_->eval(code);
            std::string output = collectOutput();
            while (!output.empty() &&
                   (output.back() == '\n' || output.back() == ' '))
                output.pop_back();
            return output;

        } catch (const mlab::MLabError& e) {
            std::string output = collectOutput();
            if (!output.empty() && output.back() != '\n')
                output += '\n';
            if (e.line() > 0) {
                output += "__ERROR_LINE__:" + std::to_string(e.line()) + "\n";
                output += "Error (line " + std::to_string(e.line()) + "): " + e.what();
            } else {
                output += std::string("Error: ") + e.what();
            }
            return output;

        } catch (const std::exception& e) {
            std::string output = collectOutput();
            if (!output.empty() && output.back() != '\n')
                output += '\n';
            output += std::string("Error: ") + e.what();
            return output;

        } catch (...) {
            std::cout.rdbuf(oldCout);
            return "Error: Unknown exception";
        }
    }

    void reset() {
        engine_ = std::make_unique<mlab::Engine>();
        mlab::StdLibrary::install(*engine_);
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

EMSCRIPTEN_BINDINGS(mlab_repl) {
    emscripten::function("repl_init",      &repl_init);
    emscripten::function("repl_execute",   &repl_execute);
    emscripten::function("repl_complete",  &repl_complete);
    emscripten::function("repl_reset",     &repl_reset);
    emscripten::function("repl_workspace", &repl_workspace);
    emscripten::function("repl_get_vars",  &repl_get_vars);
}
