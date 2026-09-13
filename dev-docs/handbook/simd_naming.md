# SIMD naming & multi-backend rules

*Living rule (user decision, 2026-09-13).* Applies to the ops layer,
its CMake wiring, presets and benchmarks. Vocabulary first — then the
recipe for the day a second SIMD backend (e.g. direct intrinsics
beside Highway) lands.

## The three name levels

| Level | Word used | Where it appears | Rule |
|---|---|---|---|
| Feature / layer | **SIMD** | `NK_WITH_SIMD` option, `bench-simd*` presets, `benchmarks/simd/` harness | Names the CONCEPT ("a vectorized path exists"). Never names a library. |
| Backend | **implementation name** | kernel TU suffixes: `*_highway.cpp`, future `*_native.cpp` / `*_avx512.cpp` | A TU containing per-target vectorized code NAMES ITS BACKEND in the filename. Bare `*_simd.cpp` for an implementation TU is FORBIDDEN (ambiguous once a second backend exists). |
| Scalar fallback | **portable / scalar** | `*_portable.cpp`, `binary_scalar.cpp` | Already consistent — keep. |

Existing shape (the model to copy):
`src/ops/src/binary_ops_highway.cpp` + `binary_scalar.cpp`;
`src/ops/src/fft/*_highway.cpp`; `src/builtin/src/elfun/mod_simd.hpp`
(the backend-split INTERFACE header — feature level, correct as-is)
with `mod_highway.cpp` / `mod_portable.cpp` implementing it.

ISA dispatch is NOT a naming level: Highway picks SSE4/AVX2/AVX-512
at first call (`HWY_DYNAMIC_DISPATCH`) inside the single Highway
backend. Do not name files after ISAs unless the file IS a
hand-written intrinsics backend for that ISA.

## Recipe: adding a second SIMD backend (e.g. native AVX-512 intrinsics)

1. **TU + gate.** New `*_avx512.cpp` (or `*_native.cpp`) TU beside the
   Highway one, behind its OWN CMake gate (`NK_WITH_NATIVE_SIMD` or
   `NK_SIMD_BACKENDS=highway;native`). `NK_WITH_SIMD` stays the master
   switch; backends are independent optional providers. Architecture-
   scoped: the native gate must not compile on ARM.
2. **One dispatch seam, above the backends.** The kernel's public
   entry forwards to a per-kernel provider table; each backend
   registers `{backend, isa floor, entry fn}` at static init. First
   call selects: highest ISA floor available wins; a backend that
   explicitly overrides (hand-tuned) beats Highway's generic codegen
   at the same ISA. Selection is cached — deterministic afterwards.
   The public API and the scalar fallback never learn backends exist.
3. **Value transparency is the acceptance gate.** The existing
   dual-engine gtests run UNCHANGED through the public API with
   (a) SIMD off, (b) Highway only, (c) Highway+native — the native TU
   must include tail cases (sizes not a multiple of the vector width),
   denormals and NaN lanes before it may register.
4. **Benchmark triangle.** Add `bench-simd-native`; the
   `benchmarks/simd/` A/B (scalar vs simd) becomes scalar /
   highway / native. Keep a native TU only if it wins ≥10% sustained
   on its target ISA (perf protocol: measured numbers, S-scale) —
   otherwise delete it. Two backends is permanent double maintenance.
5. **No speculative plumbing.** Do not add the provider-table layer,
   options or presets BEFORE the second backend exists — the naming
   rule above is chosen so that adding it later renames nothing.

## Worked example: fft_highway.cpp + fft_avx512.cpp

Conflict avoidance is structural: backend TUs live in their own
namespaces (`detail::highway::` / `detail::avx512::` — no link clash);
the public entry never calls a backend directly — it asks ONE
resolver, and only the resolver writes the cached choice.

```
build knob    NK_WITH_SIMD=ON + NK_SIMD_BACKENDS=highway;native
              (which backends EXIST; default "highway")

runtime auto  provider table, registered at static init:
                {backend, isaFloor, overrideDefault, fn}
              first call → highest CPU-supported isaFloor wins;
              tie → override (hand-tuned) beats generic Highway;
              cached. AVX-512 CPU → native; AVX2 CPU → Highway's
              AVX2 target; older → portable.

explicit      NK_SIMD_FORCE=native|highway|scalar (env, read once).
              Forced but unsupported by the CPU → LOUD startup error,
              never a silent fallback. This is what benchmarks use —
              the triangle runs off one binary + env, no extra preset.
              Semantics: PREFERENCE ORDER, not ban — "native" means
              "native provider where one exists, Highway otherwise"
              (hand-tuned kernels are written pointwise; a global ban
              would leave non-tuned kernels without any SIMD).
              Per-kernel strict form reserved: NK_SIMD_FORCE=fft:native
              (fft strictly native, everything else auto).
              Third, PROGRAMMATIC knob (internal, for nk_bench):
              ResolveFft(Force::Native) — per-CALL strict selection so
              BM_Fft_Highway and BM_Fft_Native measure both backends
              in one process. No language-level knob by design.

precedence    NK_SIMD_BACKENDS (exists?) → NK_SIMD_FORCE (preference)
              → CPU capability (isa floor) → overrideDefault (hand-tuned
              beats generic) → cached at the kernel's first call.
```

Sketch on the existing seam (public fft() already forwards into
`nk::ops::detail` dispatchers):

```cpp
// fft_dispatch.hpp — the feature-level seam
struct FftProvider {
    enum class Backend { Highway, NativeAvx512 };
    Backend  backend;
    unsigned isaFloor;         // e.g. HWY_AVX3
    bool     overrideDefault;  // hand-tuned beats generic at same floor
    void   (*forward)(Complex *, std::size_t, const Complex *);
};
void RegisterFftProvider(const FftProvider &);   // each backend TU, static init
const FftProvider *ResolveFft();                 // first call: select + cache

// fft_avx512.cpp
namespace { const bool reg = [] {
    RegisterFftProvider({Backend::NativeAvx512, HWY_AVX3, true, &fftAvx512});
    return true; }(); }

// public fft()
if (const auto *p = ResolveFft()) p->forward(buf, N, W);
else                              fftPortable(buf, N, W);
```
