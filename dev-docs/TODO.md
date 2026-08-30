# numkit — engineering TODO / tech-debt

Tracked, non-urgent improvements that are deliberately out of scope for the
change that surfaced them. Each entry is self-contained: a session should be
able to act on it cold. Behaviour-changing items must be done under user
supervision (not folded into mechanical refactors).

Format per entry: **Title** · *Kind* (tech-debt / perf / cleanup / design) ·
*Status* (open / in-progress / done) — then Problem / Why-deferred / Fix /
Affected.

---

## 1. Replace the process-global shared RNG + mutex with a per-`Engine` stream
*Kind:* design · *Status:* open · *Surfaced:* 2026-06 layering review (binornd)

**Problem.** All random builtins draw from a single **process-global** generator
`numkit::builtin::sharedEngine()`, serialised by `numkit::builtin::rngMutex()`.
Typical shape (e.g. `binornd`, `normrnd`, `rand`, `randn`, `randi`, `randperm`,
`datasample`, `bootstrp`, channel/fading noise in comm, …):

```cpp
auto &gen = ::numkit::builtin::sharedEngine();
auto &mtx = ::numkit::builtin::rngMutex();
...
std::lock_guard<std::mutex> lk(mtx);
for (size_t i = 0; i < cnt; ++i) od[i] = dist(gen);
```

The mutex is *correct* (it stops a data race on the shared generator state when
the C++ API is called from embedder threads / `ops::parallel_for` / multiple
`Engine` instances — `dist(gen)` mutates `gen`). It is **not a hot-path cost**:
one uncontended lock per *call*, not per element, and the runtime VM/eval is
single-threaded so it is essentially never contended.

**Why it is debt, not a bug.** A single global generator + mutex is an
anti-pattern for a library:
- Two `Engine` instances (or threads) share one stream, so `rng(seed)` in one
  leaks into the other; reproducibility is not isolated per engine.
- Under real concurrency the interleaving order of draws across threads is
  nondeterministic → results are not reproducible even with a fixed seed.
- Global mutable state complicates the layering goal (RNG lives in `ops`, but a
  process-global singleton is awkward to reason about per-layer).

**Fix (behaviour-changing — do under user supervision).** Give each `Engine`
its own RNG stream (state owned by the engine / threaded through `CallContext`),
drop the global singleton and the mutex. The public C++ `*rnd` API would take
the RNG state (or `mr` already carries an arena — add an rng handle) instead of
reaching for `sharedEngine()`. Keep MATLAB `rng(seed)` semantics per engine.
This changes seeding/reproducibility semantics, so it needs explicit sign-off +
parity re-validation, NOT a mechanical pass.

**Affected.** `numkit::builtin::sharedEngine` / `rngMutex` (toolboxes/builtin RNG
infra, relocated to `ops/rng.{hpp,cpp}` in the layering refactor) and every
`*rnd` / sampling / noise builtin across toolboxes/stats, toolboxes/builtin, toolboxes/comm
(`grep -rl "sharedEngine()" libs`). Original authorship: pre-layering
(commit 67a702c3d, 2026-05-03) — not introduced by the compute/register split.

---

## 2. fieldtest graphics phase — add display/plotting repos back to the corpus
*Kind:* tech-debt · *Status:* open · *Surfaced:* 2026-08-30 (user decision: compute/processing only for the current phase)

**Problem.** The fieldtest sources list deliberately excludes graphics/display
repos (export_fig, matlab2tikz, ZoomPlot), and the harness harvest drops any
script containing a drawing call. Real-world plotting code therefore exercises
neither the parser surface nor the graphics API against MATLAB.

**Why deferred.** Phase focus: computation/processing correctness. Plot calls
print nothing to stdout, and an absent plot option must not fail an otherwise
computational script.

**Fix.** A later phase: re-add the graphics repos to `fieldtest/sources.list`,
drop the display-token filter from `harness.py` harvest for a graphics batch,
and compare what is comparable (exit status, non-plot stdout, figure COUNT via
a headless figure-counter if one exists).

**Affected.** `fieldtest/sources.list` (note at the end), `fieldtest/harness.py`
(BAD_TOKENS display block).

---

## 3. Minimal generative parser/interpreter fuzzer
*Kind:* tech-debt · *Status:* open · *Surfaced:* 2026-08-29 (stack-safety design §5 layer 3; deferred repeatedly since)

**Problem.** No fuzzing harness exists for the lexer/parser/compiler/TreeWalker
chain. The stack-safety work proved the class is real (deep nesting killed the
process); arbitrary malformed input may find hangs, lexer loops, assertion
escapes that hand-written tests never will. The engine is a pure string->result
function — trivially fuzzable.

**Why deferred.** Priority went to the differential fieldtest corpus (real
code) and the P1 fix queue.

**Fix.** A gtest with a time budget: mutate the seed corpus (all repo .m files:
smokes, examples, fieldtest work copies) + random token soup; invariant =
parses or throws a diagnostic, never crashes/hangs. Coverage-guided later via
the emscripten/clang build.

**Affected.** New `src/core/tests/parser_fuzz_test.cpp` (wired like
stack_guard_test.cpp); seeds from `src/**/tests/smoke/`, `examples/`,
`fieldtest/corpus/work/`.
