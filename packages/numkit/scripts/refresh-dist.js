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

fs.mkdirSync(outDir, { recursive: true });
for (const f of files) {
  const src = path.join(srcDir, f);
  if (!fs.existsSync(src)) {
    console.error(
      `numkit: missing ${src}\n` +
        `Build the WASM engine first:  scripts\\web-build.bat  (or scripts/web-build.sh)\n` +
        `then re-run npm publish from packages/numkit.`
    );
    process.exit(1);
  }
  fs.copyFileSync(src, path.join(outDir, f));
  console.log(`numkit: refreshed dist/${f}`);
}
