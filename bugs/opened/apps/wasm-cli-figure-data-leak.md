# apps.cli — `__FIGURE_DATA__:{...}` payloads leak into CLI stdout (bare-marker filter doesn't match payload lines)

- **Status:** 🔴 OPEN
- **Severity:** P3 (cosmetic/CLI UX — megabytes of JSON on the terminal for plotting scripts; breaks stdout-consuming pipelines)
- **Kind:** bug
- **Found:** 2026-08-31 while enabling plotting scripts in the fieldtest corpus

## Symptom

Any plotting script run through the WASM CLI prints its full figure payload
(one huge JSON line per figure) to stdout. The IDE-marker fix
(bugs/closed/apps/wasm-cli-ide-markers.md) filters lines matching
`^__[A-Z_]+__$` — the figure-data line has a `:` payload and slips through.

## Repro (self-contained)

```bash
numkit -e "x = 1:10; plot(x, x.^2); disp('done')"
# numkit: __FIGURE_DATA__:{"id":1,...huge JSON...}
#         done
# MATLAB R2025b (-batch): done        (figures create no stdout)
```

## Root cause

`stripIdeMarkers` in `packages/numkit/bin/cli.js` drops bare sentinel lines
only; `__FIGURE_DATA__:<json>` keeps its payload and is printed as ordinary
output.

## Suggested fix

Extend the CLI filter: drop lines starting with `__FIGURE_DATA__:` (the
payload is for the IDE protocol channel; a CLI consumer that wants figures
can request them explicitly, e.g. a future `--figures` flag printing to
stderr or a file).

## References

- **Guard:** deferred — CLI stdout cosmetic; extend
  `packages/numkit/test/cli_fs_test.js` with a no-figure-payload-on-stdout
  check when fixed (the existing JS-guard home).
