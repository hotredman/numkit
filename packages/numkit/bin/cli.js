#!/usr/bin/env node
// numkit CLI — run MATLAB/Octave-compatible scripts via the numkit WASM engine.
//
//   numkit script.m              evaluate the file, print output, exit
//   numkit -e "A = [1 2; 3 4]; det(A)"
//   numkit --version | --help
//
// The engine keeps a single ReplSession for the whole invocation, so a script
// can define functions and call them later in the same file. Errors are
// detected via the `__ERROR_LINE__:` marker the engine appends on failure.

"use strict";

const path = require("path");
const fs = require("fs");

const DIST_DIR = path.join(__dirname, "..", "dist");

function usage() {
  return [
    "numkit — MATLAB/Octave-compatible numerical scripting (WebAssembly)",
    "",
    "Usage:",
    "  numkit script.m             evaluate the file and exit",
    "  numkit -e \"<code>\"          evaluate inline code",
    "  numkit --version            print engine version",
    "  numkit -h | --help          show this message",
    "",
    "Examples:",
    "  numkit -e \"A = [1 2; 3 4]; x = A \\ [5; 11]; disp(x)\"",
    "  numkit examples/fft_demo.m",
  ].join("\n");
}

function parseArgs(argv) {
  const args = { file: null, eval: null, help: false, version: false };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "-h" || a === "--help") args.help = true;
    else if (a === "--version" || a === "-v") args.version = true;
    else if (a === "-e" || a === "--eval") {
      if (i + 1 >= argv.length) fail("option -e requires an argument");
      args.eval = argv[++i];
    } else if (a.startsWith("-")) {
      fail(`unknown option: ${a}`);
    } else if (args.file === null) {
      args.file = a;
    } else {
      fail(`unexpected extra argument: ${a}`);
    }
  }
  return args;
}

function fail(msg) {
  process.stderr.write(`numkit: ${msg}\n`);
  process.exit(2);
}

async function loadEngine() {
  const factory = require(path.join(DIST_DIR, "numkit_ide.js"));
  const mod = await factory({
    // Emscripten resolves the .wasm next to the loader by default only in
    // browsers; in Node point it at the packaged dist directory explicitly.
    locateFile: (name) => path.join(DIST_DIR, name),
  });
  mod.repl_init();
  registerNativeFs(mod);
  // Make relative file I/O (csvwrite, save, audiowrite, …) land on the real
  // disk instead of the in-memory default filesystem — MATLAB-like CLI
  // semantics: relative paths resolve against the process cwd.
  mod.repl_execute("setenv('NUMKIT_FS', 'native');");
  return mod;
}

// ── Real-filesystem bridge ─────────────────────────────────────────────────
//
// The engine's WASM build normally sees only an in-memory filesystem (what
// the browser IDE uses). For a CLI that is wrong: `run('helper.m')` and
// sibling-file resolution must read the actual disk, and csvwrite('x.csv')
// must produce a file the caller can see. The engine exposes a synchronous
// callback FS for exactly this (the same bridge the IDE uses for local
// folders), so back it with Node's fs.
//
// Path rules: absolute paths pass through (separators normalised to '/'),
// relative paths resolve against the process cwd, falling back to the
// run-script's directory — so both `numkit main.m` and scripts that read
// sibling data files work regardless of the caller's cwd.

let scriptDir = null; // directory of the .m file being run, or null

function fsResolve(p, mode) {
  p = String(p).replace(/\\/g, "/");
  // An already-absolute path is never engine-prefixed: the engine's
  // resolvePath treats drive-letter paths as absolute on every platform
  // (fs_context.cpp) and passes them through untouched. Pass them through
  // too — the scriptDir strip below must not fire for a clean absolute
  // target that merely LIES INSIDE the script's directory (it used to
  // relocate such saves to the process cwd).
  if (/^([A-Za-z]:\/|\/|\\\\)/.test(p)) return p;
  // The engine prefixes unprefixed lookups with the pushed script-origin dir
  // itself ("<scriptDir>/<scriptDir>/name.m" when scriptDir is absolute) —
  // it expects the FS to be rooted elsewhere. Detect the doubled prefix and
  // strip it so absolute script dirs work.
  if (scriptDir) {
    const norm = scriptDir.replace(/\\/g, "/") + "/";
    while (p.startsWith(norm)) {
      const rest = p.slice(norm.length);
      // MATLAB semantics: data files (csv/mat/wav/…) resolve against the
      // CALLER'S cwd; only sibling .m lookups belong to the script's dir.
      // The restored remainder is absolute — return it; re-testing the loop
      // would strip the prefix a second time when the target lies inside
      // scriptDir and relocate the write to the process cwd.
      if (/^([A-Za-z]:\/|\/)/.test(rest)) return rest; // doubled prefix
      if (mode === "read" && rest.endsWith(".m")) return norm + rest; // sibling script
      p = rest; // data path → resolve from cwd below
      break;
    }
  }
  if (/^([A-Za-z]:\/|\/|\\\\)/.test(p)) return p;
  const cwdRel = path.resolve(process.cwd(), p);
  if (fs.existsSync(cwdRel)) return cwdRel;
  if (mode === "read" && scriptDir) {
    const dirRel = path.resolve(scriptDir, p);
    if (fs.existsSync(dirRel)) return dirRel;
  }
  return cwdRel; // not found: return the cwd path so the error names it
}

function registerNativeFs(mod) {
  mod.repl_register_fs("native", {
    exists: (p) => fs.existsSync(fsResolve(p, "read")),
    readFile: (p) => fs.readFileSync(fsResolve(p, "read"), "utf8"),
    readFileBytes: (p) => new Uint8Array(fs.readFileSync(fsResolve(p, "read"))),
    writeFile: (p, c) => fs.writeFileSync(fsResolve(p, "write"), c),
    writeFileBytes: (p, arr) =>
      fs.writeFileSync(fsResolve(p, "write"), Buffer.from(arr.buffer, arr.byteOffset, arr.byteLength)),
  });
}

// repl_execute returns one string for both stdout and errors. Runtime errors
// carry the `__ERROR_LINE__:<n>` marker; parse errors only end with a bare
// `Error: ...` / `Error (line n): ...` final line, so accept both shapes.
function failedAt(out) {
  const idx = out.indexOf("__ERROR_LINE__:");
  if (idx !== -1) return idx;
  const m = out.match(/(^|\n)Error( \(line \d+\))?:[^\n]*$/);
  return m ? out.length - m[0].length + (m[1] ? 1 : 0) : -1;
}

// MATLAB run() semantics for FUNCTION files: a file whose first code
// construct is a function definition EXECUTES its primary function when
// run (nullary runs; required inputs fail with the natural argument
// error) — mirrors numkit::runtime::primaryFunctionOfFile
// (bugs/opened/lang/run-invokes-nullary-function-file.md).
function primaryFunctionOfFile(src) {
  for (const raw of src.split("\n")) {
    let line = raw.replace("\r", "");
    const pct = line.indexOf("%");
    if (pct !== -1) line = line.slice(0, pct);
    line = line.trim();
    if (!line) continue;
    const m = line.match(/^function\s+(?:\[[^\]]*\]\s*=\s*|[A-Za-z_]\w*\s*=\s*)?([A-Za-z_]\w*)\s*(?:\(|$)/);
    return m ? m[1] : null;
  }
  return null;
}

// The REPL emits IDE-protocol sentinels (__FIGURE_CLOSE_ALL__, __CLEAR__,
// …) meant for the browser IDE; a plain CLI consumer must not print them
// (bugs/opened/apps/wasm-cli-ide-markers.md).
function stripIdeMarkers(out) {
  return out
    .split("\n")
    // Bare IDE-protocol sentinels (__CLEAR__, …) and the figure payload
    // channel (__FIGURE_DATA__:{…} — megabytes of JSON per figure) are
    // for the IDE, not a plain CLI consumer's stdout
    // (bugs/closed/apps/wasm-cli-figure-data-leak.md).
    .filter((l) => !/^__[A-Z_]+__($|:)/.test(l.trim()))
    .join("\n");
}

function runCode(mod, code, label) {
  const out = stripIdeMarkers(mod.repl_execute(code));
  const idx = failedAt(out);
  if (idx !== -1) {
    process.stderr.write(out.slice(idx) + `\n    at ${label}\n`);
    process.exitCode = 1;
  } else if (out.length > 0) {
    process.stdout.write(out + "\n");
  }
  // A FUNCTION file runs its primary function after being defined.
  const primaryFn = primaryFunctionOfFile(code);
  if (primaryFn) {
    const out2 = stripIdeMarkers(mod.repl_execute(primaryFn + ";"));
    const idx2 = failedAt(out2);
    if (idx2 !== -1) {
      process.stderr.write(out2.slice(idx2) + `\n    at ${label}\n`);
      process.exitCode = 1;
    } else if (out2.length > 0) {
      process.stdout.write(out2 + "\n");
    }
  }
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.help) return process.stdout.write(usage() + "\n");

  const mod = await loadEngine();

  if (args.version) {
    // Package version (from package.json) + engine build stamp — a bug
    // report must map to both the npm release and the engine build.
    const pkg = require(path.join(__dirname, "..", "package.json"));
    process.stdout.write(`numkit ${pkg.version} (engine build ${mod.repl_version()})\n`);
    return;
  }
  if (args.eval !== null) runCode(mod, args.eval, "command line");
  if (args.file !== null) {
    let src;
    try {
      src = fs.readFileSync(args.file, "utf8");
    } catch (e) {
      fail(`cannot read ${args.file}: ${e.message}`);
    }
    // Tell the engine where the script lives so sibling .m files resolve by
    // short name (MATLAB convention), through the native FS registered above.
    scriptDir = path.dirname(path.resolve(args.file));
    if (typeof mod.repl_push_script_origin_with_dir === "function")
      mod.repl_push_script_origin_with_dir("native", scriptDir.replace(/\\/g, "/"));
    runCode(mod, src, args.file);
  }
  if (args.eval === null && args.file === null && !args.version) {
    // No input: print usage, mirroring the native CLI's behaviour.
    process.stdout.write(usage() + "\n");
  }
}

main().catch((e) => {
  process.stderr.write(`numkit: ${e && e.stack ? e.stack : e}\n`);
  process.exit(1);
});
