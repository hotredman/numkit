#include <emscripten/bind.h>
#include <string>
#include <sstream>
#include <memory>
#include <iostream>

#include "MLabEngine.hpp"
#include "MLabStdLibrary.hpp"

// ════════════════════════════════════════════════
// Перехват stdout/stderr
// ════════════════════════════════════════════════

class OutputCapture {
public:
    OutputCapture() {
        old_cout_ = std::cout.rdbuf(cout_buf_.rdbuf());
        old_cerr_ = std::cerr.rdbuf(cerr_buf_.rdbuf());
    }

    ~OutputCapture() { restore(); }

    void restore() {
        if (old_cout_) { std::cout.rdbuf(old_cout_); old_cout_ = nullptr; }
        if (old_cerr_) { std::cerr.rdbuf(old_cerr_); old_cerr_ = nullptr; }
    }

    std::string getAll() const {
        std::string result = cout_buf_.str();
        std::string err = cerr_buf_.str();
        if (!err.empty()) {
            if (!result.empty() && result.back() != '\n') result += '\n';
            result += err;
        }
        return result;
    }

private:
    std::ostringstream cout_buf_;
    std::ostringstream cerr_buf_;
    std::streambuf* old_cout_ = nullptr;
    std::streambuf* old_cerr_ = nullptr;
};

// ════════════════════════════════════════════════
// Сессия REPL
// ════════════════════════════════════════════════

class ReplSession {
public:
    ReplSession() {
        engine_ = std::make_unique<mlab::Engine>();
        mlab::StdLibrary::install(*engine_);
    }

    std::string execute(const std::string& code) {
        OutputCapture capture;
        try {
            engine_->eval(code);
            capture.restore();

            std::string output = capture.getAll();

            // Убираем trailing whitespace
            while (!output.empty() &&
                   (output.back() == '\n' || output.back() == ' '))
                output.pop_back();

            return output;

        } catch (const std::exception& e) {
            capture.restore();

            std::string output = capture.getAll();
            if (!output.empty() && output.back() != '\n')
                output += '\n';
            output += std::string("Error: ") + e.what();
            return output;

        } catch (...) {
            capture.restore();
            return "Error: Unknown exception";
        }
    }

    void reset() {
        engine_ = std::make_unique<mlab::Engine>();
        mlab::StdLibrary::install(*engine_);
    }

    std::string getWorkspace() {
        // Попробуем выполнить who/whos если реализовано
        OutputCapture capture;
        try {
            engine_->eval("whos");
            capture.restore();
            std::string out = capture.getAll();
            if (!out.empty()) return out;
        } catch (...) {
            capture.restore();
        }

        // Fallback
        return "Workspace inspection not available.\nUse 'disp(varname)' to check variables.";
    }

    std::string complete(const std::string& partial) {
        if (partial.empty()) return "";

        static const char* keywords[] = {
            // Ключевые слова
            "break", "case", "catch", "continue",
            "else", "elseif", "end", "for", "function",
            "global", "if", "otherwise",
            "return", "switch", "try", "while",
            // Матрицы
            "zeros", "ones", "eye", "rand", "randn",
            "linspace", "logspace", "reshape",
            "size", "length", "numel",
            // Математика
            "sin", "cos", "tan", "asin", "acos", "atan",
            "exp", "log", "log2", "log10", "sqrt", "abs", "sign",
            "floor", "ceil", "round", "mod", "rem", "pow",
            // Статистика
            "min", "max", "sum", "prod", "mean",
            "cumsum", "sort",
            // Комплексные
            "real", "imag", "conj",
            // Строки
            "upper", "lower", "strcmp", "strcmpi",
            "strcat", "strsplit",
            // Вывод
            "disp", "fprintf", "sprintf", "num2str",
            // Утилиты
            "clear", "clc", "who", "whos",
            "true", "false", "pi", "inf", "nan", "eps",
            "isempty", "isnumeric", "ischar",
            "help",
            nullptr
        };

        std::string result;
        for (int i = 0; keywords[i]; ++i) {
            const char* kw = keywords[i];
            // Проверяем prefix match
            bool match = true;
            for (size_t j = 0; j < partial.size(); ++j) {
                if (kw[j] == '\0' || kw[j] != partial[j]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                if (!result.empty()) result += ',';
                result += kw;
            }
        }
        return result;
    }

private:
    std::unique_ptr<mlab::Engine> engine_;
};

// ════════════════════════════════════════════════
// Глобальная сессия и экспортируемые функции
// ════════════════════════════════════════════════

static std::unique_ptr<ReplSession> g_session;

std::string repl_init() {
    g_session = std::make_unique<ReplSession>();
    return "MATLAB Interpreter v1.0\n"
           "Type commands below. Enter to execute.\n"
           "Shift+Enter for multiline. Tab for autocomplete.\n";
}

std::string repl_execute(const std::string& input) {
    if (!g_session) repl_init();

    // trim
    size_t start = input.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = input.find_last_not_of(" \t\n\r");
    std::string trimmed = input.substr(start, end - start + 1);
    if (trimmed.empty()) return "";

    if (trimmed == "clc") return "__CLEAR__";

    if (trimmed == "help") {
        return "Commands:\n"
               "  clc        - Clear screen\n"
               "  clear      - Clear workspace\n"
               "  who/whos   - List variables\n"
               "  help       - This message\n"
               "\n"
               "Keys:\n"
               "  Enter        Execute\n"
               "  Shift+Enter  New line\n"
               "  Tab          Autocomplete\n"
               "  Up/Down      History\n"
               "  Ctrl+L       Clear screen\n"
               "  Ctrl+C       Cancel input";
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

EMSCRIPTEN_BINDINGS(matlab_repl) {
    emscripten::function("repl_init",      &repl_init);
    emscripten::function("repl_execute",   &repl_execute);
    emscripten::function("repl_complete",  &repl_complete);
    emscripten::function("repl_reset",     &repl_reset);
    emscripten::function("repl_workspace", &repl_workspace);
}