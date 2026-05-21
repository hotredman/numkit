# Clean-room specification — `designMelFilterBankSlaney`

Builds a Slaney-style triangular mel filterbank (the filterbank used by
`mfcc`). Written per `cleanroom/PROTOCOL.md` (Spec Author role).

## Public references (concrete PDFs)

- S. B. Davis and P. Mermelstein, "Comparison of Parametric
  Representations for Monosyllabic Word Recognition in Continuously
  Spoken Sentences", *IEEE Trans. ASSP* 28(4):357–366, 1980 — the
  triangular mel filterbank.
  PDF: https://courses.physics.illinois.edu/ece417/fa2017/davis80.pdf
- M. Slaney, "Auditory Toolbox, Version 2", Interval Research Corp.
  Technical Report #1998-010, 1998 (formerly Apple Tech. Report #45) —
  the Slaney mel band-edge layout (13 linear edges at 133.33 Hz
  spacing, then log-spaced by ×1.0711703) and the bandwidth-normalised
  triangular filters.
  PDF: https://engineering.purdue.edu/~malcolm/interval/1998-010/AuditoryToolboxTechReport.pdf

## 1. Algorithm

Given a set of mel band edges in Hz, a mel filterbank places one
triangular filter per band. Band `k` spans three consecutive edges
`edges[k] < edges[k+1] < edges[k+2]`: the filter rises linearly from 0
at `edges[k]` to 1 at `edges[k+1]`, then falls linearly from 1 back to
0 at `edges[k+2]` (Davis & Mermelstein 1980). With `numEdges` edges
there are `numBands = numEdges − 2` filters.

Each filter is evaluated on the one-sided FFT frequency grid: bin `j`
has frequency `f_j = j · fs / NFFT`, for `j = 0 .. H−1` where
`H = NFFT/2 + 1`.

The triangular weight of bin `j` in band `k` is the standard
overlapping-triangle formula:

```
rise = (f_j − edges[k])     / (edges[k+1] − edges[k])
fall = (edges[k+2] − f_j)   / (edges[k+2] − edges[k+1])
w    = max(0, min(rise, fall))
```

`min(rise, fall)` selects the rising side below the centre and the
falling side above it; `max(0, …)` zeroes bins outside
`[edges[k], edges[k+2]]`. Bins beyond `fs/2`, or bands whose edges lie
above `fs/2`, simply produce zero weights — no special-casing needed.

**Bandwidth normalisation** (Slaney 1998 — `Normalization='Bandwidth'`):
divide every weight of band `k` by half its base width,
`(edges[k+2] − edges[k]) / 2`. (A unit-height triangle of base `B` has
area `B/2`; dividing by `B/2` normalises each filter to unit area.)

Degenerate guards: if a band has a zero or negative side width
(`edges[k+1] ≤ edges[k]` or `edges[k+2] ≤ edges[k+1]`), or a
non-positive base width, leave that band's column at zero.

## 2. Interface (numkit)

```cpp
void designMelFilterBankSlaney(double *FB, double fs, size_t NFFT,
                               const double *edges, size_t numEdges,
                               size_t H /* = NFFT/2 + 1 */);
```

- `FB` — output buffer, pre-allocated, length `H * (numEdges − 2)`,
  **column-major**: the weight of bin `j` in band `k` is written to
  `FB[j + k * H]`. The function must first zero the whole buffer, then
  fill it.
- `fs` — sample rate (Hz). `NFFT` — FFT length. `H` — number of
  one-sided bins (`NFFT/2 + 1`).
- `edges` — `numEdges` mel band edges in Hz, ascending (produced by the
  caller's `slaneyBandEdges`).
- Returns `void`. Early-return (leaving nothing to do) if
  `numEdges < 3`, `H == 0`, or `NFFT == 0`.

This is a file-local helper (anonymous namespace) in
`libs/audio/src/spectral/cepstral.cpp`; it has no other numkit
dependencies — plain arithmetic over C arrays. Standard headers only
(`<algorithm>`, `<cmath>`).

## 3. Verification

- gtest: the `mfcc` tests in `libs/audio/tests/` (`mfcc` consumes this
  filterbank).
- Parity: `tools/parity/specs/audio_*` covering `mfcc` — require
  `correctness = OK`. The triangular construction is deterministic, so
  this is expected to remain bit-compatible with MATLAB.

## Constraints for the Implementer

- 0-based indexing; natural C++ — do not introduce a "first bin past
  the edge" index variable or any 1-based bookkeeping.
- Do **not** open `libs/audio/src/spectral/cepstral.cpp`.
- Do **not** consult MATLAB `.m` source.
- Implement solely from this specification and the cited public
  references.
