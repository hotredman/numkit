// apps/numkit_codegen/numkit_codegen_main.cpp
//
// `numkit_codegen` — the CLI front end for the codegen driver (DESIGN.md §8
// M4). It transpiles a numkit source file's entry function to C++ (the
// MATLAB-Coder-style "generate code" step). By default the emitted TU is
// written to stdout / -o; with --run it is additionally AOT-compiled with
// the external C++ compiler (env NUMKIT_CXX first, build-time fallback) and
// executed, with the program's stdout forwarded. A bridged artifact
// additionally links the nk_codegen_rt runtime; see DESIGN.md §6a — the e2e
// tests exercise that path with the build-captured runtime paths.
//
// Usage:
//   numkit_codegen <file.m> [options]
//     --args "<spec>"   entry parameter types, MATLAB-Coder -args style,
//                       comma-separated; each a dtype with optional [] for a
//                       row vector, e.g. "double[], double, double".
//     --entry <name>    entry function (default: the file's sole function)
//     --bridge          emit C-ABI calls for builtins the emitter can't lower
//     --run             transpile → AOT-compile → run; print program stdout.
//                       The compiler is NUMKIT_CXX (env) or the build-time
//                       default; the artifact is a temp file (not -o).
//     -o <file>         write the generated C++ here (default: stdout).
//                       Ignored with --run unless --emit is also given.

#include <numkit/codegen/aot.hpp>
#include <numkit/codegen/driver.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

[[noreturn]] void usage(const char *argv0, const std::string &err)
{
    std::cerr << "error: " << err << "\n\n"
              << "usage: " << argv0 << " <file.m> [--args \"<spec>\"] [--entry <name>]"
              << " [--bridge | --plugin <name>] [--run] [-o <file>]\n";
    std::exit(2);
}

std::string readFile(const std::string &path, bool &ok)
{
    std::ifstream is(path, std::ios::binary);
    if (!is) { ok = false; return {}; }
    std::ostringstream ss;
    ss << is.rdbuf();
    ok = true;
    return ss.str();
}

} // namespace

int main(int argc, char **argv)
{
    std::string file, argsSpec, entry, outPath, pluginExport;
    bool        bridge = false, runFlag = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto              next = [&](const char *flag) -> std::string {
            if (i + 1 >= argc) usage(argv[0], std::string("missing value for ") + flag);
            return argv[++i];
        };
        if (a == "--args") argsSpec = next("--args");
        else if (a == "--entry") entry = next("--entry");
        else if (a == "-o") outPath = next("-o");
        else if (a == "--bridge") bridge = true;
        else if (a == "--plugin") pluginExport = next("--plugin");
        else if (a == "--run") runFlag = true;
        else if (a == "-h" || a == "--help") usage(argv[0], "help");
        else if (!a.empty() && a[0] == '-') usage(argv[0], "unknown option " + a);
        else if (file.empty()) file = a;
        else usage(argv[0], "unexpected extra argument " + a);
    }
    if (file.empty()) usage(argv[0], "no input file");
    if (runFlag && !pluginExport.empty())
        usage(argv[0], "--run and --plugin are mutually exclusive (a plugin TU is "
                       "a shared lib, not a runnable program)");

    bool ok = false;
    const std::string source = readFile(file, ok);
    if (!ok) { std::cerr << "error: cannot read " << file << "\n"; return 1; }

    if (bridge && !pluginExport.empty())
        usage(argv[0], "--plugin and --bridge are mutually exclusive (a plugin TU is "
                       "self-contained-compiled, not bridged)");

    try {
        namespace cg = numkit::codegen;
        const auto  paramTypes = cg::driver::parseTypeSpec(argsSpec);

        std::string generated;  // the C++ TU to write
        std::string info;       // a one-line note to stderr (keeps -o clean)
        cg::EmittedFunction emittedFn;  // kept for --run's harness (non-plugin path)
        if (!pluginExport.empty()) {
            generated = cg::driver::transpileToPlugin(source, entry, paramTypes, pluginExport,
                                                      "nk_plugin.h");
            info      = "emitted plugin registering '" + pluginExport + "'";
        } else {
            cg::BridgeOptions bridgeOpts;
            bridgeOpts.enabled       = bridge;
            bridgeOpts.runtimeHeader = "nk_codegen_rt.h";
            emittedFn = cg::driver::transpileSource(source, entry, paramTypes, bridgeOpts);
            generated = emittedFn.source;
            info      = "emitted entry '" + emittedFn.name + "': " + emittedFn.signature;
        }

        // --run: wrap the emitted TU in a main() harness, AOT-compile it, and
        // execute it. The artifact is a temp file so -o (the C++ source path)
        // and the binary don't collide. v1 requires a nullary entry returning a
        // scalar / complex / void — buildHarnessMain throws otherwise.
        if (runFlag) {
            if (!cg::aot::available())
                throw std::runtime_error(
                    "no C++ compiler configured — set NUMKIT_CXX or rebuild with a "
                    "build-time compiler (see DESIGN.md §8)");
            if (!pluginExport.empty())
                throw std::runtime_error("--run is incompatible with --plugin");
            if (!paramTypes.empty())
                throw std::runtime_error(
                    "--run: v1 supports only a nullary entry (no --args). For "
                    "functions with args, emit the C++ with -o and write your "
                    "own main() harness.");
            const std::string program = generated + cg::driver::buildHarnessMain(emittedFn);

            namespace fs = std::filesystem;
            const auto base = fs::temp_directory_path() / "numkit_codegen_run";
            fs::create_directories(base);
            std::string exe = (base / "aot").string();
#ifdef _WIN32
            exe += ".exe";
#endif
            const auto cr = cg::aot::compileToExecutable(program, exe);
            if (!cr.ok())
                throw std::runtime_error("compilation failed.\n--- compiler log ---\n" +
                                         cr.log + "\n--- end log ---");
            std::cerr << "numkit_codegen: compiled -> " << exe << "\n";
            const auto rr = cg::aot::runExecutable(exe);
            std::error_code ec;
            fs::remove(exe, ec); fs::remove(exe + ".cpp", ec); fs::remove(exe + ".log", ec);
#ifdef _WIN32
            fs::remove(exe + ".obj", ec); fs::remove(exe + ".build.bat", ec);
#endif
            std::cout << rr.log;
            if (rr.exitCode != 0) {
                std::cerr << "numkit_codegen: program exited with code " << rr.exitCode << "\n";
                return rr.exitCode;
            }
            return 0;
        }

        if (outPath.empty()) {
            std::cout << generated;
        } else {
            std::ofstream os(outPath, std::ios::binary);
            if (!os) { std::cerr << "error: cannot open " << outPath << "\n"; return 1; }
            os << generated;
            os.flush();
            if (!os) { std::cerr << "error: failed writing " << outPath << "\n"; return 1; }
        }
        std::cerr << "numkit_codegen: " << info << "\n";  // stderr, so -o stays clean
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
