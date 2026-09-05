// Refresh the packaged WASM engine from the repo's latest browser build.
// Run automatically on `npm publish` (prepublishOnly); can also be run by hand.
// Keeps packages/numkit/dist out of git — it is a build product, like ide/dist.

"use strict";

const fs = require("fs");
const path = require("path");

const repoRoot = path.resolve(__dirname, "..", "..", "..");
const srcDir = path.join(repoRoot, "build", "browser", "wasm", "dist");
const outDir = path.join(__dirname, "..", "dist");

const files = ["numkit_ide.js", "numkit_ide.wasm"];

// Staleness guard: the wasm must not predate the last commit touching
// engine sources. mtime-based comparison false-positives after
// rebase/checkout (they touch every file), so use git committer time.
// A silent stale copy once shipped an outdated engine; this protects a
// raw `npm publish` (the documented flow rebuilds via
// scripts/npm-publish.sh). Opt out explicitly with --allow-stale.
function lastSrcCommitTime() {
  try {
    const { execSync } = require("child_process");
    const out = execSync("git log -1 --format=%cI -- src", {
      cwd: repoRoot,
      encoding: "utf8",
      stdio: ["ignore", "pipe", "ignore"],
    }).trim();
    return out ? new Date(out).getTime() : 0;
  } catch {
    return 0; // no git → cannot judge; the missing-source check still applies
  }
}

if (!process.argv.includes("--allow-stale")) {
  const wasm = path.join(srcDir, "numkit_ide.wasm");
  const wasmMtime = fs.statSync(wasm).mtimeMs;
  const srcCommit = lastSrcCommitTime();
  if (srcCommit > wasmMtime) {
    console.error(
      `numkit: STALE ENGINE — src/ has commits newer than the wasm build:\n` +
        `  last src commit: ${new Date(srcCommit).toISOString()}\n` +
        `  wasm build:      ${new Date(wasmMtime).toISOString()}\n` +
        `Rebuild first:  scripts\\windows\\engine-build.cmd --wasm  (or scripts/linux/engine-build.sh --wasm)\n` +
        `Override only deliberately:  node scripts/refresh-dist.js --allow-stale`
    );
    process.exit(1);
  }
}

fs.mkdirSync(outDir, { recursive: true });
for (const f of files) {
  const src = path.join(srcDir, f);
  if (!fs.existsSync(src)) {
    console.error(
      `numkit: missing ${src}\n` +
        `Build the WASM engine first:  scripts\\windows\\web-build.cmd  (or scripts/linux/web-build.sh)\n` +
        `then re-run npm publish from packages/numkit.`
    );
    process.exit(1);
  }
  fs.copyFileSync(src, path.join(outDir, f));
  console.log(`numkit: refreshed dist/${f}`);
}
