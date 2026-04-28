// example/main.cpp — numkit-m console runner.
//
// Two modes:
//   numkit_example               interactive REPL with multi-line input support
//   numkit_example script.m      batch: read file, evaluate, print output, exit
//   numkit_example -h | --help   usage
//
// Builds natively (portable / desktop-fast / bench presets) and under
// Emscripten (browser / bench-wasm presets). When compiled to WASM,
// launch via Node with filesystem access:
//   node build-browser/example/numkit_example.js path/to/script.m

#include <numkit/core/engine.hpp>
#include <numkit/core/lexer.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

using numkit::Engine;
using numkit::Lexer;
using numkit::TokenType;

// Decide whether the accumulated buffer is still waiting for more input.
// Runs the lexer, balances brackets and block-keywords, and also honours
// a trailing `...` continuation marker. Used in REPL mode so the user
// can type blocks like `for i = 1:5 ... end` across multiple lines.
bool isIncompleteInput(const std::string &src)
{
    int bracket = 0;
    int blockOpen = 0;
    try {
        Lexer lex(src);
        const auto toks = lex.tokenize();
        for (const auto &t : toks) {
            switch (t.type) {
            case TokenType::LPAREN:
            case TokenType::LBRACKET:
            case TokenType::LBRACE:
                ++bracket;
                break;
            case TokenType::RPAREN:
            case TokenType::RBRACKET:
            case TokenType::RBRACE:
                --bracket;
                break;
            case TokenType::KW_IF:
            case TokenType::KW_FOR:
            case TokenType::KW_WHILE:
            case TokenType::KW_FUNCTION:
            case TokenType::KW_SWITCH:
            case TokenType::KW_TRY:
                ++blockOpen;
                break;
            case TokenType::KW_END:
                // MATLAB uses `end` both for block closers and as an
                // index sentinel (x(end)). Only count it as a block
                // closer at the outermost bracket level.
                if (bracket == 0)
                    --blockOpen;
                break;
            default:
                break;
            }
        }
    } catch (...) {
        // Lexer threw — usually an unterminated string literal. Keep
        // reading; the user will close it on a subsequent line or ^C.
        return true;
    }

    if (bracket > 0 || blockOpen > 0)
        return true;

    // Trailing `...` (after optional whitespace/newlines) is MATLAB's
    // explicit line-continuation marker.
    auto isWs = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    size_t i = src.size();
    while (i > 0 && isWs(src[i - 1]))
        --i;
    if (i >= 3 && src.compare(i - 3, 3, "...") == 0)
        return true;

    return false;
}

void reportError(const Engine::EvalResult &r, const std::string &prefix)
{
    std::string ctx = r.errorContext.empty() ? "" : " (" + r.errorContext + ")";
    if (r.errorLine > 0)
        std::cerr << prefix << "line " << r.errorLine << ", column " << r.errorCol
                  << ": " << r.errorMessage << ctx << "\n";
    else
        std::cerr << prefix << r.errorMessage << ctx << "\n";
}

int runScript(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "numkit_example: cannot open '" << path << "'\n";
        return 1;
    }
    std::ostringstream ss;
    ss << f.rdbuf();

    Engine engine;
    auto r = engine.evalSafe(ss.str());
    if (!r.ok) {
        reportError(r, path + ": ");
        return 1;
    }
    return 0;
}

int runRepl()
{
    Engine engine;
    std::cout << "numkit-m REPL  (type 'quit' or 'exit' to leave)\n\n";

    std::string accum;
    std::string line;
    while (true) {
        std::cout << (accum.empty() ? ">> " : "... ");
        std::cout.flush();
        if (!std::getline(std::cin, line))
            break;

        if (accum.empty()) {
            if (line == "quit" || line == "exit")
                break;
            if (line.empty())
                continue;
        }

        accum += line;
        accum += '\n';

        if (isIncompleteInput(accum))
            continue;

        auto r = engine.evalSafe(accum);
        if (!r.ok)
            reportError(r, "Error: ");
        accum.clear();
    }

    std::cout << "\nGoodbye!\n";
    return 0;
}

void printUsage(const char *prog)
{
    std::cout << "usage: " << prog << " [script.m]\n"
              << "  (no args)        interactive REPL\n"
              << "  script.m         evaluate the file and exit\n"
              << "  -h | --help      show this message\n";
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 1)
        return runRepl();

    const std::string arg = argv[1];
    if (arg == "-h" || arg == "--help") {
        printUsage(argv[0]);
        return 0;
    }
    return runScript(arg);
}
