# wavelet.wpdec / wprec / wpcoef — wavelet packet decomposition missing

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`wpdec` (1-D wavelet packet tree decomposition) is not registered (and with
it the packet family `wprec`, `wpcoef`, `wpsplt`, `besttree`).

## Repro
```matlab
t = wpdec([1 2 3 4 5 6 7 8], 2, 'db1')
% numkit: Error — VM: undefined function 'wpdec'
% MATLAB: wavelet-packet tree object (2 levels, full binary tree)
```

## Root cause
Not implemented.

## Suggested fix
Wavelet packet decomposition recursively applies the DWT low- AND high-pass
filters to BOTH the approximation and detail at each node (full binary
tree), unlike `wavedec` (only the approximation). numkit has the DWT
primitives (`dwt`, `wfilters`). The blocker is the **tree representation**:
MATLAB returns a packet-tree object — needs the engine's object model (see
dev-docs/OBJECT_MODEL.md) or a struct-based tree encoding. Medium-large; decide the
representation first. Defer until a tree container is chosen.

## References
- new file under `libs/wavelet/src/...`
- shipped: `dwt`, `wavedec`, `wfilters`
- MATLAB `doc wpdec`
