// apps/numkit/main.cpp — numkit console runner (the interpreter CLI).
//
// Modes:
//   numkit_repl                  interactive REPL with multi-line input support
//   numkit_repl script.m         batch: read file, evaluate, print output, exit
//   numkit_repl --ide-session    persistent pipe session for the Numkit IDE
//   numkit_repl -h | --help      usage
//
// Builds natively (portable / desktop-fast / bench presets) and under
// Emscripten (browser / bench-wasm presets). When compiled to WASM,
// launch via Node with filesystem access:
//   node build/browser/apps/numkit/numkit_repl.js path/to/script.m

#include <numkit/core/engine.hpp>
#include <numkit/bundle/standard_engine.hpp>   // StandardEngine — Engine + standard library
#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>
#include <numkit/core/debug_session.hpp>        // DebugSession / DebugAction / ExecStatus
#include <numkit/scriptgraph/ast_serialize.hpp>
#include <numkit/scriptgraph/serialize.hpp>
#include <numkit/scriptgraph/lowering.hpp>
#include "ide_serializer.hpp"                  // Var serialisation for __INSPECT__ etc.
#include "ide_debug_serializer.hpp"             // Debug state → pipe protocol markers

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>


namespace {

using numkit::DebugAction;
using numkit::DebugSession;
using numkit::Engine;
using numkit::ExecStatus;
using numkit::Lexer;
using numkit::StandardEngine;
using numkit::TokenType;

std::string buildASTJSON(const std::string &source) {
    try {
        numkit::Lexer lex(source);
        auto tokens = lex.tokenize();
        numkit::Parser parser(tokens);
        auto root = parser.parse();
        if (!root) return "{\"error\":\"parser returned null AST\"}";
        return numkit::scriptgraph::toASTJSON(*root);
    } catch (const std::exception &e) {
        return "{\"error\":\"" + numkit::ide::detail::escapeJSON(e.what()) + "\"}";
    } catch (...) {
        return "{\"error\":\"unknown exception during AST build\"}";
    }
}

std::string buildGraphJSON(const std::string &source) {
    try {
        numkit::Lexer lex(source);
        auto tokens = lex.tokenize();
        numkit::Parser parser(tokens);
        auto root = parser.parse();
        if (!root) return "{\"error\":\"parser returned null AST\"}";
        auto g = numkit::scriptgraph::lowerScript(*root, source, tokens);
        return numkit::scriptgraph::toJSON(g);
    } catch (const std::exception &e) {
        return "{\"error\":\"" + numkit::ide::detail::escapeJSON(e.what()) + "\"}";
    } catch (...) {
        return "{\"error\":\"unknown exception during graph build\"}";
    }
}

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
        std::cerr << "numkit_repl: cannot open '" << path << "'\n";
        return 1;
    }
    std::ostringstream ss;
    ss << f.rdbuf();

    StandardEngine engine;
    auto r = engine.evalSafe(ss.str());
    if (!r.ok) {
        reportError(r, path + ": ");
        return 1;
    }
    return 0;
}

int runRepl()
{
    StandardEngine engine;
    std::cout << "numkit REPL  (type 'quit' or 'exit' to leave)\n\n";

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

// ── Native debugger session ────────────────────────────────────────────────
//
// Runs a full debug session for one code block:
//   1. Sets breakpoints on the engine (from a JSON int-array).
//   2. Starts DebugSession::start(code) — blocks until a breakpoint or completion.
//   3. On pause: emits __BREAKPOINT__:{...}\n__END_OF_STEP__\n, then reads
//      __DEBUG_CMD__:<action> commands in a loop.
//   4. On completion: emits stdout + __VARS__: + __DEBUG_RESULT__: + __DEBUG_END__
//      + __END_OF_RUN__  (so the JS side unblocks its pending promise).
//   5. On __DEBUG_CMD__:stop: emits __DEBUG_STOPPED__\n__END_OF_RUN__\n.
//
static void handleDebugSession(StandardEngine&    engine,
                                const std::string& bpJson,
                                const std::string& code)
{
    // ── 1. Set breakpoints on the engine ────────────────────────────────────
    auto& bpm = engine.debug().breakpoints();
    bpm.clearAll();
    {
        std::string s = bpJson;
        const size_t lbrace = s.find('[');
        const size_t rbrace = s.rfind(']');
        if (lbrace != std::string::npos && rbrace != std::string::npos) {
            s = s.substr(lbrace + 1, rbrace - lbrace - 1);
            std::istringstream iss(s);
            std::string tok;
            while (std::getline(iss, tok, ',')) {
                int ln = 0;
                try { ln = std::stoi(tok); } catch (...) { continue; }
                if (ln > 0) bpm.addBreakpoint(static_cast<uint16_t>(ln));
            }
        }
    }

    // ── 2. Create session and start execution ───────────────────────────────
    DebugSession dbg(engine);
    auto status = dbg.start(code);

    if (status == ExecStatus::Paused) {
        numkit::ide::emitBreakpoint(dbg, engine);

        // ── 3. Debug command loop ───────────────────────────────────────────
        std::string cmdLine;
        while (std::getline(std::cin, cmdLine)) {
            if (!cmdLine.empty() && cmdLine.back() == '\r') cmdLine.pop_back();

            // Only handle __DEBUG_CMD__: lines; skip stray blank lines.
            if (cmdLine.compare(0, 14, "__DEBUG_CMD__:") != 0) continue;
            const std::string action = cmdLine.substr(14);

            if (action == "stop") {
                dbg.stop();
                std::cout << "__DEBUG_STOPPED__\n__END_OF_RUN__\n";
                std::cout.flush();
                return;
            }

            DebugAction da;
            if      (action == "step_over") da = DebugAction::StepOver;
            else if (action == "step_into") da = DebugAction::StepInto;
            else if (action == "step_out")  da = DebugAction::StepOut;
            else                             da = DebugAction::Continue;  // "continue"

            status = dbg.resume(da);

            if (status == ExecStatus::Paused) {
                numkit::ide::emitBreakpoint(dbg, engine);
            } else {
                numkit::ide::emitDebugCompletion(dbg, status, engine);
                return;
            }
        }
        // stdin closed mid-session — clean up silently.
        dbg.stop();
        return;
    }

    // ── 4. Completed (or error) without hitting any breakpoint ──────────────
    numkit::ide::emitDebugCompletion(dbg, status, engine);
}


// IDE persistent-session mode.
//
// Protocol (stdin -> stdout):
//   IDE sends:   <code lines>\n__END_OF_INPUT__\n
//   REPL sends:  <output>\n__VARS__:{workspaceJSON}\n__END_OF_RUN__\n
//
// Introspection commands (single-line, also end with __END_OF_RUN__):
//   __INSPECT__:<name>                    getVarDataJSON  (page 0)
//   __GET_SHAPE__:<name>                  getVarShapeJSON
//   __GET_PAGE__:<name>\t<page>           getVarDataJSON(page)
//   __GET_STATS__:<name>\t<page>          getVarStatsJSON
//   __GET_TILE__:<name>\t<r0>\t<c0>\t<rows>\t<cols>\t<page>  getVarTileJSON
//   __INSPECT_PATH__:<name>\t<path>       getInspectPathJSON
//   __RESET__                             clear all
//   __QUIT__                              exit
//
// The Engine persists across calls -- workspace is preserved between runs.
int runIdeSession()
{
    StandardEngine engine;

    // Import compat.* at startup so user code finds standard functions
    // without an explicit import statement -- same as the WASM init().
    engine.evalSafe("import compat.*;");

    std::string accum;
    std::string line;

    while (std::getline(std::cin, line)) {
        // Strip trailing \r for Windows line endings over the pipe.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // ── Single-line commands ─────────────────────────────────────────────

        if (line == "__QUIT__")
            break;

        if (line == "__RESET__") {
            engine.evalSafe("clear all");
            engine.evalSafe("import compat.*;");
            std::cout << "__RESET_OK__\n"
                      << "__VARS__:" << engine.workspaceJSON() << "\n"
                      << "__END_OF_RUN__\n";
            std::cout.flush();
            accum.clear();
            continue;
        }

        // Helper: emit introspection response and continue.
        auto sendInspect = [&](const std::string& marker, const std::string& json) {
            std::cout << marker << json << "\n__END_OF_RUN__\n";
            std::cout.flush();
        };

        // __INSPECT__:<name>  — full data, page 0
        if (line.size() > 12 && line.compare(0, 12, "__INSPECT__:") == 0) {
            sendInspect("__VAR_DATA__:", numkit::ide::getVarDataJSON(engine, line.substr(12)));
            accum.clear();
            continue;
        }

        // __GET_SHAPE__:<name>
        if (line.size() > 14 && line.compare(0, 14, "__GET_SHAPE__:") == 0) {
            sendInspect("__SHAPE_DATA__:", numkit::ide::getVarShapeJSON(engine, line.substr(14)));
            accum.clear();
            continue;
        }

        // __GET_PAGE__:<name>\t<page>
        if (line.size() > 13 && line.compare(0, 13, "__GET_PAGE__:") == 0) {
            auto p = numkit::ide::parseTabParams(line, 13);
            const int pg = (p.size() > 1) ? std::stoi(p[1]) : 0;
            sendInspect("__PAGE_DATA__:", numkit::ide::getVarDataJSON(engine, p[0], pg));
            accum.clear();
            continue;
        }

        // __GET_STATS__:<name>\t<page>   (page -1 = whole array)
        if (line.size() > 14 && line.compare(0, 14, "__GET_STATS__:") == 0) {
            auto p = numkit::ide::parseTabParams(line, 14);
            const int pg = (p.size() > 1) ? std::stoi(p[1]) : -1;
            sendInspect("__STATS_DATA__:", numkit::ide::getVarStatsJSON(engine, p[0], pg));
            accum.clear();
            continue;
        }

        // __GET_FIGURE__:<name>\t<optsJSON>
        if (line.size() > 15 && line.compare(0, 15, "__GET_FIGURE__:") == 0) {
            auto p = numkit::ide::parseTabParams(line, 15);
            const std::string opts = (p.size() > 1) ? p[1] : "{}";
            sendInspect("__FIGURE_DATA__:", numkit::ide::getVarFigureJSON(engine, p[0], opts));
            accum.clear();
            continue;
        }

        // __GET_TILE__:<name>\t<r0>\t<c0>\t<rows>\t<cols>\t<page>
        if (line.size() > 13 && line.compare(0, 13, "__GET_TILE__:") == 0) {
            auto p = numkit::ide::parseTabParams(line, 13);
            const int r0   = (p.size() > 1) ? std::stoi(p[1]) : 0;
            const int c0   = (p.size() > 2) ? std::stoi(p[2]) : 0;
            const int rows = (p.size() > 3) ? std::stoi(p[3]) : 50;
            const int cols = (p.size() > 4) ? std::stoi(p[4]) : 50;
            const int pg   = (p.size() > 5) ? std::stoi(p[5]) : 0;
            sendInspect("__TILE_DATA__:",
                numkit::ide::getVarTileJSON(engine, p[0], r0, c0, rows, cols, pg));
            accum.clear();
            continue;
        }

        // __INSPECT_PATH__:<name>\t<pathStr>   (empty pathStr = root)
        if (line.size() > 17 && line.compare(0, 17, "__INSPECT_PATH__:") == 0) {
            auto p = numkit::ide::parseTabParams(line, 17);
            const std::string pathStr = (p.size() > 1) ? p[1] : "";
            sendInspect("__PATH_DATA__:", numkit::ide::getInspectPathJSON(engine, p[0], pathStr));
            accum.clear();
            continue;
        }

        // ── Code accumulation and execution ──────────────────────────────────

        // Accumulate until __END_OF_INPUT__
        if (line == "__END_OF_INPUT__") {
            if (!accum.empty()) {
                // ── Build AST ────────────────────────────────────────────────
                static constexpr size_t AST_PFX = 14; // len("__BUILD_AST__\n") == 14
                if (accum.size() >= 13 && accum.compare(0, 13, "__BUILD_AST__") == 0) {
                    const std::string code = (accum.size() > AST_PFX) ? accum.substr(AST_PFX) : "";
                    accum.clear();
                    sendInspect("__AST_DATA__:", buildASTJSON(code));
                    continue;
                }

                // ── Build Graph ──────────────────────────────────────────────
                static constexpr size_t GPH_PFX = 16; // len("__BUILD_GRAPH__\n") == 16
                if (accum.size() >= 15 && accum.compare(0, 15, "__BUILD_GRAPH__") == 0) {
                    const std::string code = (accum.size() > GPH_PFX) ? accum.substr(GPH_PFX) : "";
                    accum.clear();
                    sendInspect("__GRAPH_DATA__:", buildGraphJSON(code));
                    continue;
                }

                // ── Debug start: first line is __DEBUG_START__:[3,7,...] ──────
                static constexpr size_t DBG_PFX = 16; // len("__DEBUG_START__:") == 16
                if (accum.size() > DBG_PFX &&
                    accum.compare(0, DBG_PFX, "__DEBUG_START__:") == 0)
                {
                    const size_t nlPos = accum.find('\n');
                    const std::string bpJson = accum.substr(
                        DBG_PFX,
                        nlPos == std::string::npos ? std::string::npos
                                                   : nlPos - DBG_PFX);
                    const std::string code =
                        (nlPos != std::string::npos) ? accum.substr(nlPos + 1) : "";
                    accum.clear();
                    handleDebugSession(engine, bpJson, code);
                    continue;  // handleDebugSession already emitted __END_OF_RUN__
                }

                // ── Normal code execution ────────────────────────────────────
                auto r = engine.evalSafe(accum);
                if (!r.ok) {
                    reportError(r, "Error: ");  // → stderr (shown in console)
                    // Also emit to stdout so the IDE can highlight the failing line.
                    if (r.errorLine > 0)
                        std::cout << "__ERROR_LINE__:" << r.errorLine << "\n";
                }
                accum.clear();
            }
            // Always emit markers so the IDE can unblock.
            std::cout << "__VARS__:" << engine.workspaceJSON() << "\n"
                      << "__END_OF_RUN__\n";
            std::cout.flush();
            continue;
        }

        accum += line;
        accum += '\n';
    }

    return 0;
}


void printUsage(const char *prog)
{
    std::cout << "usage: " << prog << " [options] [script.m]\n"
              << "  (no args)          interactive REPL\n"
              << "  script.m           evaluate the file and exit\n"
              << "  --ide-session      persistent pipe session (used by the IDE)\n"
              << "  -h | --help        show this message\n";
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
    if (arg == "--ide-session")
        return runIdeSession();
    return runScript(arg);
}
