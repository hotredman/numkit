// Corpus runner: executes every repo example through the packaged CLI exactly
// the way an external consumer (human or AI agent) would:
//   node bin/cli.js <script.m>
//
// Usage: node test/run-examples.js [examplesDir] [--concurrency N] [--timeout ms]
//   --json     print full results as JSON (default: summary + failures only)
//
// Exit status: 0 if everything passed, 1 otherwise.

"use strict";

const { spawn } = require("child_process");
const path = require("path");
const fs = require("fs");
const os = require("os");

const PKG_DIR = path.resolve(__dirname, "..");
const CLI = path.join(PKG_DIR, "bin", "cli.js");

const argv = process.argv.slice(2);
const flags = {
  json: argv.includes("--json"),
  concurrency: intFlag("--concurrency", 6),
  timeout: intFlag("--timeout", 25000),
};
const positional = argv.filter(
  (a, i) => !a.startsWith("--") && !["--concurrency", "--timeout"].includes(argv[i - 1])
);
const examplesDir = path.resolve(
  positional[0] || path.resolve(PKG_DIR, "..", "..", "examples")
);

function intFlag(name, dflt) {
  const i = argv.indexOf(name);
  return i !== -1 && argv[i + 1] ? parseInt(argv[i + 1], 10) : dflt;
}

function listScripts(dir) {
  const out = [];
  (function walk(d) {
    for (const e of fs.readdirSync(d, { withFileTypes: true })) {
      if (e.isDirectory()) walk(path.join(d, e.name));
      else if (e.name.endsWith(".m")) out.push(path.join(d, e.name));
    }
  })(dir);
  return out.sort().filter((f) => {
    // FUNCTION files are helpers, not standalone scripts — running one
    // bare (correctly, since the CLI now invokes a function file's
    // primary function) fails with "not enough input arguments" exactly
    // like MATLAB. They are not corpus cases; skip them.
    const src = fs.readFileSync(f, "utf8");
    const NL = String.fromCharCode(10);
    const CR = String.fromCharCode(13);
    for (const raw of src.split(NL)) {
      let line = raw.split(CR).join("");
      const pct = line.indexOf("%");
      if (pct !== -1) line = line.slice(0, pct);
      line = line.trim();
      if (!line) continue;
      return !/^function\s/.test(line);
    }
    return true;
  });
}

function runOne(file) {
  return new Promise((resolve) => {
    const t0 = Date.now();
    // Each script gets its own empty cwd: examples that WRITE files
    // (csvwrite, save, …) land in scratch space instead of the repo and
    // cannot collide when running in parallel.
    const scratch = fs.mkdtempSync(path.join(os.tmpdir(), "numkit-corpus-"));
    const p = spawn(process.execPath, [CLI, file], { cwd: scratch });
    let out = "", err = "";
    const timer = setTimeout(() => p.kill("SIGKILL"), flags.timeout);
    p.stdout.on("data", (d) => (out += d));
    p.stderr.on("data", (d) => (err += d));
    p.on("close", (code, signal) => {
      clearTimeout(timer);
      fs.rmSync(scratch, { recursive: true, force: true });
      resolve({
        file: path.relative(examplesDir, file).replace(/\\/g, "/"),
        status: signal ? "timeout" : code === 0 ? "pass" : "fail",
        exitCode: code,
        ms: Date.now() - t0,
        stdoutLen: out.length,
        stderrTail: err.slice(-400).trim(),
        stdoutTail: out.slice(-200).trim(),
      });
    });
  });
}

async function main() {
  const files = listScripts(examplesDir);
  const results = [];
  let done = 0;
  const queue = files.slice();
  async function worker() {
    while (queue.length) {
      const f = queue.shift();
      const r = await runOne(f);
      results.push(r);
      process.stderr.write(
        `\r[${++done}/${files.length}] ${r.status.padEnd(7)} ${r.file.slice(0, 60).padEnd(60)}`
      );
    }
  }
  await Promise.all(Array.from({ length: flags.concurrency }, worker));
  process.stderr.write("\n\n");

  results.sort((a, b) => a.file.localeCompare(b.file));
  const by = (s) => results.filter((r) => r.status === s);
  const cat = {};
  for (const r of results) {
    const c = r.file.split("/")[0];
    cat[c] = cat[c] || { total: 0, pass: 0 };
    cat[c].total++;
    if (r.status === "pass") cat[c].pass++;
  }

  if (flags.json) {
    console.log(JSON.stringify({ examplesDir, count: results.length, results }, null, 1));
  } else {
    console.log(`Corpus: ${results.length} scripts from ${examplesDir}\n`);
    console.log("PASS " + by("pass").length + "   FAIL " + by("fail").length + "   TIMEOUT " + by("timeout").length + "\n");
    console.log("Per category:");
    for (const c of Object.keys(cat).sort())
      console.log(
        `  ${c.padEnd(24)} ${String(cat[c].pass).padStart(3)}/${String(cat[c].total).padStart(3)}`
      );
    const bad = results.filter((r) => r.status !== "pass");
    if (bad.length) {
      console.log("\nFailures & timeouts (stderr tail):");
      for (const r of bad) {
        console.log(`\n[${r.status}] ${r.file}  (${r.ms} ms, exit=${r.exitCode})`);
        console.log("  " + (r.stderrTail || r.stdoutTail || "<no output>").replace(/\n/g, "\n  "));
      }
    }
  }
  process.exit(by("pass").length === results.length ? 0 : 1);
}

main();
