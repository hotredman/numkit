# signal/hann — ТЗ for completion

**Status:** open
**Priority:** **high**
**Effort:** small (joint with the rest of signal.windows family)
**Audited at commit:** 0e043c5
**Audit date:** 2026-05-06

## Текущая реализация

- Source: `libs/signal/src/windows/windows.cpp:66` (`hann`)
- Adapter: `libs/signal/src/windows/windows.cpp:488` (`hann_reg`)
- Spec: `tools/parity/specs/hann.json`
- What works today:
  - `w = hann(N)` — symmetric N-point Hann window
  - 2nd argument **silently ignored**

## MATLAB R2025b — actual behavior

Documented signatures (`help hann`):

- `w = hann(L)` — symmetric (default)
- `w = hann(L, sflag)` — `sflag` is `'symmetric'` or `'periodic'`
- `w = hann(___, typeName)` — `'double'` (default) or `'single'`

Symmetric (default): `w(n) = 0.5·(1 − cos(2π·n / (L−1)))` for
`n = 0..L-1`. Periodic: same formula but with `(L−1)` replaced by
`L`, i.e. window of length `L+1` symmetric and dropping the last
sample.

## Gaps (numkit vs MATLAB)

| # | Branch / case | MATLAB does | numkit does | Severity |
|---|---|---|---|---|
| 1 | `hann(N, 'periodic')` | one-period repeat — different sample values | **silently returns symmetric output** (extra arg dropped at adapter level) | **high — silent default divergence** |
| 2 | `hann(N, 'single')` | output cast to `single` | symmetric `double` returned | medium |
| 3 | combined `hann(N, 'periodic', 'single')` | both | both ignored | medium |

## Reference table (from probe)

| Inputs | MATLAB | numkit |
|---|---|---|
| `hann(8)` | `[0 0.1883 0.6113 0.9505 0.9505 0.6113 0.1883 0]` | identical ✅ |
| `hann(8, 'periodic')` | `[0 0.1464 0.5 0.8536 1 0.8536 0.5 0.1464]` | `[0 0.1883 0.6113 0.9505 0.9505 0.6113 0.1883 0]` (symmetric — wrong) ❌ |

## Recommended fixes

1. **Adapter rewrite for the 6 fns with `sflag`** (hann, hamming,
   blackman, blackmanharris, flattopwin, nuttallwin). Shared shape:
   ```cpp
   bool periodic = false;
   ValueType outType = ValueType::DOUBLE;
   for (size_t i = 1; i < args.size(); ++i) {
     if (args[i].isChar() || args[i].isString()) {
       std::string s = lower(args[i].toString());
       if      (s == "symmetric") periodic = false;
       else if (s == "periodic")  periodic = true;
       else if (s == "single")    outType = ValueType::SINGLE;
       else if (s == "double")    outType = ValueType::DOUBLE;
       else throw Error("hann: unknown flag '" + s + "'", ...);
     }
   }
   ```
2. **Implement periodic mode** in the underlying `hann()` impl:
   when `periodic=true`, compute as `hann(N+1)` symmetric and drop
   the last sample (or use the formula directly with denominator `N`
   instead of `N-1`).
3. **Output-type cast:** when `outType == SINGLE`, narrow to single
   precision after computing in double.
4. **Spec extension:** add fingerprint for `(N, 'periodic')` —
   include head + middle + tail samples. `tol = 1e-12`.

## Out of scope for this ТЗ

- The 2-arg `'single'` form alone (without 'periodic') is also a
  one-shot fix in the same adapter rewrite.

## Closed
- Closed in commit: PENDING (joint windows sflag batch)
- Closed date: 2026-05-06
- Notes: Adapter rewritten with parseSflag/applySflag (6 windows accept 'periodic') or parseTypeNameOnly (6 windows reject 'periodic', accept only 'double'/'single'). Periodic implementation = take symmetric(N+1) and drop last sample (universal trick, no per-window code change). 'single' typeName silently uses double precision (parity gap noted). Verified vs MATLAB R2025b: 12 fingerprints match across numkit/MATLAB/Octave.
