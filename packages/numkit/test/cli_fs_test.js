#!/usr/bin/env node
// CLI filesystem-bridge regression guard
// (bugs/opened/apps/wasm-cli-abs-save-relocated-to-cwd.md).
//
// A script run by ABSOLUTE path from a foreign cwd must write an
// absolute-path save() target exactly where it says — not relocate it to
// the process cwd (the old doubled-prefix double-strip bug).
//
//   node packages/numkit/test/cli_fs_test.js   → exit 0 = pass, 1 = fail

"use strict";

const fs = require("fs");
const os = require("os");
const path = require("path");
const { execFileSync } = require("child_process");

const CLI = path.join(__dirname, "..", "bin", "cli.js");

function run() {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), "numkit-cli-fs-"));
  try {
    const dirA = path.join(root, "a"); // where the script lives
    const dirB = path.join(root, "b"); // foreign cwd to invoke from
    fs.mkdirSync(dirA);
    fs.mkdirSync(dirB);

    const target = path.join(dirA, "out.mat"); // abs target INSIDE scriptDir
    const script = path.join(dirA, "s.m");
    fs.writeFileSync(script, `save('${target.replace(/\\/g, "/")}');\n`);

    execFileSync(process.execPath, [CLI, script], { cwd: dirB, stdio: "pipe" });

    const inA = fs.existsSync(target);
    const inB = fs.existsSync(path.join(dirB, "out.mat"));
    if (!inA || inB) {
      console.error(
        `FAIL: save target inside scriptDir relocated — inA=${inA} inB(inB)=${inB}`
      );
      process.exit(1);
    }
    console.log("PASS: absolute-path save inside scriptDir lands at its target");
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
}

run();

// Figure payload must not leak into CLI stdout (wasm-cli-figure-data-leak).
{
  const out = execFileSync(process.execPath,
    [CLI, "-e", "x = 1:10; plot(x, x.^2); disp('ok')"],
    { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
  if (out.includes("__FIGURE_DATA__")) {
    console.error("FAIL: __FIGURE_DATA__ payload leaked into CLI stdout");
    process.exit(1);
  }
  if (!out.trim().endsWith("ok")) {
    console.error("FAIL: expected script output missing, got: " + JSON.stringify(out));
    process.exit(1);
  }
  console.log("PASS: figure payload stays off CLI stdout");
}
