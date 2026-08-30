# apps.numkit_repl — script error still exits with code 0

- **Status:** 🔴 OPEN
- **Severity:** P3 minor/style (P1 for scripting/CI consumers — see below)
- **Found:** 2026-08-29 via npm CLI corpus run (comparing native vs WASM CLI behaviour)

## Symptom
`numkit_repl script.m` reports a script error on stderr but the process exit
code is 0, so shells / CI / AI agents cannot detect failure without parsing
output.

## Repro
```bash
./numkit_repl.exe examples/Frame_Introspection/assignin_setter.m
# numkit:  …line 39…: Undefined function or variable 'cfg_one'
# $LASTEXITCODE / $?: 0        ← should be 1
```

## Notes
- The WASM CLI in `packages/numkit/bin/cli.js` already returns exit code 1 on
  `__ERROR_LINE__` / trailing-`Error:` output — use that contract when fixing
  the native app (`apps/numkit/main.cpp`): batch mode should map a failed
  `evalSafe` to a nonzero exit.

## References
- **Guard:** deferred — CLI exit-code semantics; checked by fieldtest corpus scripts.
