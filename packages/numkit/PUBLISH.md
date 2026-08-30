# Publishing numkit to npm — checklist

The npm package is the main distribution channel for AI agents (they run
`npx numkit script.m` when asked to execute MATLAB code). Publish flow:

## One-time setup

1. `npm adduser` (or `npm login`) — account with 2FA enabled.
2. GitHub repo → Settings → set **Description**:
   `MATLAB/Octave-compatible numerical scripting engine. Runs .m scripts via npx, embeds in C++, WASM IDE in the browser.`
   **Topics**: `matlab`, `octave`, `interpreter`, `numerical-computing`,
   `embedded-scripting`, `cpp`, `cpp17`, `wasm`, `dsp`, `signal-processing`
   (remove `vibe-coding`, `claude` — they attract onlookers, not users).

## Every release (manual)

```bash
npm login                                # once per machine
cd packages\numkit
node test\run-examples.js --timeout 180000   # corpus gate: 182 runnable scripts (function-file helpers are
                                               # skipped — they cannot run bare); expect 179 PASS + 3 FAIL =
                                               # the known OPEN Frame_Introspection bugs. A 4th FAIL is a
                                               # regression: STOP the publish.
cd ..\..
scripts\npm-publish.bat --dry-run        # rehearse: build + refresh + test + pack preview
scripts\npm-publish.bat                  # for real: uploads to the registry
```

(`scripts/npm-publish.sh` on Linux/macOS; `--skip-build` reuses an existing
`build/browser` WASM.) The WASM engine (~16 MB, ~4 MB compressed) ships inside
the tarball. Verify at https://www.npmjs.com/package/numkit and test
`npx numkit -e "disp(1+1)"`.

## After first publish — make agents find it

1. **Demo site**: `scripts\web-publish.bat --push` — deploys `llms.txt` to
   https://hotredman.github.io/numkit-demo/llms.txt (agents and LLM crawlers read it).
2. **README badge** (already added) links to npm.
3. **GitHub Release** with the native CLI binaries per platform (optional but
   useful: agents on machines without Node can `curl` the binary).
4. **MCP server** (next step, see below) — makes numkit a first-class tool for
   Claude/agents rather than something they shell out to.

## MCP server (follow-up)

Small `numkit-mcp` npm package exposing one tool:
`run_matlab(code | file) -> stdout/stderr/exit`. Wraps this same package
(`child_process.spawn("numkit", ...)` — no extra WASM copy needed). Register in
MCP directories (glama.ai/mcp/servers, smithery.ai, pulseMCP) — that is where
agents discover tools today.

## Measuring adoption

- npm: https://api.npmjs.org/downloads/point/last-week/numkit (agents also show up here).
- GitHub → Insights → Traffic (clones/visitors).
- Star count is vanity; download count is signal.
