# todo: follow-ups for the portion-5 partial fixes (varargin / randn-seed / freqs-2arg)

*Kind:* tech-debt · *Status:* open · *Surfaced:* 2026-08-31 (post-fix quality review)

> Lifecycle: open → done. On completion record the outcome in
> `dev-docs/memory/` and delete this file.

The three fixes are structurally sound (real work at the right layer, both
engines, live guards) but each deliberately traded part of the surface for
unblocking. The trades are documented in the closed bug files; this todo
tracks finishing them.

**1. freqs(b,a) auto-grid — make the grid MATLAB's, not ours.**
Current: 200 log-spaced points [min-corner/100, max-corner·100] — our
heuristic. MATLAB's is deterministic and probeable: `[h, w] = freqs(b, a)`
returns w. Probe R2025b on 2–3 filter families, reverse the range rule,
add a parity spec (`freqs_2arg`) that fails until the grid matches.
Until then any fieldtest script consuming freqs(b,a) output is
workspace-incomparable.

**2. Legacy RNG — value parity decision.**
`randn('seed', n)` now seeds our MT19937 (rng(n) semantics); MATLAB's
legacy generator produces a DIFFERENT sequence. Consequence: any corpus
script that seeds and computes is workspace-incomparable (values diverge
legitimately). Either implement the legacy generators (MATLAB documents
'seed'→the old lagged-Fibonacci/MT variants — real work), or permanently
document seeded scripts as a non-comparable class in the fieldtest
triage rules (prevents false workspace-mismatch bugs).

**3. varargin surface gaps.**
- No parse-time validation: `function f(varargin, x)` is a MATLAB parse
  error ("varargin must be the last"), numkit silently accepts.
- Untested paths: classdef methods with varargin, function handles to
  file functions with varargin, nested functions. Add dual-engine tests.
- Pre-existing (out of scope, noted): too-few-args for non-varargin
  functions still yields undefined-variable instead of MATLAB's "Not
  enough input arguments".

**4. Legacy RNG small surface.**
- `randn('seed')` single-arg QUERY form (returns current seed) still errors.
- `randi('seed', …)` legacy form not covered (randi has its own reg).

**5. Verification debt (user's call per test policy).**
Full suite + corpus gate (`packages/numkit/test/run-examples.js`) not run
after the WASM rebuild — the call-path change is exercised implicitly by
every dual-engine test, risk is low, but a full pass is the thorough
check before the next push/publish.
