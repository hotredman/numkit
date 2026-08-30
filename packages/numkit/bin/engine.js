// packages/numkit/bin/engine.js
//
// Shared engine loader — used by both the CLI (bin/cli.js) and the MCP
// server (packages/numkit-mcp/bin/mcp.js). Loads the WASM module, bridges
// the real filesystem, and provides the error-marker parser.

"use strict";

const fs = require("fs");
const path = require("path");

const DIST_DIR = path.join(__dirname, "..", "dist");

let scriptDir = null; // directory of the .m file being run, or null

function setScriptDir(dir) { scriptDir = dir; }

function fsResolve(p, mode) {
  p = String(p).replace(/\\/g, "/");
  if (scriptDir) {
    const norm = scriptDir.replace(/\\/g, "/") + "/";
    while (p.startsWith(norm)) {
      const rest = p.slice(norm.length);
      if (/^([A-Za-z]:\/|\/)/.test(rest)) { p = rest; continue; }
      if (mode === "read" && rest.endsWith(".m")) return norm + rest;
      p = rest;
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
  return cwdRel;
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

// Strip IDE-protocol sentinel lines (a CLI consumer must not print them).
function stripIdeMarkers(out) {
  return out
    .split("\n")
    .filter((l) => !/^__[A-Z_]+__$/.test(l.trim()))
    .join("\n");
}

// Detect the error marker in a repl_execute result; returns the index or -1.
function failedAt(out) {
  const idx = out.indexOf("__ERROR_LINE__:");
  if (idx !== -1) return idx;
  const m = out.match(/(^|\n)Error( \(line \d+\))?:[^\n]*$/);
  return m ? out.length - m[0].length + (m[1] ? 1 : 0) : -1;
}

// MATLAB run() semantics for FUNCTION files.
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

async function loadEngine() {
  const factory = require(path.join(DIST_DIR, "numkit_ide.js"));
  const mod = await factory({
    locateFile: (name) => path.join(DIST_DIR, name),
  });
  mod.repl_init();
  registerNativeFs(mod);
  mod.repl_execute("setenv('NUMKIT_FS', 'native');");
  return mod;
}

module.exports = {
  loadEngine,
  stripIdeMarkers,
  failedAt,
  primaryFunctionOfFile,
  setScriptDir,
  DIST_DIR,
};
