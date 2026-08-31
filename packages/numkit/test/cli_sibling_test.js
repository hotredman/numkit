#!/usr/bin/env node
// RED guard for bugs/opened/lang/run-abs-path-sibling-resolution.md
// (deliberately failing while the bug is open — the DISABLED_-gtest
// equivalent at the CLI level; not wired into any default gate).
//
// A wrapper script that does run('<ABS>/main.m') must resolve main.m's
// sibling FUNCTION files exactly like the relative run('main.m') does.
//
//   node packages/numkit/test/cli_sibling_test.js
//     → exit 1 + "KNOWN BUG" while open; exit 0 once fixed (live guard).

"use strict";

const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const CLI = path.join(__dirname, "..", "bin", "cli.js");

function run() {
  const dirA = fs.mkdtempSync(path.join(os.tmpdir(), "numkit-sib-"));
  try {
    fs.writeFileSync(path.join(dirA, "helper.m"),
      "function z = helper(x)\nz = 2 * x;\nend\n");
    fs.writeFileSync(path.join(dirA, "main.m"), "disp(helper(21));\n");
    fs.writeFileSync(path.join(dirA, "wrap_rel.m"), "run('main.m');\n");
    const abs = path.join(dirA, "main.m").replace(/\\/g, "/");
    fs.writeFileSync(path.join(dirA, "wrap_abs.m"), `run('${abs}');\n`);

    const rel = execFileSync(process.execPath, [CLI, "wrap_rel.m"],
      { cwd: dirA, encoding: "utf8" }).trim();
    if (rel !== "42") {
      console.error(`FAIL: relative wrapper unexpectedly printed ${JSON.stringify(rel)}`);
      process.exit(1);
    }

    const absOut = execFileSync(process.execPath, [CLI, "wrap_abs.m"],
      { cwd: dirA, encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] }).trim();
    if (absOut !== "42") {
      console.error(`FAIL: abs wrapper printed ${JSON.stringify(absOut)} (expected 42)`);
      process.exit(1);
    }
    console.log("PASS: run(<abs>) resolves sibling functions");
  } catch (e) {
    const msg = (e.stderr || e.message || "").toString();
    if (/undefined function 'helper'/.test(msg)) {
      console.error("KNOWN BUG (open): bugs/opened/lang/run-abs-path-sibling-resolution.md");
      console.error("  run(abs) loses sibling-function resolution through the native-FS bridge");
      process.exit(1);
    }
    console.error("FAIL: " + msg.split("\n")[0]);
    process.exit(1);
  } finally {
    fs.rmSync(dirA, { recursive: true, force: true });
  }
}

run();
