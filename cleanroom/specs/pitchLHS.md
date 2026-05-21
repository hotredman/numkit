# Clean-room specification — `pitchLHS`

Harmonic-summation ("log harmonic summation") fundamental-frequency
estimation, per analysis frame. Written per `cleanroom/PROTOCOL.md`
(Spec Author role).

## Public references

- D. J. Hermes, "Measurement of pitch by subharmonic summation",
  *Journal of the Acoustical Society of America* 83(1):257–264, 1988
  — the principle of detecting the fundamental by summing the spectrum
  at harmonic multiples.
- Harmonic-sum pitch detection is standard, surveyed material — e.g.
  W. Hess, "Pitch and voicing determination", in *Advances in Speech
  Signal Processing*, 1992.

## 1. Algorithm

A voiced signal of fundamental `f` has energy at `f, 2f, 3f, …`.
Summing the log-magnitude spectrum at integer multiples of a candidate
fundamental therefore produces a score that peaks at the true `f0`.

Per analysis frame:

1. Apply an analysis window `w[n]` to the frame.
2. Zero-pad to `NFFT` and take the DFT. Use `NFFT = round(fs)` so each
   bin spans exactly 1 Hz (`Δf = fs/NFFT = 1`); a bin index then
   equals a frequency in Hz, and integer-harmonic indexing is direct.
3. Log-magnitude spectrum: `S[k] = log(|Y[k]|)`, k = 0 .. K−1. Floor
   `|Y[k]| = 0` at `log` of the smallest positive normal double.
   `K` must cover the highest harmonic bin needed (see §3).
4. Harmonic-sum score for a candidate fundamental at bin `j`:
   `domain[j] = Σ_{m=1..H} S[j·m]` (H = harmonic count, see §3).
5. `f0` candidate = the `j` maximising `domain[j]` over the search
   range.
6. Clip the result to `[minF, maxF]`.

**Indexing:** all arrays 0-based; `j` is a 0-based bin index. With
1-Hz bins, `domain[j]` scores a fundamental of `j` Hz. The
MATLAB-compatibility convention (§3) is applied on top of this.

## 2. Interface (numkit)

```cpp
Value pitchLHS(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr);
```

- `x` — real audio `Value` vector; sample `i` via `x.elemAsDouble(i)`.
- `fs` sample rate (Hz); `minF`, `maxF` search range (Hz); `mr` PMR
  resource. **PMR HARD RULE:** scratch via `ScratchArena` +
  `ScratchVec<T>`.
- Returns `Value::matrix(numFrames, 1, ValueType::DOUBLE, mr)`; when
  `numFrames == 0` return `Value::matrix(0, 0, ValueType::DOUBLE, mr)`.

File-local helpers already defined — **call, do not reimplement**:
- `void hammingPeriodic(double *w, size_t N)` — periodic Hamming
  `0.54 - 0.46·cos(2π·n/N)`.
- `struct FrameSpec { size_t winLen, overlap, hop, numFrames; };`
  `FrameSpec frameSpec(size_t N, double fs, double winSec, double ovSec);`

numkit signal API:
- `Value signal::fft(const Value &m, int nfft, int dim, std::pmr::memory_resource *mr);`
  — `dim == 1` transforms each column. Forward result is COMPLEX.
  Access via `Value::complexData()` → `const Complex *`.

A per-frame DFT loop (one column `Value`, FFT to `NFFT`) is acceptable;
batching all frames into one `NFFT × numFrames` matrix and a single
`signal::fft(..., NFFT, 1, mr)` is also acceptable.

## 3. Compatibility parameters

From MATLAB's **public documentation** of `pitch` (Method `"LHS"`):
- Analysis window: periodic Hamming, `winLen = round(0.052·fs)`.
- Overlap: `round(0.042·fs)`. Obtain framing via
  `frameSpec(N, fs, 0.052, 0.042)`.

Method parameters (facts about the LHS method):
- Harmonic count `H = 5` (the first five harmonics are summed).
- DFT length `NFFT = round(fs)` (1-Hz bins).
- Highest bin needed: `K = H · floor(maxF)` (i.e. `5·floor(maxF)`).
  If `K > NFFT`, the range cannot be served — return the empty/zero
  result.

### Bin-index convention (for MATLAB-output parity)

MATLAB's LHS reports `f0` using a **1-based** bin index: empirically —
verified against MATLAB R2025b black-box output — a harmonic-sum peak
at 0-based bin index `j` is reported as `f0 = j + 1` Hz.

For output parity:
- search `domain` over 0-based indices
  `j ∈ [ ceil(minF) − 1, floor(maxF) − 1 ]`, clamped to valid indices;
- report `f0 = j* + 1`, then clip to `[minF, maxF]`.

This is a documented numeric convention — a fact about MATLAB's output,
obtained by black-box probing — not a structural element.

## 4. Verification

- Parity: `tools/parity/specs/audio_pitch_harmonics.json` — fingerprints
  `lhs_first`, `lhs_2`, `lhs_mean`; require `correctness = OK`.
- gtest: `PitchHarmonicsTest` in `libs/audio/tests/pitch_harmonics_test.cpp`.

## Constraints for the Implementer

- Do **not** open `libs/audio/src/features/pitch_harmonics.cpp`.
- Do **not** consult MATLAB `.m` source or any third-party reference
  implementation.
- Implement solely from this specification and the cited public
  references.
