# Clean-room specification — `pitchCEP`

Cepstrum-based fundamental-frequency (pitch) estimation, per analysis
frame. Written per `cleanroom/PROTOCOL.md` (Spec Author role).

## Public references

- A. M. Noll, "Cepstrum Pitch Determination", *Journal of the
  Acoustical Society of America* 41(2):293–309, 1967.
- The real-cepstrum pitch method is also standard textbook material —
  e.g. Rabiner & Schafer, *Digital Processing of Speech Signals*;
  Oppenheim & Schafer, *Discrete-Time Signal Processing* (cepstral
  analysis chapter).

## 1. Algorithm

The real cepstrum of a voiced-speech frame shows a pronounced peak at
the quefrency equal to the fundamental period. For each analysis frame:

1. Apply an analysis window `w[n]` to the frame `y[n]`, n = 0..winLen-1.
2. Zero-pad the windowed frame to length `NFFT` and take its DFT:
   `Y[k] = DFT_NFFT(y·w)`, k = 0..NFFT-1.
3. Form the log power spectrum: `L[k] = log(|Y[k]|²)`. Where
   `|Y[k]|² == 0`, substitute `log` of the smallest positive normal
   double as a floor. (Log-magnitude vs log-power differ only by a
   constant factor 2 and do not move the peak.)
4. Real cepstrum: `c[q] = real(IDFT_NFFT(L))`, q = 0..NFFT-1. The
   index `q` is the *quefrency*, measured in samples.
5. A frequency `f` corresponds to quefrency `q = fs / f`. The pitch
   period therefore lies in the quefrency search range
   `q ∈ [ round(fs / maxF), round(fs / minF) ]`.
6. Find `q*`, the quefrency of the maximum of `c[q]` over that range.
7. The frame's estimate is `f0 = fs / q*`. If the search range is
   empty/invalid, `f0 = 0`.

**Indexing:** all arrays are 0-based; `q` denotes a 0-based cepstrum
array index. The pure-algorithm estimate is `f0 = fs / q`. See §3 for
the MATLAB-compatibility period convention that the shipped function
applies on top of this.

**NFFT:** use `NFFT` = smallest power of two ≥ `2·winLen − 1`.
Rationale: the cepstrum is an IDFT of a log-spectrum; padding the frame
to at least `2·winLen − 1` before the forward DFT avoids time-domain
aliasing of the cepstrum, and rounding up to a power of two keeps the
FFT on its fast path. This is a standard cepstral-analysis choice.

## 2. Interface (numkit)

Signature (drop-in replacement):

```cpp
Value pitchCEP(const Value &x, double fs, double minF, double maxF,
               std::pmr::memory_resource *mr);
```

- `x` — audio signal: a real `Value` vector of `x.numel()` samples;
  read sample `i` with `x.elemAsDouble(i)`.
- `fs` — sample rate (Hz). `minF`, `maxF` — pitch search range (Hz).
- `mr` — PMR resource. **PMR HARD RULE:** every scratch buffer via
  `ScratchArena` + `ScratchVec<T>`; no plain `std::vector` for scratch.
- Returns `Value::matrix(numFrames, 1, ValueType::DOUBLE, mr)` — one
  `f0` per frame. When `numFrames == 0`, return
  `Value::matrix(0, 0, ValueType::DOUBLE, mr)`.

File-local helpers already defined in the target file — **call these,
do not reimplement them**:

- `void hammingPeriodic(double *w, size_t N)` — fills `w[0..N-1]` with
  the periodic Hamming window `0.54 - 0.46·cos(2π·n/N)`.
- `struct FrameSpec { size_t winLen, overlap, hop, numFrames; };`
  `FrameSpec frameSpec(size_t N, double fs, double winSec, double ovSec);`
  — computes framing from window/overlap durations in seconds.
- `size_t nextPow2(size_t x);` — smallest power of two ≥ x.

numkit signal API:

- `Value signal::fft (const Value &m, int nfft, int dim, std::pmr::memory_resource *mr);`
- `Value signal::ifft(const Value &m, int nfft, int dim, std::pmr::memory_resource *mr);`
  — FFT/IFFT with transform length `nfft` along dimension `dim`
  (`dim == 1` transforms each column independently). `ifft` may return
  a `DOUBLE` or a `COMPLEX` `Value`; handle both via `Value::type()`,
  `Value::complexData()`, `Value::doubleData()`.

Suggested vectorised shape: build one `Value::matrix(NFFT, numFrames,
DOUBLE)` whose column `f` holds the zero-padded windowed frame, then
`signal::fft(..., NFFT, 1, mr)` transforms all frames at once.

## 3. Compatibility parameters

From MATLAB's **public documentation** of the `pitch` function
(Method `"CEP"`):

- Analysis window: periodic Hamming, length `winLen = round(0.052·fs)`.
- Overlap: `round(0.042·fs)`; `hop = winLen − overlap`.

Obtain framing via `frameSpec(N, fs, 0.052, 0.042)`. `minF`/`maxF` are
passed in by the caller (MATLAB default range `[50, 400] Hz`).

### Quefrency convention (for MATLAB-output parity)

MATLAB's CEP method reports the fundamental period using a **1-based**
quefrency index: empirically — verified against MATLAB R2025b black-box
output (e.g. a 220 Hz tone at fs = 16000 yields f0 values of exactly
fs/74, fs/65, …) — a cepstral peak at 0-based array index `q`
corresponds to a period of `q + 1` samples.

For output parity, therefore:
- search the cepstrum over array indices
  `q ∈ [ round(fs/maxF) − 1, round(fs/minF) − 1 ]`, clamped to valid
  indices;
- report `f0 = fs / (q* + 1)`.

This is a documented numeric convention — a fact about MATLAB's output,
obtained by black-box probing — not a structural element. The
implementation's control flow and naming remain independent.

## 4. Verification

- Parity: the CEP-method `pitch` parity spec under
  `tools/parity/specs/` — run the parity harness; require
  `correctness = OK` at the harness tolerance.
- gtest: the pitch/CEP tests under `libs/audio/tests/`.
- Bit-exact parity on undocumented internals is not required. A few-Hz
  agreement on the test signals is acceptable; record any tolerance
  change from a prior baseline in `cleanroom/PROTOCOL.md`.

## Constraints for the Implementer

- Do **not** open `libs/audio/src/features/pitch_harmonics.cpp`.
- Do **not** consult MATLAB `.m` source or any third-party reference
  implementation.
- Implement solely from this specification and the cited public
  references.
