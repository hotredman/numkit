# fieldtest/ — real-world differential testing vs MATLAB

Testing numkit against **real third-party MATLAB code** (GitHub), run through
both MATLAB R2025b and numkit (WASM CLI + native REPL), with every divergence
classified and routed into the repo's bug protocol. This is deliberately
complementary to:

- `tools/parity/` — *spec-driven* parity (curated calls, MATLAB R2025b ground
  truth, fingerprint diff);
- `examples/` — the *synthetic* corpus (written by the engine's authors —
  blind spots correlate). Fieldtest exists because synthetic corpora share the
  author's blind spots; real code does not.

## Layout

```
fieldtest/
  README.md        this file — the method + the workflow rules
  sources.list     curated list of tested projects (URL + license + purpose)
  fetch.py         clone/update sources.list into corpus/ + prepare work copy
  harness.py       dual-run (MATLAB vs numkit WASM vs native), classify, report
  signprobe.py     signature check: `help fn` (arg list) diff, MATLAB vs numkit
  reports/         committed run reports (JSON + summary per batch)
  corpus/          GITIGNORED — cloned repos (corpus/repos/) and the prepared
                   work copy (corpus/work/). Third-party code NEVER enters git.
```

## Workflow

1. **Fetch** — `python fetch.py` clones/updates everything in `sources.list`
   into `corpus/repos/`, then prepares `corpus/work/`: a UTF-8-transcoded
   mirror (many real-world files are GBK; transcoding gives BOTH engines the
   same text — MATLAB on a non-Chinese locale garbles GBK just like numkit
   does, so the comparison stays fair).

2. **Run** — `python harness.py harvest [N]` selects deterministic
   self-contained scripts that print output; `python harness.py run [N]
   [filter]` executes each through the three engines (timeout-protected,
   determinism-probed by a double numkit run), fuzzy-diffs output (numeric
   tokens compared at rel 1e-6), and writes `reports/` + a verdict per script:

   | verdict | meaning | action |
   |---|---|---|
   | `pass` | outputs match numerically | record timings (speed stats) |
   | `runtime-error` / `output-mismatch` | numkit fails or diverges where MATLAB succeeds | **file a bug** (see below) |
   | `parse-error` | numkit's parser rejects valid MATLAB | **file a bug** |
   | `absent-fn` | function not implemented | add to `bugs/missing.md` (a parity gap, NOT a defect per the Kind legend) |
   | `both-error` | both engines error | eyeball: same construct → error-parity PASS; different → bug on the diverging side |
   | `numkit-hang` / `matlab-timeout` | hang | bug (hang) |
   | `nondeterministic` | double-run differs | exclude, note in report |

3. **File every finding.** Runtime/parse divergence → `bugs/opened/<ns>/<fn>.md`
   (Kind: bug) **+ a reproducing gtest** per the repo protocol (`DISABLED_`
   until fixed, live after). Absent function → a row in `missing.md`.
   Signature divergence → `bugs/opened/<ns>/<fn>-signature.md` (Kind: bug).

4. **Signature audit** — `python signprobe.py <fn> [<fn>…]` captures MATLAB's
   documented signature (`help fn`) and numkit's, diffs them, and reports the
   first divergent line. Any arg-list mismatch (missing option, different
   default, different nargout shape) becomes a signature bug. Batch-audit the
   functions a corpus batch actually used:
   `python signprobe.py $(python harness.py used-fns <report>)`.

## Sources policy

- Permissive licenses only (MIT / BSD / Apache-2.0) — recorded per line in
  `sources.list`.
- `corpus/` is gitignored; third-party code is never committed, shipped, or
  redistributed — only executed for testing. Reports contain paths + verdicts,
  not third-party source text.

## Speed measurement rules

- Report wall time per engine per script, but **subtract the MATLAB batch
  startup** (measured ≈3.2 s on this machine) for compute comparisons.
- Startup-dominated scripts (< 1 s MATLAB compute) are excluded from speed
  claims; the speed table is built from `pass` verdicts only (comparable
  work), and reports both numkit builds (WASM = the npm artifact, native =
  the engine ceiling).
