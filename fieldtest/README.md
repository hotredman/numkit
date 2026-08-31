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
  fetch.py         clone/update catalog repos into corpus/ + prepare work copy
  harness.py       dual-run (MATLAB vs numkit WASM vs native), classify, report
  signprobe.py     signature check: `help fn` (arg list) diff, MATLAB vs numkit
  reports/         committed run reports (JSON + summary per batch)
  runnable.json    COMMITTED — the run corpus: scripts verified to execute in
                   MATLAB (paths + verdicts only, no third-party source text)
  corpus/          GITIGNORED — cloned repos (corpus/repos/), the prepared
                   work copy (corpus/work/), and the cached catalog.json.
                   Third-party code NEVER enters git.
```

Sources come from the external catalog
[awesome-matlab-books](https://github.com/hotredman/awesome-matlab-books)
(`catalog.json`: companion-code repositories for 75+ MATLAB books across 12
categories). The catalog is cached at `corpus/catalog.json` on first fetch and
reused from cache afterwards, so a batch stays reproducible against the
catalog it was fetched with; `--refresh-catalog` pulls a newer one.

## Workflow

1. **Fetch** — `python fetch.py` clones/updates every catalog repo into
   `corpus/repos/<owner--repo>` (filters: `--category <id>[,<id>]`,
   `--book <id>[,<id>]`; `--list` shows what's available), then prepares
   `corpus/work/`: a UTF-8-transcoded mirror (many real-world files are GBK;
   transcoding gives BOTH engines the same text — MATLAB on a non-Chinese
   locale garbles GBK just like numkit does, so the comparison stays fair).

2. **Qualify** — `python harness.py qualify [N] [filter]` harvests heuristic
   candidates (self-contained scripts that print output, static token filter)
   and then **verifies runnability empirically**: each candidate is executed
   in MATLAB R2025b; the ones that run to completion form the run corpus,
   committed as `runnable.json`. This replaces trusting the static harvest:
   heuristic filters both miss runnable scripts and admit scripts that error
   in MATLAB (worthless for diffing — there is no ground truth to diverge
   from). Runnable = MATLAB exit 0; determinism is still probed later by the
   double numkit run at diff time. The catalog of runnable scripts is the
   stable comparison set batches track progress against.

3. **Run** — `python harness.py run [N] [filter]` executes each `runnable.json`
   script through the three engines (timeout-protected, determinism-probed by
   a double numkit run), fuzzy-diffs output (numeric tokens compared at
   rel 1e-6), and writes `reports/` + a verdict per script:

   | verdict | meaning | action |
   |---|---|---|
   | `pass` | outputs match numerically | record timings (speed stats) |
   | `runtime-error` / `output-mismatch` | numkit fails or diverges where MATLAB succeeds | **file a bug** (see below) |
   | `parse-error` | numkit's parser rejects valid MATLAB | **file a bug** |
   | `absent-fn` | function not implemented | add to `bugs/missing.md` (a parity gap, NOT a defect per the Kind legend) |
   | `both-error` | both engines error | eyeball: same construct → error-parity PASS; different → bug on the diverging side |
   | `numkit-hang` / `matlab-timeout` | hang | bug (hang) |
   | `nondeterministic` | double-run differs | exclude, note in report |

4. **File every finding — self-contained.** Runtime/parse divergence →
   `bugs/opened/<ns>/<fn>.md` (Kind: bug) **+ a reproducing gtest** per the
   repo protocol (`DISABLED_` until fixed, live after). The bug file's Repro
   is a **minimal inline snippet** distilled from the corpus script — no
   fieldtest/corpus references (the bug must survive the corpus being wiped;
   see the Self-Contained Repro Rule in AGENTS.md). Absent function → a row
   in `missing.md`. Signature divergence →
   `bugs/opened/<ns>/<fn>-signature.md` (Kind: bug).

5. **Signature audit** — `python signprobe.py <fn> [<fn>…]` captures MATLAB's
   documented signature (`help fn`) and numkit's, diffs them, and reports the
   first divergent line. Any arg-list mismatch (missing option, different
   default, different nargout shape) becomes a signature bug. Batch-audit the
   functions a corpus batch actually used:
   `python signprobe.py $(python harness.py used-fns <report>)`.

## Sources policy

- **The catalog is the ONLY source of repositories.** No supplementary local
  picks — new sources are added to awesome-matlab-books itself (it is a
  curated project for exactly this), then picked up by `fetch.py
  --refresh-catalog`. The local corpus is disposable: `rm -rf fieldtest/corpus`
  must lose nothing that isn't rebuildable or already distilled into a bug
  file.
- Source of truth: the awesome-matlab-books catalog (CC BY-SA 4.0 as a
  compilation). Per-repo licenses vary — book companion code is often
  all-rights-reserved with no LICENSE file; that is acceptable here because:
- `corpus/` is gitignored; third-party code is never committed, shipped, or
  redistributed — only executed for testing. Reports contain paths + verdicts,
  not third-party source text. The cached `catalog.json` stays under
  `corpus/` for the same reason.
- `python_companions` (books whose companion code is Python) are deliberately
  not fetched.

## Speed measurement rules

- Report wall time per engine per script, but **subtract the MATLAB batch
  startup** (measured ≈3.2 s on this machine) for compute comparisons.
- Startup-dominated scripts (< 1 s MATLAB compute) are excluded from speed
  claims; the speed table is built from `pass` verdicts only (comparable
  work), and reports both numkit builds (WASM = the npm artifact, native =
  the engine ceiling).
