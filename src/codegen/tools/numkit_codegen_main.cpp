// codegen/tools/numkit_codegen_main.cpp
//
// `numkit_codegen` — the CLI front end for the codegen driver (DESIGN.md §8
// M4). It transpiles a numkit source file's entry function to C++ (the
// MATLAB-Coder-style "generate code" step). Compilation of the emitted TU is
// left to the user's toolchain (a bridged artifact additionally links the
// nk_codegen_rt runtime; see DESIGN.md §6a) — the e2e tests exercise that
// path with the build-captured runtime paths.
//
// Usage:
//   numkit_codegen <file.m> [options]
//     --args "<spec>"   entry parameter types, MATLAB-Coder -args style,
//                       comma-separated; each a dtype with optional [] for a
//                       row vector, e.g. "double[], double, double".
//     --entry <name>    entry function (default: the file's sole function)
//     --bridge          emit C-ABI calls for builtins the emitter can't lower
//     -o <file>         write the generated C++ here (default: stdout)

#include <numkit/codegen/driver.hpp>

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
              << " [--bridge] [-o <file>]\n";
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
    std::string file, argsSpec, entry, outPath;
    bool        bridge = false;

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
        else if (a == "-h" || a == "--help") usage(argv[0], "help");
        else if (!a.empty() && a[0] == '-') usage(argv[0], "unknown option " + a);
        else if (file.empty()) file = a;
        else usage(argv[0], "unexpected extra argument " + a);
    }
    if (file.empty()) usage(argv[0], "no input file");

    bool ok = false;
    const std::string source = readFile(file, ok);
    if (!ok) { std::cerr << "error: cannot read " << file << "\n"; return 1; }

    try {
        const auto paramTypes = numkit::codegen::driver::parseTypeSpec(argsSpec);
        numkit::codegen::BridgeOptions bridgeOpts;
        bridgeOpts.enabled       = bridge;
        bridgeOpts.runtimeHeader = "nk_codegen_rt.h";
        const numkit::codegen::EmittedFunction em =
            numkit::codegen::driver::transpileSource(source, entry, paramTypes, bridgeOpts);

        if (outPath.empty()) {
            std::cout << em.source;
        } else {
            std::ofstream os(outPath, std::ios::binary);
            if (!os) { std::cerr << "error: cannot open " << outPath << "\n"; return 1; }
            os << em.source;
            os.flush();
            if (!os) { std::cerr << "error: failed writing " << outPath << "\n"; return 1; }
        }
        // The emitted entry symbol + signature, to stderr so -o stays clean.
        std::cerr << "numkit_codegen: emitted entry '" << em.name << "': " << em.signature << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
