# (cross-cutting) complex MATRICES unsupported across linear algebra

- **Status:** 🔴 OPEN
- **Severity:** P2 (errors where MATLAB returns a value) — but broad & fundamental
- **Kind:** bug
- **Found:** 2026-06-04 via DEEP-PROBE (complex matrix sweep)

## Symptom
Essentially the entire linear-algebra suite rejects a complex matrix with
"Not a double array" (or "not yet supported"). MATLAB supports complex
matrices everywhere. Even `trace` (a trivial diagonal sum) fails.

| Op | numkit | MATLAB (B = [1+1i 2; 3 4-1i]) |
|---|---|---|
| `trace(B)` | Not a double array | `5+0i` |
| `det(B)` | Not a double array | `-1+3i` |
| `inv(B)` | Not a double array | (complex inverse) |
| `eig(B)` | Not a double array | `[-0.2474+0.5460i, 5.2474-0.5460i]` |
| `svd(B)` | Not a double array | `S(1,1)=5.6289` |
| `qr(B)` | Not a double array | complex Q,R |
| `lu(B)` | Not a double array | complex L,U,P |
| `chol([2 1i;-1i 2])` | Not a double array | `[1.4142, 0.7071i; 0, …]` |
| `rank(B)` | Not a double array | `2` |
| `pinv(B)` | Not a double array | complex pseudo-inverse |
| `B\b` | "complex matrix systems not yet supported" | complex solve |

## Root cause
The linalg kernels read `x.doubleData()` (real storage) with no
`ValueType::COMPLEX` path; `mldivide` bails out explicitly. This is the
matrix-level counterpart of the element-wise complex gap
(bugs/math/complex-input-unsupported.md) and the vector-norm gap
(bugs/linalg/norm-complex.md).

## Suggested fix
Large — needs complex versions of the kernels (Hermitian transpose where a
real algorithm uses transpose; complex pivoting). Best done incrementally,
cheapest first:
1. `trace` (diagonal sum — trivial), `det`/`inv` (via a complex LU).
2. `qr`/`lu`/`chol` (complex Householder / pivoting / Hermitian Cholesky).
3. `eig`/`svd`/`rank`/`pinv`/`mldivide` (complex eigen/SVD — the big ones;
   route through LAPACK z* if available, else complex Jacobi/QR iteration).
Each can land independently; this entry is the umbrella.

## References
- `src/toolboxes/linalg/src/...` (decomposition + solve kernels)
- MATLAB: complex inputs accepted by eig/svd/qr/lu/chol/det/inv/etc.
