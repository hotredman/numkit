# numkit-m parity progress

This is the live parity map of numkit's MATLAB compatibility surface.
Every documented MATLAB function gets a row here, organized by
MATLAB-doc section + namespace. Empty cells mean "not yet benched";
filled cells reflect the most recent harness run.

Updated by `tools/parity/run_parity.py` — each spec run rewrites the
row(s) for its function in place (the same function may appear in
multiple sections; all occurrences refresh together).

**Columns:**
- `function` — MATLAB-doc name
- `status` — ✅ implemented · ❌ missing · ⚠️ partial / operator-only
- `numkit_ms` — single-iteration mean (ms) on numkit native build
- `vs_MATLAB` — MATLAB_ms / numkit_ms (>1× = numkit faster)
- `vs_Octave` — same against Octave
- `correctness` — `OK` element-wise vs MATLAB · `MISMATCH` · `N/A` if
  the comparison engine doesn't support the function
- `comment` — input size / notes / deviations

## Entering Commands

**Namespace:** core — 5 ✅ + 0 ⚠️ / 9 = 56%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ans` | ✅ |  |  |  |  | implicit-assigned unsuppressed result |
| `clc` | ✅ |  |  |  |  |  |
| `commandhistory` | ❌ |  |  |  |  | IDE-only |
| `commandwindow` | ❌ |  |  |  |  | IDE-only |
| `diary` | ❌ |  |  |  |  | session log |
| `format` | ✅ |  |  |  |  | output format (no-op stub) |
| `home` | ✅ |  |  |  |  | terminal home |
| `iskeyword` | ✅ |  |  |  |  | introspection |
| `more` | ❌ |  |  |  |  | pager |

## Matrices and Arrays

**Namespace:** core — 53 ✅ + 1 ⚠️ / 55 = 98%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `blkdiag` | ✅ | 0.081 | 1.19× | 1.88× | OK | Sig: D = blkdiag(A,B,C). 50/80/60 deterministic mats. 100 iters. Element-wise SAVE. |
| `cat` | ✅ | 1.843 | 0.91× | 0.59× | OK | Sig: D = cat(DIM,A,B). 500x500 vert-cat. 100 iters. Element-wise SAVE. |
| `circshift` | ✅ | 3.830 | 0.33× | 0.64× | OK | Sig: B = circshift(A, K). 1000x1000 shift [3 5]. 100 iters. Element-wise SAVE. |
| `colon` | ⚠️ |  |  |  |  | works as `:` (range) operator; not callable as named fn |
| `combinations` | ❌ |  |  |  |  | all combinations |
| `ctranspose` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `diag` | ✅ | 0.008 | 1.22× | 1.97× | OK | Sig: V = diag(A). Diagonal of 2000x2000 deterministic. 100 iters. |
| `end` | ✅ |  |  |  |  | keyword + `A(end)` indexing form |
| `eye` | ✅ | 1.808 | 0.57× | 0.00× | OK | Sig: I = eye(N). 1000x1000 identity. 100 iters. |
| `false` | ✅ |  |  |  |  | literal/constant |
| `flip` | ✅ | 2.122 | 0.79× | 1.03× | OK | Sig: B = flip(A, DIM). 1000x1000 flip dim 2. 100 iters. Element-wise SAVE. |
| `fliplr` | ✅ | 2.144 | 0.80× | 1.02× | OK | Sig: B = fliplr(A). 1000x1000 left-right flip. 100 iters. Element-wise SAVE. |
| `flipud` | ✅ | 2.308 | 0.53× | 0.99× | OK | Sig: B = flipud(A). 1000x1000 up-down flip. 100 iters. Element-wise SAVE. |
| `freqspace` | ✅ | 0.001 | 14.68× |  | MISMATCH | Sig: F = freqspace(N). 1024-pt freq vector. 10000 iters. |
| `head` | ✅ | 0.000 | 59.14× |  | OK | Sig: Y = head(X, K). First 100 elements. 10000 iters. |
| `horzcat` | ✅ | 1.842 | 0.62× | 0.57× | OK | Sig: D = horzcat(A, B). 500x500 || 500x500. 100 iters. |
| `ind2sub` | ✅ | 12.093 |  | 0.93× | OK | Sig: [I,J] = ind2sub(SZ, IND). 1M idx → row index. SAVE on row idx (y). 50 iters. |
| `ipermute` | ✅ | 5.008 | 0.66× | 1.16× | OK | Sig: Y = ipermute(X, ORDER). Round-trip via permute. 100 iters. |
| `iscolumn` | ✅ | 0.000 | 26.89× | 68.46× | OK | Sig: TF = iscolumn(X). 1k column. 100k iters. |
| `isempty` | ✅ | 0.000 | 25.72× | 34.68× | OK | Sig: TF = isempty(X). Empty []. 100k iters. |
| `ismatrix` | ✅ | 0.000 | 24.44× | 57.78× | OK | Sig: TF = ismatrix(X). 1k×1k mat. 100k iters. |
| `isrow` | ✅ | 0.000 | 29.38× | 17.67× | OK | Sig: TF = isrow(X). 1k row. 100k iters. |
| `isscalar` | ✅ | 0.000 | 41.65× | 43.61× | OK | Sig: TF = isscalar(X). 100k iters. |
| `issorted` | ✅ | 0.008 | 0.86× | 1.64× | OK | Sig: TF = issorted(X). 10k pre-sorted. 10k iters. |
| `issortedrows` | ✅ |  |  |  |  |  |
| `isuniform` | ✅ |  |  |  |  | uniform-spacing test |
| `isvector` | ✅ | 0.000 | 26.56× | 51.51× | OK | Sig: TF = isvector(X). 10k vec. 100k iters. |
| `length` | ✅ | 0.000 | 26.91× | 36.70× | OK | Sig: L = length(X). 100x600 → returns 600. 100k iters. |
| `linspace` | ✅ | 2.871 | 1.00× | 0.80× | OK | Sig: V = linspace(A,B,N). N=1M. 100 iters. Element-wise SAVE. |
| `logspace` | ✅ | 9.205 | 0.95× | 1.42× | OK | Sig: V = logspace(A,B,N). N=1M log-spaced. 100 iters. Element-wise SAVE. |
| `meshgrid` | ✅ |  |  |  | N/A | Sig: [X,Y] = meshgrid(x,y). 1k×1k grid. 100 iters. SAVE on X. |
| `ndgrid` | ✅ |  |  |  | N/A | Sig: [X,Y] = ndgrid(x,y). 1k×1k grid. 100 iters. |
| `ndims` | ✅ | 0.000 | 27.42× | 25.81× | OK | Sig: N = ndims(X). 2D mat → 2. 100k iters. |
| `numel` | ✅ | 0.000 | 22.63× | 20.44× | OK | Sig: N = numel(X). 1M-elem mat. 100k iters. |
| `ones` | ✅ | 2.645 | 0.73× | 0.84× | OK | Sig: O = ones(M,N). 1000x1000. 100 iters. |
| `paddata` | ✅ |  |  |  |  | pad N-D |
| `permute` | ✅ | 2.322 | 0.54× | 1.11× | OK | Sig: Y = permute(X, ORDER). 100×100×100 → reordered. 100 iters. |
| `rand` | ✅ | 6.807 | 0.51× | 0.81× | OK | Sig: A = rand(M,N). 1k×1k uniform. 100 iters. Custom fp (RNG diffs). |
| `repelem` | ✅ | 2.189 | 0.55× | 1.01× | OK | Sig: Y = repelem(X, K). 1k vec each elem 1000x. 50 iters. |
| `repmat` | ✅ | 2.113 | 0.44× | 1.08× | OK | Sig: B = repmat(A,M,N). 50x50 → 1000x1000. 100 iters. |
| `reshape` | ✅ | 1.999 | 0.00× | 1.06× | OK | Sig: B = reshape(A,M,N). 1M vec → 1000x1000. 100 iters. |
| `resize` | ✅ |  |  |  |  | general resize |
| `rot90` | ✅ | 2.992 | 0.80× | 1.92× | OK | Sig: B = rot90(A). 1k×1k 90° rotate. 100 iters. |
| `shiftdim` | ✅ | 2.273 | 0.01× | 7.12× | MISMATCH | Sig: B = shiftdim(A). Drop leading singleton. 1000 iters. |
| `size` | ✅ | 0.000 | 18.70× | 36.28× | OK | Sig: S = size(X). 2D 100x600 → [100 600]. 100k iters. |
| `sort` | ✅ | 44.711 | 0.15× | 0.15× | OK | Sig: B = sort(A). 1M deterministic sin values. 100 iters. Element-wise SAVE. |
| `sortrows` | ✅ | 0.425 | 0.74× | 0.19× | OK | Sig: B = sortrows(A). 10k×3 sort by first col. 100 iters. |
| `squeeze` | ✅ | 2.034 | 0.01× | 0.00× | OK | Sig: Y = squeeze(X). 1×1k×1×1k → 1k×1k. 1000 iters. |
| `sub2ind` | ✅ | 7.505 | 0.23× | 0.47× | OK | Sig: IND = sub2ind(SZ, I, J). 1M (r,c) pairs. 50 iters. |
| `tail` | ✅ | 0.000 | 54.28× |  | OK | Sig: Y = tail(X, K). Last 100 elements. 10000 iters. |
| `transpose` | ✅ | 7.375 | 0.20× | 0.35× | OK | Sig: Y = transpose(X). 1k×1k transpose. 100 iters. Element-wise SAVE. |
| `trimdata` | ✅ |  |  |  |  |  |
| `true` | ✅ |  |  |  |  | literal/constant |
| `vertcat` | ✅ |  |  |  |  |  |
| `zeros` | ✅ | 1.807 | 0.03× | 1.16× | OK | Sig: Z = zeros(M,N). 1000x1000. 100 iters. |

## Control Flow

**Namespace:** core (keywords) — 10 ✅ + 0 ⚠️ / 11 = 91%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `break` | ✅ |  |  |  |  | keyword |
| `continue` | ✅ |  |  |  |  | keyword |
| `end` | ✅ |  |  |  |  | keyword + `A(end)` indexing form |
| `for` | ✅ |  |  |  |  | keyword |
| `if` | ✅ |  |  |  |  | keyword |
| `parfor` | ❌ |  |  |  |  | parallel — out of scope |
| `pause` | ✅ |  |  |  |  | no time.sleep |
| `return` | ✅ |  |  |  |  | keyword |
| `switch` | ✅ |  |  |  |  | keyword (`switch/case/otherwise`) |
| `try` | ✅ |  |  |  |  | keyword (`try/catch`) |
| `while` | ✅ |  |  |  |  | keyword |

## Numeric Types

**Namespace:** core — 27 ✅ + 0 ⚠️ / 29 = 93%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `allfinite` | ✅ | 0.503 | 0.10× |  | OK | Sig: TF = allfinite(X). Returns scalar (logical-scalar fp BUGS #14). 100k iters. |
| `anynan` | ✅ | 0.276 | 0.12× |  | OK | Sig: TF = anynan(X). Returns scalar. 100k iters. |
| `cast` | ✅ | 5.072 | 0.30× | 0.55× | OK | 1M doubles -> int32. 50 iters. |
| `double` | ✅ | 3.606 | 0.04× | 0.57× | OK | Sig: Y = double(X). 1M single → double. 50 iters. Element-wise SAVE. |
| `eps` | ✅ | 0.000 | 21.02× | 44.01× | OK | Sig: E = eps. Machine epsilon scalar. 1M iters. |
| `flintmax` | ✅ | 0.000 | 25.86× | 47.63× | OK | Sig: M = flintmax. Largest exact float-int. 1M iters. |
| `inf` | ✅ |  |  |  |  | constant |
| `int16` | ✅ |  |  |  |  |  |
| `int32` | ✅ | 5.063 | 0.03× | 0.54× | OK | Sig: Y = int32(X). 1M doubles → int32. 50 iters. Element-wise SAVE. |
| `int64` | ✅ |  |  |  |  |  |
| `int8` | ✅ |  |  |  |  |  |
| `intmax` | ✅ | 0.000 | 11.60× | 16.41× | OK | Sig: M = intmax(TYPE). int32 max. 1M iters. |
| `intmin` | ✅ | 0.000 | 11.16× | 4.63× | OK | Sig: M = intmin(TYPE). int32 min. 1M iters. |
| `isfinite` | ✅ | 3.404 | 0.02× | 0.07× | OK | Sig: TF = isfinite(X). 1M-pt mixed. 50 iters. |
| `isfloat` | ✅ | 0.000 | 20.26× | 26.00× | OK | Sig: TF = isfloat(X). Returns scalar. 100k iters. |
| `isinf` | ✅ | 3.383 | 0.03× | 0.07× | OK | Sig: TF = isinf(X). 1M-pt with Inf/-Inf scattered. 50 iters. |
| `isinteger` | ✅ | 0.000 | 20.54× | 16.06× | OK | Sig: TF = isinteger(X). Returns scalar. 100k iters. |
| `isnan` | ✅ | 3.391 | 0.03× | 0.07× | OK | Sig: TF = isnan(X). 1M-pt with NaN every 3rd. 50 iters. Element-wise SAVE on logical. |
| `isnumeric` | ✅ | 0.000 | 23.28× | 24.81× | OK | Sig: TF = isnumeric(X). Returns scalar. 100k iters. |
| `isreal` | ✅ | 0.000 | 18.13× | 31.18× | OK | Sig: TF = isreal(X). Returns scalar. 100k iters. |
| `nan` | ✅ |  |  |  |  | constant |
| `realmax` | ✅ | 0.000 | 30.11× | 45.22× | OK | Sig: M = realmax. Largest finite double. 1M iters. |
| `realmin` | ✅ | 0.000 | 31.22× | 26.61× | OK | Sig: M = realmin. Smallest normal double. 1M iters. |
| `single` | ✅ | 2.755 | 0.06× | 0.43× | OK | Sig: Y = single(X). 1M double → single. 50 iters. Element-wise SAVE. |
| `typecast` | ✅ | 1.059 | 0.01× | 0.97× | OK | 1M uint32 reinterpreted as 2M uint16 (LE byte order). 50 iters. |
| `uint16` | ✅ |  |  |  |  |  |
| `uint32` | ✅ | 5.264 | 0.03× | 0.49× | OK | Sig: Y = uint32(X). 1M doubles → uint32. 50 iters. Element-wise SAVE. |
| `uint64` | ✅ |  |  |  |  |  |
| `uint8` | ✅ |  |  |  |  |  |

## Characters and Strings

**Namespace:** core — 54 ✅ + 0 ⚠️ / 65 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `append` | ✅ | 0.000 | 8.59× |  | OK | Sig: S = append(S1,S2). 3k char + 'bar'. 1000 iters. |
| `blanks` | ✅ | 0.000 | 3.40× | 30.21× | OK | Sig: S = blanks(N). N=1000. 10000 iters. |
| `cellstr` | ✅ | 0.000 | 13.65× |  | MISMATCH | Sig: C = cellstr(CHAR). 3-row char mat → cellstr. 10000 iters. |
| `char` | ✅ | 0.000 | 2.37× | 11.62× | OK | Sig: S = char(X). ASCII codes A-Z. 10000 iters. |
| `compose` | ✅ | 0.391 | 0.68× | — | OK | Format 1000 ints with single-spec template. 100 iters. |
| `contains` | ✅ | 0.000 | 3.59× |  | OK | Sig: TF = contains(S, PAT). 2k char single check (cellstr/string-array forms have parity issues). 1000 iters. Logical-scalar fp (BUGS #14). |
| `convertcharstostrings` | ✅ |  |  |  |  |  |
| `convertcontainedstringstochars` | ✅ |  |  |  |  |  |
| `convertstringstochars` | ✅ |  |  |  |  |  |
| `count` | ✅ | 0.005 | 1.11× |  | OK | Sig: N = count(S, PAT). 2.2k char string. 10k iters. |
| `deblank` | ✅ | 0.000 | 4.21× | 145.71× | OK | Sig: S = deblank(S). Trim trailing space. 10000 iters. |
| `double` | ✅ | 3.606 | 0.04× | 0.57× | OK | Sig: Y = double(X). 1M single → double. 50 iters. Element-wise SAVE. |
| `endswith` | ❌ |  |  |  |  |  |
| `erase` | ✅ | 0.002 | 2.26× | 10.85× | OK | Sig: S2 = erase(S, PAT). 1.2k-char string remove 'bar '. 1000 iters. |
| `erasebetween` | ✅ |  |  |  |  |  |
| `extract` | ✅ | 0.106 | 1.00× | — | OK | Extract 'xyz' from 8000-char string with 1000 hits. 1000 iters. |
| `extractafter` | ✅ | 0.000 | 2.75× |  | OK | Sig: S2 = extractAfter(S, PAT). 10k iters. Function name camelCase. |
| `extractbefore` | ✅ | 0.000 | 2.81× |  | OK | Sig: S2 = extractBefore(S, PAT). 10k iters. |
| `extractbetween` | ✅ |  |  |  |  |  |
| `insertafter` | ✅ |  |  |  |  |  |
| `insertbefore` | ✅ |  |  |  |  |  |
| `iscellstr` | ✅ | 0.000 | 7.36× | 23.33× | OK | Sig: TF = iscellstr(X). 100k iters. |
| `ischar` | ✅ |  |  |  |  |  |
| `isletter` | ✅ | 0.034 | 0.77× | 2.18× | OK | Sig: TF = isletter(S). 14k char input. 1000 iters. Logical-array fp. |
| `isspace` | ✅ | 0.028 | 1.02× | 2.07× | OK | Sig: TF = isspace(S). 12k char input. 1000 iters. Logical-array fp. |
| `isstring` | ✅ | 0.000 | 23.96× | 66.39× | OK | Sig: TF = isstring(X). Returns scalar logical. 100k iters. |
| `isstringscalar` | ✅ |  |  |  | N/A | Sig: TF = isStringScalar(X). Camel-case fn name. 100k iters. |
| `isstrprop` | ✅ |  |  |  |  |  |
| `join` | ✅ | 0.001 | 0.27× | — | OK | Join 24-element Greek-letter string array. 10k iters. |
| `lower` | ✅ | 0.046 | 1.59× | 3.67× | OK | Sig: Y = lower(S). 32k char string with mixed case. 1000 iters. Element-wise SAVE. |
| `matches` | ✅ |  |  |  |  |  |
| `newline` | ✅ | 0.000 | 0.10× | 7.25× | OK | ASCII LF char. Bench is 100k iters of the call itself. |
| `num2str` | ✅ |  |  |  |  |  |
| `pad` | ✅ | 0.000 | 15.01× |  | OK | Sig: S2 = pad(S, LEN). Pad 'foo' to length 20. 10000 iters. |
| `plus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `regexp` | ✅ |  |  |  |  |  |
| `regexpi` | ✅ |  |  |  |  |  |
| `regexprep` | ✅ |  |  |  |  |  |
| `regexptranslate` | ✅ | 0.000 | 13.86× | 63.52× | OK | Escape 21-char string with many metachars. 10k iters. |
| `replace` | ✅ | 0.012 | 2.62× |  | OK | Sig: Y = replace(S, OLD, NEW). 16k string, 1k replacements. 1000 iters. |
| `replacebetween` | ✅ |  |  |  |  |  |
| `reverse` | ✅ | 0.000 | 8.98× |  | OK | Sig: S2 = reverse(S). 1k-char reverse. 10000 iters. |
| `split` | ✅ | 0.102 | 0.99× | — | OK | Split CSV-like 4000-char string into 1000 tokens. 1000 iters. |
| `splitlines` | ✅ | 0.001 | 3.19× |  | OK | Sig: C = splitlines(S). 5-line input via sprintf '
'. 1000 iters. |
| `sprintf` | ✅ |  |  |  |  |  |
| `sscanf` | ✅ |  |  |  |  |  |
| `startswith` | ❌ |  |  |  |  |  |
| `str2double` | ✅ |  |  |  |  |  |
| `strcat` | ✅ | 0.001 | 26.76× | 84.88× | OK | Sig: S = strcat(A, B). 5k + 6k char concat. 1000 iters. |
| `strcmp` | ✅ | 0.000 | 7.11× | 33.62× | OK | Sig: TF = strcmp(A, B). char-vs-char only. 100k iters. Logical-scalar fp (BUGS #14). |
| `strcmpi` | ✅ |  |  |  |  |  |
| `strfind` | ✅ | 0.017 | 0.71× | 0.77× | OK | Sig: K = strfind(S, PAT). 15k string, 1k matches. 1000 iters. |
| `string` | ✅ |  |  |  |  |  |
| `strings` | ✅ | 0.710 | 0.22× | — | OK | 100x100 empty-string array. 1000 iters. |
| `strip` | ✅ | 0.000 | 12.71× |  | OK | Sig: S = strip(S). Trim both. 10000 iters. |
| `strjoin` | ✅ | 0.009 | 12.80× | 89.14× | OK | Sig: S = strjoin(C, DELIM). 1k tokens via for-init (repmat rejects cell). 1000 iters. |
| `strjust` | ✅ | 0.084 | 2.07× | 2.42× | OK | 1000x50 char matrix, right-justify. 200 iters. |
| `strlength` | ✅ | 0.000 | 8.95× |  | OK | Sig: L = strlength(S). Single string (cellstr form differs). 100k iters. |
| `strncmp` | ✅ |  |  |  |  |  |
| `strncmpi` | ✅ |  |  |  |  |  |
| `strrep` | ✅ | 0.012 | 1.59× | 1.21× | OK | Sig: Y = strrep(S, OLD, NEW). 16k string, 1k replacements. 1000 iters. |
| `strsplit` | ✅ | 0.076 | 1.27× |  | MISMATCH | Sig: C = strsplit(S, DELIM). 3.5k string, 500 splits → cell. 1000 iters. Custom fp (cell out). |
| `strtok` | ✅ |  |  |  |  |  |
| `strtrim` | ✅ | 0.000 | 3.09× | 135.74× | OK | Sig: S = strtrim(S). Trim leading+trailing. 10000 iters. |
| `upper` | ✅ | 0.068 | 1.10× | 2.51× | OK | Sig: Y = upper(S). 32k char string with mixed case. 1000 iters. Element-wise SAVE. |

## Structures

**Namespace:** core — 12 ✅ + 0 ⚠️ / 14 = 86%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `arrayfun` | ✅ |  |  |  |  |  |
| `cell2struct` | ✅ | 0.000 | 4.73× | 15.26× | OK | Sig: S = cell2struct(C, FIELDS, DIM). 10k iters. |
| `fieldnames` | ✅ | 0.001 | 1.67× |  | MISMATCH | Sig: C = fieldnames(S). 5-field struct. 10k iters. Cell-out fp. |
| `getfield` | ✅ | 0.000 | 15.25× | 107.10× | OK | Sig: V = getfield(S, F). 100k iters. |
| `isfield` | ✅ | 0.000 | 6.67× | 26.00× | OK | Sig: TF = isfield(S, F). 100k iters. |
| `isstruct` | ✅ | 0.000 | 8.76× | 29.48× | OK | Sig: TF = isstruct(S). Returns scalar logical. 100k iters. |
| `orderfields` | ✅ |  |  |  |  | reorder |
| `rmfield` | ✅ | 0.000 | 13.56× | 11.47× | OK | Sig: S2 = rmfield(S, F). Remove 'c' from 5-field. 10k iters. |
| `setfield` | ✅ | 0.000 | 7.82× | 67.62× | OK | Sig: S2 = setfield(S, F, V). 10k iters. |
| `struct` | ✅ | 0.000 | 7.90× | 34.50× | OK | Sig: S = struct(name1,val1,...). 5 fields. 10k iters. Custom fp. |
| `struct2cell` | ✅ | 0.000 | 3.91× | 22.13× | OK | Sig: C = struct2cell(S). 5 fields. 10k iters. |
| `struct2table` | ❌ |  |  |  |  |  |
| `structfun` | ✅ |  |  |  |  |  |
| `table2struct` | ❌ |  |  |  |  |  |

## Cell Arrays

**Namespace:** core — 12 ✅ + 0 ⚠️ / 17 = 71%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cell` | ✅ | 0.068 | 0.08× | 2.60× | OK | Sig: C = cell(M, N). 100x100 empty cell. 1000 iters. |
| `cell2mat` | ✅ |  |  |  |  | concat cells |
| `cell2struct` | ✅ | 0.000 | 4.73× | 15.26× | OK | Sig: S = cell2struct(C, FIELDS, DIM). 10k iters. |
| `cell2table` | ❌ |  |  |  |  |  |
| `celldisp` | ✅ |  |  |  |  |  |
| `cellfun` | ✅ |  |  |  |  |  |
| `cellplot` | ❌ |  |  |  |  |  |
| `cellstr` | ✅ | 0.000 | 13.65× |  | MISMATCH | Sig: C = cellstr(CHAR). 3-row char mat → cellstr. 10000 iters. |
| `iscell` | ✅ | 0.000 | 8.30× | 36.14× | OK | Sig: TF = iscell(X). 100k iters. |
| `iscellstr` | ✅ | 0.000 | 7.36× | 23.33× | OK | Sig: TF = iscellstr(X). 100k iters. |
| `mat2cell` | ✅ |  |  |  |  | split into cell |
| `num2cell` | ✅ | 0.007 | 10.72× | 7.25× | OK | Sig: C = num2cell(A). 1k-vec wrap each. 1000 iters. |
| `string` | ✅ |  |  |  |  |  |
| `struct2cell` | ✅ | 0.000 | 3.91× | 22.13× | OK | Sig: C = struct2cell(S). 5 fields. 10k iters. |
| `table` | ❌ |  |  |  |  |  |
| `table2cell` | ❌ |  |  |  |  |  |
| `timetable` | ❌ |  |  |  |  |  |

## Function Handles

**Namespace:** core — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `feval` | ✅ |  |  |  |  | call handle by name |
| `func2str` | ✅ | 0.000 | 4.42× |  | MISMATCH | Sig: S = func2str(F). 10k iters. |
| `function_handle` | ❌ |  |  |  |  | OOP class |
| `functions` | ✅ |  |  |  |  | introspection |
| `localfunctions` | ✅ |  |  |  |  | (stub: empty cell) |
| `str2func` | ✅ | 0.000 | 14.84× | 19.64× | OK | Sig: F = str2func(NAME). 10k iters. fp checks created handle works. |

## Categorical Arrays

**Namespace:** `categorical.*` (future) — 1 ✅ + 0 ⚠️ / 17 = 5%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `addcats` | ❌ |  |  |  |  |  |
| `categorical` | ❌ |  |  |  |  |  |
| `categories` | ❌ |  |  |  |  |  |
| `combinations` | ❌ |  |  |  |  | all combinations |
| `countcats` | ❌ |  |  |  |  |  |
| `discretize` | ✅ |  |  |  |  |  |
| `iscategory` | ❌ |  |  |  |  |  |
| `isordinal` | ❌ |  |  |  |  |  |
| `isprotected` | ❌ |  |  |  |  |  |
| `isundefined` | ❌ |  |  |  |  |  |
| `mergecats` | ❌ |  |  |  |  |  |
| `removecats` | ❌ |  |  |  |  |  |
| `renamecats` | ❌ |  |  |  |  |  |
| `reordercats` | ❌ |  |  |  |  |  |
| `setcats` | ❌ |  |  |  |  |  |
| `summary` | ❌ |  |  |  |  |  |

## Tables / Timetables

**Namespace:** `table.*` (future) — 6 ✅ + 0 ⚠️ / 66 = 9%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `addprop` | ❌ |  |  |  |  |  |
| `addvars` | ❌ |  |  |  |  |  |
| `anymissing` | ❌ |  |  |  |  |  |
| `array2table` | ❌ |  |  |  |  |  |
| `cell2table` | ❌ |  |  |  |  |  |
| `computebygroup` | ❌ |  |  |  |  |  |
| `convertvars` | ❌ |  |  |  |  |  |
| `fillmissing` | ❌ |  |  |  |  |  |
| `findgroups` | ❌ |  |  |  |  |  |
| `groupcounts` | ❌ |  |  |  |  |  |
| `groupfilter` | ❌ |  |  |  |  |  |
| `groupsummary` | ❌ |  |  |  |  |  |
| `grouptransform` | ❌ |  |  |  |  |  |
| `head` | ✅ | 0.000 | 59.14× |  | OK | Sig: Y = head(X, K). First 100 elements. 10000 iters. |
| `height` | ❌ |  |  |  |  |  |
| `inner2outer` | ❌ |  |  |  |  |  |
| `innerjoin` | ❌ |  |  |  |  |  |
| `intersect` | ✅ | 0.623 | 0.43× | 0.42× | OK | Sig: C = intersect(A, B). 10k vs 10k overlap. 100 iters. Element-wise SAVE. |
| `ismember` | ✅ | 1.359 | 0.30× | 0.55× | OK | Sig: TF = ismember(A, B). 100k vs 20k members. 50 iters. Element-wise SAVE on logical. |
| `ismissing` | ❌ |  |  |  |  |  |
| `issortedrows` | ❌ |  |  |  |  |  |
| `join` | ✅ | 0.001 | 0.27× | — | OK | Join 24-element Greek-letter string array. 10k iters. |
| `jointables` | ❌ |  |  |  |  |  |
| `mergevars` | ❌ |  |  |  |  |  |
| `movevars` | ❌ |  |  |  |  |  |
| `outerjoin` | ❌ |  |  |  |  |  |
| `parquetread` | ❌ |  |  |  |  |  |
| `parquetwrite` | ❌ |  |  |  |  |  |
| `pivot` | ❌ |  |  |  |  |  |
| `pivottable` | ❌ |  |  |  |  |  |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `removevars` | ❌ |  |  |  |  |  |
| `renamevars` | ❌ |  |  |  |  |  |
| `rmmissing` | ❌ |  |  |  |  |  |
| `rmprop` | ❌ |  |  |  |  |  |
| `rowfun` | ❌ |  |  |  |  |  |
| `rows2vars` | ❌ |  |  |  |  |  |
| `setdiff` | ✅ | 0.591 | 0.52× | 0.50× | OK | Sig: C = setdiff(A, B). 10k minus 10k. 100 iters. Element-wise SAVE. |
| `setxor` | ✅ | 0.971 | 0.57× | 0.35× | OK | Sig: C = setxor(A, B). 10k symdiff 10k. 100 iters. Element-wise SAVE. |
| `sortrows` | ✅ | 0.425 | 0.74× | 0.19× | OK | Sig: B = sortrows(A). 10k×3 sort by first col. 100 iters. |
| `splitapply` | ❌ |  |  |  |  |  |
| `splitvars` | ❌ |  |  |  |  |  |
| `stack` | ❌ |  |  |  |  |  |
| `stackedplot` | ❌ |  |  |  |  |  |
| `stacktablevariables` | ❌ |  |  |  |  |  |
| `standardizemissing` | ❌ |  |  |  |  |  |
| `struct2table` | ❌ |  |  |  |  |  |
| `summary` | ❌ |  |  |  |  |  |
| `table` | ❌ |  |  |  |  |  |
| `table2array` | ❌ |  |  |  |  |  |
| `table2cell` | ❌ |  |  |  |  |  |
| `table2struct` | ❌ |  |  |  |  |  |
| `table2timetable` | ❌ |  |  |  |  |  |
| `tail` | ✅ | 0.000 | 54.28× |  | OK | Sig: Y = tail(X, K). Last 100 elements. 10000 iters. |
| `timetable2table` | ❌ |  |  |  |  |  |
| `topkrows` | ❌ |  |  |  |  |  |
| `union` | ✅ | 1.183 | 0.33× | 0.15× | OK | Sig: C = union(A, B). 10k union 10k. 100 iters. Element-wise SAVE. |
| `unique` | ✅ | 0.931 | 1.08× | 0.35× | OK | Sig: C = unique(A). 100k with ~7919 distinct. 100 iters. Element-wise SAVE. |
| `unstack` | ❌ |  |  |  |  |  |
| `unstacktablevariables` | ❌ |  |  |  |  |  |
| `varfun` | ❌ |  |  |  |  |  |
| `vartype` | ❌ |  |  |  |  |  |
| `width` | ❌ |  |  |  |  |  |
| `writetable` | ❌ |  |  |  |  | needs table type |

## Bit-wise Operations

**Namespace:** core — 7 ✅ + 0 ⚠️ / 8 = 88%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bitand` | ✅ | 5.839 | 1.85× | 2.29× | OK | Sig: Y = bitand(A, B). 1M double (numkit rejects uint32 — see BUGS #13). 50 iters. |
| `bitcmp` | ✅ | 4.817 | 0.50× |  | OK | Sig: Y = bitcmp(A, type). 1M double + 'uint32' (numkit rejects uint32 array — see BUGS #13). 50 iters. |
| `bitget` | ✅ | 3.916 | 0.63× | 2.51× | OK | Sig: Y = bitget(A, K). 1M double, bit 3. 50 iters. |
| `bitor` | ✅ | 6.197 | 1.74× | 2.18× | OK | Sig: Y = bitor(A, B). 1M double. 50 iters. |
| `bitset` | ✅ | 4.155 | 0.61× | 8.75× | OK | Sig: Y = bitset(A, K). 1M double, set bit 5. 50 iters. |
| `bitshift` | ✅ | 4.447 | 0.56× | 1.74× | OK | Sig: Y = bitshift(A, K). 1M double << 3. 50 iters. |
| `bitxor` | ✅ | 5.789 | 1.87× | 2.28× | OK | Sig: Y = bitxor(A, B). 1M double. 50 iters. |
| `swapbytes` | ✅ | 1.070 | 0.95× | 8.06× | OK | Sig: Y = swapbytes(X). 1M uint32 endian-swap. 50 iters. (uint out — fp via double cast). |

## Set Operations

**Namespace:** core — 10 ✅ + 0 ⚠️ / 13 = 77%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `allunique` | ✅ | 0.100 | 0.67× |  | OK | Sig: TF = allunique(X). 10k unique values. 1000 iters. |
| `innerjoin` | ❌ |  |  |  |  |  |
| `intersect` | ✅ | 0.623 | 0.43× | 0.42× | OK | Sig: C = intersect(A, B). 10k vs 10k overlap. 100 iters. Element-wise SAVE. |
| `ismember` | ✅ | 1.359 | 0.30× | 0.55× | OK | Sig: TF = ismember(A, B). 100k vs 20k members. 50 iters. Element-wise SAVE on logical. |
| `ismembertol` | ✅ | 0.769 | 0.31× | 7.59× | OK | Sig: TF = ismembertol(A, B, TOL). 10k vs 100 with tol=0.005. 100 iters. |
| `join` | ✅ | 0.001 | 0.27× | — | OK | Join 24-element Greek-letter string array. 10k iters. |
| `numunique` | ✅ | 0.029 | 3.45× |  | OK | Sig: N = numunique(X). 10k with 137 distinct. 1000 iters. |
| `outerjoin` | ❌ |  |  |  |  |  |
| `setdiff` | ✅ | 0.591 | 0.52× | 0.50× | OK | Sig: C = setdiff(A, B). 10k minus 10k. 100 iters. Element-wise SAVE. |
| `setxor` | ✅ | 0.971 | 0.57× | 0.35× | OK | Sig: C = setxor(A, B). 10k symdiff 10k. 100 iters. Element-wise SAVE. |
| `union` | ✅ | 1.183 | 0.33× | 0.15× | OK | Sig: C = union(A, B). 10k union 10k. 100 iters. Element-wise SAVE. |
| `unique` | ✅ | 0.931 | 1.08× | 0.35× | OK | Sig: C = unique(A). 100k with ~7919 distinct. 100 iters. Element-wise SAVE. |
| `uniquetol` | ✅ | 0.234 | 0.49× | 6.98× | MISMATCH | Sig: U = uniquetol(X, TOL). 10k with rounded vals. 100 iters. |

## Arithmetic

**Namespace:** core — 28 ✅ + 0 ⚠️ / 34 = 82%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bsxfun` | ✅ |  |  |  |  | legacy broadcast |
| `ceil` | ✅ | 4.028 | 0.14× | 0.87× | OK | Sig: Y = ceil(X). 1M-pt sweep with non-integer offset. 20 iters. Element-wise SAVE. |
| `ctranspose` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `cumprod` | ✅ | 0.002 | 13.53× | 24.37× | OK | Sig: Y = cumprod(X). 1k-pt cumprod near 1 (avoid overflow). 20 iters. |
| `cumsum` | ✅ | 2.579 | 1.13× | 1.01× | OK | Sig: Y = cumsum(X). 1M-pt cumulative sum (default dim). 20 iters. |
| `diff` | ✅ | 4.714 | 0.31× | 0.50× | OK | Sig: Y = diff(X). 1M-pt adjacent differences. 20 iters. Element-wise SAVE. |
| `fix` | ✅ | 5.126 | 0.08× | 0.71× | OK | Sig: Y = fix(X). 1M-pt sweep with non-integer offset. 20 iters. Element-wise SAVE. |
| `floor` | ✅ | 4.055 | 0.10× | 0.88× | OK | Sig: Y = floor(X). 1M-pt sweep with non-integer offset. 20 iters. Element-wise SAVE. |
| `idivide` | ✅ |  |  |  |  | integer division |
| `ldivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `minus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mldivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mod` | ✅ | 3.384 | 0.30× | 1.45× | OK | Sig: Y = mod(X, D). 1M-pt with scalar divisor 7. 20 iters. Element-wise SAVE. |
| `movsum` | ✅ | 4.686 | 0.35× | 19.19× | OK | Sig: Y = movsum(X, K). 1M-pt moving window K=5. 20 iters. Element-wise SAVE. |
| `mpower` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mrdivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mtimes` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `pagectranspose` | ✅ | 0.207 | 0.24× | 0.23× | OK | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pagemldivide` | ❌ |  |  |  |  |  |
| `pagemrdivide` | ❌ |  |  |  |  |  |
| `pagemtimes` | ✅ |  |  |  |  |  |
| `pagetranspose` | ✅ | 0.083 | 1.11× | 0.63× | OK | 128x64x8 array, page-wise transpose. 100 iters. |
| `plus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `power` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `prod` | ✅ | 0.002 | 11.52× | 20.71× | OK | Sig: Y = prod(X). 1k-pt reduction near 1 (avoid overflow). 20 iters. |
| `rdivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `rem` | ✅ | 4.909 | 0.15× | 0.96× | OK | Sig: Y = rem(X, D). 1M-pt with scalar divisor 7. 20 iters. Element-wise SAVE. |
| `round` | ✅ | 4.992 | 0.11× | 0.72× | OK | Sig: Y = round(X). 1M-pt sweep with non-half offset. 20 iters. Element-wise SAVE. |
| `sum` | ✅ | 1.378 | 0.05× | 0.29× | OK | Sig: Y = sum(X). 1M-pt full reduction (default dim). 20 iters. |
| `tensorprod` | ❌ |  |  |  |  | tensor contraction |
| `times` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `transpose` | ✅ | 7.375 | 0.20× | 0.35× | OK | Sig: Y = transpose(X). 1k×1k transpose. 100 iters. Element-wise SAVE. |
| `uminus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `uplus` | ✅ |  |  |  |  | named-fn form added in Pack 11 |

## Trigonometry

**Namespace:** core — 47 ✅ + 0 ⚠️ / 47 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `acos` | ✅ | 6.911 | 0.23× | 1.15× | OK | Sig: Y = acos(X). 1M-pt sweep on [-1, 1]. 20 iters. Element-wise SAVE. |
| `acosd` | ✅ | 6.752 | 0.23× | 1.92× | OK | Sig: Y = acosd(X). 1M-pt sweep on [-1,1]. Inverse trig (degrees). 20 iters. Element-wise SAVE. tol relaxed to 1e-10 because acos derivative diverges near x=±1 (1 elem rel diff 1.11e-12 at x≈0.99993, algorithmically correct). |
| `acosh` | ✅ | 9.405 | 0.22× | 1.02× | OK | Sig: Y = acosh(X). 1M-pt on [1,10] (domain X>=1). 20 iters. Element-wise SAVE. |
| `acot` | ✅ | 6.452 | 0.09× | 1.81× | OK | Sig: Y = acot(X). 1M-pt on [0.1,10] (avoid 0 singularity). 20 iters. Element-wise SAVE. |
| `acotd` | ✅ | 6.589 | 0.09× | 2.13× | OK | Sig: Y = acotd(X). 1M-pt (degrees). 20 iters. Element-wise SAVE. |
| `acoth` | ✅ | 8.760 | 0.28× | 1.92× | OK | Sig: Y = acoth(X). 1M-pt on (1,10] (domain |X|>1). 20 iters. Element-wise SAVE. |
| `acsc` | ✅ | 6.491 | 0.22× | 2.40× | OK | Sig: Y = acsc(X). 1M-pt domain |X|>=1. 20 iters. |
| `acscd` | ✅ | 6.601 | 0.23× | 3.04× | OK | Sig: Y = acscd(X). 1M-pt deg. 20 iters. |
| `acsch` | ✅ | 11.801 | 0.08× | 1.31× | OK | Sig: Y = acsch(X). 1M-pt avoid 0 (X != 0). 20 iters. |
| `asec` | ✅ | 6.587 | 0.23× | 2.32× | OK | Sig: Y = asec(X). 1M-pt domain |X|>=1. 20 iters. |
| `asecd` | ✅ | 6.762 | 0.23× | 3.05× | OK | Sig: Y = asecd(X). 1M-pt deg. 20 iters. |
| `asech` | ✅ | 10.153 | 0.21× | 1.69× | OK | Sig: Y = asech(X). 1M-pt domain (0,1]. 20 iters. |
| `asin` | ✅ | 6.750 | 0.24× | 1.23× | OK | Sig: Y = asin(X). 1M-pt sweep on [-1, 1]. 20 iters. Element-wise SAVE. |
| `asind` | ✅ | 6.855 | 0.29× | 1.95× | OK | Sig: Y = asind(X). 1M-pt on [-1,1]. Inverse (degrees). 20 iters. Element-wise SAVE. |
| `asinh` | ✅ | 10.521 | 0.13× | 0.77× | OK | Sig: Y = asinh(X). 1M-pt on [-10,10]. 20 iters. Element-wise SAVE. |
| `atan` | ✅ | 6.363 | 0.09× | 0.69× | OK | Sig: Y = atan(X). 1M-pt sweep on [-10, 10]. 20 iters. Element-wise SAVE. |
| `atan2` | ✅ | 10.641 | 0.07× | 0.73× | OK | Sig: P = atan2(Y, X). 1000x1000 quadrant grid. 20 iters. Element-wise SAVE. |
| `atan2d` | ✅ | 10.705 | 0.09× | 0.95× | OK | Sig: Z = atan2d(Y, X). 1k×1k quadrant grid (degrees). 20 iters. Element-wise SAVE. |
| `atand` | ✅ | 6.528 | 0.11× | 1.04× | OK | Sig: Y = atand(X). 1M-pt on [-10,10]. Inverse (degrees). 20 iters. Element-wise SAVE. |
| `atanh` | ✅ | 8.309 | 0.33× | 1.12× | OK | Sig: Y = atanh(X). 1M-pt on (-1,1) (avoid pole). 20 iters. Element-wise SAVE. |
| `cart2pol` | ✅ | 17.127 | 0.19× | 1.34× | OK | Sig: [TH,R] = cart2pol(X,Y) (2-D). 1000x1000 grid. 3-D form [TH,R,Z] = cart2pol(X,Y,Z) not benched yet. 20 iters. |
| `cart2sph` | ✅ |  |  |  | N/A | Sig: [TH,PHI,R] = cart2sph(X,Y,Z). 50³ grid. 50 iters. SAVE on TH (y). |
| `cos` | ✅ | 0.855 | 1.03× | 5.49× | OK | Sig: Y = cos(X). 1M-point sweep over 4π. 20 iters. Element-wise SAVE. |
| `cosd` | ✅ | 10.733 | 0.09× | 2.07× | OK | Sig: Y = cosd(X). 1M-pt sweep on [-720°, 720°]. degree variant. 20 iters. Element-wise SAVE. |
| `cosh` | ✅ | 8.197 | 0.11× | 0.69× | OK | Sig: Y = cosh(X). 1M-pt sweep on [-3, 3]. 20 iters. Element-wise SAVE. |
| `cospi` | ✅ | 9.236 | 0.07× | 1.93× | OK | Sig: Y = cospi(X) = cos(π·X). 1M-pt sweep on [-2, 2]. 20 iters. Element-wise SAVE. |
| `cot` | ✅ | 8.745 | 0.13× | 1.56× | OK | Sig: Y = cot(X). 1M-pt on (0, π) avoiding 0/π poles. 20 iters. |
| `cotd` | ✅ | 14.327 | 0.07× | 2.49× | OK | Sig: Y = cotd(X). 1M-pt deg, avoid 0/180. 20 iters. |
| `coth` | ✅ | 10.989 | 0.12× | 1.32× | OK | Sig: Y = coth(X). 1M-pt avoid 0 pole. 20 iters. |
| `csc` | ✅ | 7.207 | 0.12× | 1.69× | OK | Sig: Y = csc(X). 1M-pt on (0, π). 20 iters. |
| `cscd` | ✅ | 9.650 | 0.07× | 2.85× | OK | Sig: Y = cscd(X). 1M-pt deg. 20 iters. |
| `csch` | ✅ | 8.617 | 0.14× | 1.52× | OK | Sig: Y = csch(X). 1M-pt avoid 0 pole. 20 iters. |
| `deg2rad` | ✅ | 4.090 | 0.33× | 0.61× | OK | Sig: R = deg2rad(D). 1M-pt sweep. 20 iters. |
| `hypot` | ✅ | 6.636 | 0.17× | 0.75× | OK | Sig: Y = hypot(A, B). 1k×1k grid. 20 iters. Element-wise SAVE. |
| `pol2cart` | ✅ | 15.792 |  | 0.99× | OK | Sig: [X,Y]=pol2cart(TH,R). 1k×1k grid. 20 iters. SAVE on X. |
| `rad2deg` | ✅ | 3.942 | 0.36× | 0.60× | OK | Sig: D = rad2deg(R). 1M-pt sweep. 20 iters. |
| `sec` | ✅ | 6.642 | 0.10× | 1.77× | OK | Sig: Y = sec(X). 1M-pt on [-1.5, 1.5] (avoid π/2). 20 iters. Element-wise SAVE. |
| `secd` | ✅ | 9.655 | 0.08× | 3.07× | OK | Sig: Y = secd(X). 1M-pt on [-89°, 89°]. 20 iters. Element-wise SAVE. |
| `sech` | ✅ | 8.684 | 0.11× | 1.52× | OK | Sig: Y = sech(X). 1M-pt on [-5, 5]. 20 iters. Element-wise SAVE. |
| `sin` | ✅ | 0.845 | 1.07× | 5.61× | OK | Sig: Y = sin(X). 1M-point sweep over 4π. 20 iters. Element-wise SAVE. |
| `sind` | ✅ | 10.629 | 0.07× | 1.92× | OK | Sig: Y = sind(X). 1M-pt sweep on [-720°, 720°]. degree variant. 20 iters. Element-wise SAVE. |
| `sinh` | ✅ | 8.339 | 0.15× | 0.69× | OK | Sig: Y = sinh(X). 1M-pt sweep on [-3, 3]. 20 iters. Element-wise SAVE. |
| `sinpi` | ✅ | 9.110 | 0.09× | 1.90× | OK | Sig: Y = sinpi(X) = sin(π·X). 1M-pt sweep on [-2, 2]. 20 iters. Element-wise SAVE. |
| `sph2cart` | ✅ |  |  |  | N/A | Sig: [X,Y,Z] = sph2cart(TH,PH,R). 50³ grid. 50 iters. SAVE on X (y). |
| `tan` | ✅ | 7.283 | 0.12× | 0.73× | OK | Sig: Y = tan(X). 1M-point sweep on [-1.5, 1.5] (avoid π/2 singularity). 20 iters. Element-wise SAVE. |
| `tand` | ✅ | 10.176 | 0.09× | 2.59× | OK | Sig: Y = tand(X). 1M-pt sweep on [-89°, 89°] (avoid 90° singularity). 20 iters. Element-wise SAVE. |
| `tanh` | ✅ | 9.677 | 0.13× | 0.72× | OK | Sig: Y = tanh(X). 1M-pt sweep on [-5, 5]. 20 iters. Element-wise SAVE. |

## Exponents and Logarithms

**Namespace:** core — 13 ✅ + 0 ⚠️ / 13 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `exp` | ✅ | 0.892 | 0.84× | 4.94× | OK | Sig: Y = exp(X). 1M-pt sweep on [-10,10]. 20 iters. Element-wise SAVE. |
| `expm1` | ✅ | 6.865 | 0.11× | 0.70× | OK | Sig: Y = expm1(X) = exp(X)-1. 1M-pt on [-2,2]. 20 iters. Element-wise SAVE. |
| `log` | ✅ | 0.762 | 2.60× | 10.62× | OK | Sig: Y = log(X). 1M-pt on [0.001, 100]. 20 iters. Element-wise SAVE. |
| `log10` | ✅ | 6.206 | 0.39× | 1.31× | OK | Sig: Y = log10(X). 1M-pt on [0.001, 1000]. 20 iters. Element-wise SAVE. |
| `log1p` | ✅ | 6.747 | 0.29× | 1.33× | OK | Sig: Y = log1p(X) = log(1+X). 1M-pt on [-0.5, 5] (avoid X=-1). 20 iters. Element-wise SAVE. |
| `log2` | ✅ | 8.248 | 0.30× | 1.90× | OK | Sig: Y = log2(X). 1M-pt on [0.001, 1024]. 20 iters. Element-wise SAVE. |
| `nextpow2` | ✅ |  |  |  | N/A | Sig: Y = nextpow2(X). 1M-pt integer-ish on [1, 1e6]. 20 iters. Element-wise SAVE. |
| `nthroot` | ✅ | 10.025 | 1.72× | 1.00× | OK | Sig: Y = nthroot(X, N). N=3, X on [0.001, 100]. 20 iters. Element-wise SAVE. |
| `pow2` | ✅ | 5.549 | 0.74× | 0.61× | OK | Sig: Y = pow2(X) = 2.^X. 1M-pt on [-50, 50]. 20 iters. Element-wise SAVE. |
| `reallog` | ✅ | 6.065 | 0.35× | 1.40× | OK | Sig: Y = reallog(X). Strict positive domain. 1M-pt on [0.001, 100]. 20 iters. Element-wise SAVE. |
| `realpow` | ✅ | 12.251 | 0.48× | 1.36× | OK | Sig: Z = realpow(X,Y). 1k×1k grid of x>0, real exp. 20 iters. Element-wise SAVE. |
| `realsqrt` | ✅ | 4.286 | 0.33× | 1.89× | OK | Sig: Y = realsqrt(X). 1M-pt on [0, 1000]. 20 iters. Element-wise SAVE. |
| `sqrt` | ✅ |  |  |  |  |  |

## Special Functions

**Namespace:** core — 20 ✅ + 0 ⚠️ / 24 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `airy` | ✅ | 7.487 | 0.10× | 0.38× | OK | Ai over 10k pts on [-5,5]. 10 iters. Element-wise comparison. |
| `besselh` | ✅ |  |  |  |  |  |
| `besseli` | ✅ |  |  |  |  |  |
| `besselj` | ✅ |  |  |  |  |  |
| `besselk` | ✅ |  |  |  |  |  |
| `bessely` | ✅ |  |  |  |  |  |
| `beta` | ✅ | 63.067 | 0.11× | 0.70× | OK | Sig: Y = beta(Z, W). 1000x1000 grid. 20 iters. Element-wise SAVE. |
| `betainc` | ✅ | 0.095 | 1.09× | 3.09× | OK | Sig: I = betainc(X, A, B). 1k-pt with scalar a=2.5 b=4. 20 iters. Element-wise SAVE. |
| `betaincinv` | ✅ | 1.119 | 1.04× | 4.43× | OK | Inverse regularized beta over 2k probability points, a=3 b=5. 20 iters, element-wise. |
| `betaln` | ✅ | 149.841 | 0.05× | 0.25× | OK | Sig: Y = betaln(Z, W). 1000x1000 grid. 20 iters. Element-wise SAVE. |
| `ellipj` | ✅ | 0.614 | 2.23× | 1.41× | OK | Jacobi sn over 5k pts at m=0.7. 50 iters, element-wise on sn. |
| `ellipke` | ✅ |  |  |  |  |  |
| `erf` | ✅ | 9.174 | 0.28× | 0.78× | OK | smoke-test (already implemented). N=1e6, mean over 10 iters. |
| `erfc` | ✅ | 12.800 | 0.21× | 0.84× | OK | Sig: Y = erfc(X). 1M-pt sweep. 20 iters. Element-wise SAVE. |
| `erfcinv` | ✅ | 46.123 | 0.08× | 0.28× | OK | Sig: Y = erfcinv(X). 1M-pt sweep on (0,2). 20 iters. Element-wise SAVE. |
| `erfcx` | ✅ | 8.660 | 0.21× | 0.45× | OK | Sig: Y = erfcx(X) = exp(X^2)*erfc(X). 1M-pt. 20 iters. Element-wise SAVE. |
| `erfinv` | ✅ | 45.836 | 0.08× | 0.28× | OK | Sig: Y = erfinv(X). 1M-pt sweep avoiding singularities. 20 iters. Element-wise SAVE. |
| `expint` | ✅ | 4.783 | 3.12× | 8.57× | OK | Sig: Y = expint(X). 100k-pt on (0,50]. 20 iters. Element-wise SAVE. |
| `gamma` | ✅ | 1.306 | 0.28× | 0.84× | OK | Sig: Y = gamma(X). 100k-pt sweep on (0,10]. 20 iters. Element-wise SAVE. |
| `gammainc` | ✅ |  |  |  |  |  |
| `gammaincinv` | ✅ | 1.725 | 1.18× | 23.31× | OK | Inverse regularized gamma over 5k probability points, a=2.5. 20 iters, element-wise. |
| `gammaln` | ✅ | 3.523 | 0.09× | 0.24× | OK | Sig: Y = gammaln(X). 100k-pt sweep on (0,100]. 20 iters. Element-wise SAVE. |
| `legendre` | ✅ | 0.039 | 12.03× | 6.21× | OK | Sig: P = legendre(N, X). N=4, 1k pts. 20 iters. SAVE on (n+1)x1000 matrix. |
| `psi` | ✅ | 0.689 | 0.81× | 1.05× | OK | Sig: Y = psi(X). 100k-pt sweep on positive domain. 20 iters. Element-wise SAVE. |

## Discrete Math

**Namespace:** core — 10 ✅ + 0 ⚠️ / 11 = 90%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `factor` | ✅ |  |  |  |  |  |
| `factorial` | ✅ | 0.000 | 39.29× | 39.98× | OK | Sig: Y = factorial(N). N=0:20. 1k iters. Element-wise SAVE. |
| `gcd` | ✅ | 0.012 | 9.98× | 3.05× | OK | Sig: G = gcd(A, B). 1k-pt vector pair. 100 iters. Element-wise SAVE. |
| `isprime` | ✅ | 0.132 | 3.35× | 46.77× | OK | Sig: TF = isprime(X). 1..10000. 20 iters. Element-wise SAVE on logical. |
| `lcm` | ✅ | 0.014 | 11.69× | 6.69× | OK | Sig: L = lcm(A, B). 1k-pt vector pair. 100 iters. Element-wise SAVE. |
| `matchpairs` | ❌ |  |  |  |  |  |
| `nchoosek` | ✅ | 0.007 | 28.74× | 4.32× | OK | Sig: C = nchoosek(N, K). N=30, K=0:30 via for-loop (arrayfun-wrap broken in numkit, see BUGS.md #11). 1 iter. |
| `perms` | ✅ |  |  |  |  |  |
| `primes` | ✅ | 0.286 | 1.00× | 2.30× | OK | Sig: P = primes(N). N=100000. 50 iters. Element-wise SAVE. |
| `rat` | ✅ |  |  |  |  |  |
| `rats` | ✅ |  |  |  |  |  |

## Polynomials

**Namespace:** core — 10 ✅ + 0 ⚠️ / 12 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `conv` | ✅ | 0.025 | 0.78× | 1.24× | OK | Sig: C = conv(A, B). Deterministic 1k * 100 conv. 100 iters. Element-wise SAVE. |
| `deconv` | ✅ | 0.001 |  | 67.45× | OK | Sig: [Q,R] = deconv(U, V). Polynomial division. 10k iters. |
| `poly` | ✅ |  |  |  |  | roots → coeffs |
| `polyder` | ✅ | 0.001 | 71.29× | 28.02× | MISMATCH | Sig: K = polyder(P). Deterministic 100-coef poly. 1000 iters. Element-wise SAVE. |
| `polydiv` | ✅ |  |  |  |  |  |
| `polyeig` | ❌ |  |  |  |  | poly eig |
| `polyfit` | ✅ | 0.054 | 0.92× | 1.53× | OK | Sig: P = polyfit(X, Y, N). Deterministic 1k pts (sin), 5th-order fit. 100 iters. tol=1e-9 (LSQ residual noise). |
| `polyint` | ✅ | 0.001 | 15.55× | 28.71× | OK | Sig: P_int = polyint(P). Deterministic 100-coef. 1000 iters. Element-wise SAVE. |
| `polyval` | ✅ | 3.318 | 0.81× | 7.69× | OK | Sig: Y = polyval(P, X). 4th-order poly on 1M pts. 20 iters. Element-wise SAVE. |
| `polyvalm` | ✅ |  |  |  |  | matrix poly eval |
| `residue` | ❌ |  |  |  |  | partial-fraction |
| `roots` | ✅ | 0.001 | 21.54× | 38.26× | OK | Sig: R = roots(P). 4th-order poly with real roots {1,2,3,4}. 1000 iters. SAVE on sorted real parts. |

## Linear Algebra

**Namespace:** `linalg.*` (future) — 12 ✅ + 0 ⚠️ / 82 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `balance` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `bandwidth` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `cdf2rdf` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `chol` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `cholupdate` | ❌ |  |  |  |  |  |
| `cond` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `condeig` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `condest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `cross` | ✅ | 0.000 | 19.03× | 187.75× | OK | Sig: C = cross(A, B). Single 3-vec pair (numkit batch unsupported — see BUGS). 100k iters. |
| `ctranspose` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `decomposition` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `det` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `dot` | ✅ | 2.036 | 0.02× | 0.07× | OK | Sig: D = dot(A, B). 1M-elem dot product. 100 iters. Scalar fp. |
| `eig` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `eigs` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `expm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `expmv` | ❌ |  |  |  |  |  |
| `funm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `gsvd` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `hess` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `inv` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `isbanded` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `isdiag` | ❌ |  |  |  |  |  |
| `ishermitian` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `issymmetric` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `istril` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `istriu` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `kron` | ✅ | 0.189 | 0.51× | 0.12× | OK | Sig: K = kron(A, B). 10x10 ⊗ 20x20 = 200x200. 100 iters. |
| `ldl` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `linsolve` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `logm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `lscov` | ❌ |  |  |  |  |  |
| `lsqminnorm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `lsqnonneg` | ❌ |  |  |  |  |  |
| `lu` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `mldivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mpower` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mrdivide` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `mtimes` | ✅ |  |  |  |  | named-fn form added in Pack 11 |
| `norm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `normest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `null` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordeig` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordqz` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `ordschur` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `orth` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `pagectranspose` | ✅ | 0.207 | 0.24× | 0.23× | OK | 128x64x8 real-valued — pagectranspose equals pagetranspose. 100 iters. |
| `pageeig` | ❌ |  |  |  |  |  |
| `pageinv` | ❌ |  |  |  |  |  |
| `pagelsqminnorm` | ❌ |  |  |  |  |  |
| `pagemldivide` | ❌ |  |  |  |  |  |
| `pagemrdivide` | ❌ |  |  |  |  |  |
| `pagemtimes` | ✅ |  |  |  |  |  |
| `pagenorm` | ❌ |  |  |  |  |  |
| `pagepinv` | ❌ |  |  |  |  |  |
| `pagesvd` | ❌ |  |  |  |  |  |
| `pagetranspose` | ✅ | 0.083 | 1.11× | 0.63× | OK | 128x64x8 array, page-wise transpose. 100 iters. |
| `pinv` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `planerot` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `polyeig` | ❌ |  |  |  |  | poly eig |
| `qr` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `qrdelete` | ❌ |  |  |  |  |  |
| `qrinsert` | ❌ |  |  |  |  |  |
| `qrupdate` | ❌ |  |  |  |  |  |
| `qz` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rank` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rcond` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `rref` | ❌ |  |  |  |  |  |
| `rsf2csf` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `schur` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `sqrtm` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `subspace` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `svd` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `svdappend` | ❌ |  |  |  |  |  |
| `svds` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `svdsketch` | ❌ |  |  |  |  |  |
| `sylvester` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `trace` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `transpose` | ✅ | 7.375 | 0.20× | 0.35× | OK | Sig: Y = transpose(X). 1k×1k transpose. 100 iters. Element-wise SAVE. |
| `tril` | ✅ | 2.260 | 0.88× | 0.94× | OK | Sig: L = tril(A). 1k×1k lower triangular. 100 iters. |
| `triu` | ✅ | 2.255 | 0.89× | 0.97× | OK | Sig: U = triu(A). 1k×1k upper triangular. 100 iters. |
| `vecnorm` | ❌ |  |  |  |  | **deferred — libs/linalg** |

## Random Number Generation

**Namespace:** core — 5 ✅ + 0 ⚠️ / 6 = 83%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `rand` | ✅ | 6.807 | 0.51× | 0.81× | OK | Sig: A = rand(M,N). 1k×1k uniform. 100 iters. Custom fp (RNG diffs). |
| `randi` | ✅ |  |  |  |  |  |
| `randn` | ✅ |  |  |  |  |  |
| `randperm` | ✅ |  |  |  |  |  |
| `randstream` | ❌ |  |  |  |  |  |
| `rng` | ✅ |  |  |  |  |  |

## Interpolation

**Namespace:** core — 11 ✅ + 0 ⚠️ / 18 = 61%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `griddata` | ❌ |  |  |  |  |  |
| `griddatan` | ❌ |  |  |  |  |  |
| `griddedinterpolant` | ❌ |  |  |  |  |  |
| `interp1` | ✅ | 0.123 | 1.16× | 8.36× | OK | Sig: VQ = interp1(X, V, XQ). 100 → 10k linear interp. 100 iters. |
| `interp2` | ✅ |  |  |  | N/A | Sig: Vq = interp2(X,Y,V,Xq,Yq). 50x50 → 200x200 bilinear. 50 iters. |
| `interp3` | ✅ |  |  |  | N/A | Sig: Vq = interp3(X,Y,Z,V,Xq,Yq,Zq). 20³ → 50³ trilinear. 10 iters. |
| `interpft` | ✅ | 0.012 | 2.33× | 16.15× | OK | 256-pt band-limited signal interpolated to 1024 points. 200 iters, element-wise. |
| `interpn` | ✅ |  |  |  |  |  |
| `makima` | ❌ |  |  |  |  |  |
| `meshgrid` | ✅ |  |  |  | N/A | Sig: [X,Y] = meshgrid(x,y). 1k×1k grid. 100 iters. SAVE on X. |
| `mkpp` | ✅ |  |  |  |  |  |
| `ndgrid` | ✅ |  |  |  | N/A | Sig: [X,Y] = ndgrid(x,y). 1k×1k grid. 100 iters. |
| `padecoef` | ✅ | 0.000 | 3.03× | 158.05× | OK | Pade(10,10) of e^{-1.5s} numerator coefficients. 10k iters. Octave's padecoef (control pkg) uses a different normalization — comparison reference is MATLAB. |
| `pchip` | ✅ | 0.016 | 15.97× | 29.07× | OK | Sig: yq = pchip(x, v, xq). 50 → 1000 PCHIP. 100 iters. |
| `ppval` | ✅ |  |  |  | N/A | Sig: V = ppval(PP, X). 50-knot spline → 10k pts. 100 iters. |
| `scatteredinterpolant` | ❌ |  |  |  |  |  |
| `spline` | ✅ | 0.016 | 23.98× | 40.02× | MISMATCH | Sig: yq = spline(x, v, xq). 50 → 1000 cubic spline. 100 iters. |
| `unmkpp` | ✅ |  |  |  |  |  |

## Optimization

**Namespace:** core — 5 ✅ + 0 ⚠️ / 7 = 71%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fminbnd` | ✅ | 0.002 | 52.63× | 201.91× | OK | Sig: X = fminbnd(F,A,B). 1-D quadratic min at x=3. 1000 iters. |
| `fminsearch` | ✅ | 0.041 | 5.39× | 64.92× | MISMATCH | Sig: X = fminsearch(F, X0). 2-D quadratic at (1,2). 1000 iters. |
| `fzero` | ✅ | 0.005 | 14.35× | 145.72× | OK | Sig: X = fzero(F, [A B]). Cubic root in [0,5]. 1000 iters. |
| `lsqnonneg` | ❌ |  |  |  |  |  |
| `optimget` | ✅ | 0.000 | 209.63× | 107.63× | OK | Sig: V = optimget(O, NAME). 10000 iters. |
| `optimize` | ❌ |  |  |  |  |  |
| `optimset` | ✅ | 0.001 | 116.44× | 68.66× | MISMATCH | Sig: O = optimset('NAME', VAL, ...). 10000 iters. |

## Ordinary Differential Equations

**Namespace:** `ode.*` (future) — 0 ✅ + 0 ⚠️ / 21 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `decic` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `deval` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode` | ❌ |  |  |  |  |  |
| `ode113` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode15i` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode15s` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23s` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23t` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode23tb` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode45` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode78` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `ode89` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `odeevent` | ❌ |  |  |  |  |  |
| `odeget` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `odejacobian` | ❌ |  |  |  |  |  |
| `odemassmatrix` | ❌ |  |  |  |  |  |
| `odesensitivity` | ❌ |  |  |  |  |  |
| `odeset` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `odextend` | ❌ |  |  |  |  | **deferred — libs/ode** |
| `solveode` | ❌ |  |  |  |  |  |

## Sparse Matrices

**Namespace:** `sparse.*` (future) — 4 ✅ + 0 ⚠️ / 53 = 7%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `amd` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `bicg` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `bicgstab` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `bicgstabl` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `cgs` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `colamd` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `colperm` | ❌ |  |  |  |  |  |
| `condest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `dissect` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `dmperm` | ❌ |  |  |  |  |  |
| `eigs` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `equilibrate` | ❌ |  |  |  |  |  |
| `etree` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `etreeplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `find` | ✅ | 2.383 | 0.23× | 0.06× | OK | Sig: K = find(X). 1M-pt logical, ~77k matches. 100 iters. |
| `full` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `gmres` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `gplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `ichol` | ❌ |  |  |  |  |  |
| `ilu` | ❌ |  |  |  |  |  |
| `issparse` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `lsqr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `minres` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `nnz` | ✅ | 0.142 | 0.23× | 1.39× | OK | Sig: N = nnz(X). 1M-pt count. 1000 iters. |
| `nonzeros` | ✅ | 1.245 | 0.48× | 0.80× | OK | Sig: V = nonzeros(X). 1M-pt extract non-zero (logical→double cast for .*). 100 iters. |
| `normest` | ❌ |  |  |  |  | **deferred — libs/linalg** |
| `nzmax` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `pcg` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `qmr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `randperm` | ✅ |  |  |  |  |  |
| `spalloc` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sparse` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `spaugment` | ❌ |  |  |  |  |  |
| `spconvert` | ❌ |  |  |  |  |  |
| `spdiags` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `speye` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `spfun` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `spones` | ❌ |  |  |  |  |  |
| `spparms` | ❌ |  |  |  |  |  |
| `sprand` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sprandn` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sprandsym` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `sprank` | ❌ |  |  |  |  |  |
| `spy` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `svds` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symamd` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symbfact` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symmlq` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `symrcm` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `tfqmr` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `treelayout` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `treeplot` | ❌ |  |  |  |  | **deferred — libs/sparse** |
| `unmesh` | ❌ |  |  |  |  | **deferred — libs/sparse** |

## Fourier Analysis and Filtering

**Namespace:** `signal.transforms.*` + 6 promotions in core (`fft, ifft, fftshift, ifftshift, conv, xcorr`) — 8 ✅ + 0 ⚠️ / 21 = 38%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `conv` | ✅ | 0.025 | 0.78× | 1.24× | OK | Sig: C = conv(A, B). Deterministic 1k * 100 conv. 100 iters. Element-wise SAVE. |
| `conv2` | ✅ | 0.318 | 0.25× | 0.34× | OK | 128x128 image, 7x7 averaging kernel, 'same' shape. 100 iters. |
| `convn` | ✅ | 0.028 | 2.06× | 0.85× | OK | 64x64 2-D image / convn dispatch (delegates to conv2). 100 iters. |
| `deconv` | ✅ | 0.001 |  | 67.45× | OK | Sig: [Q,R] = deconv(U, V). Polynomial division. 10k iters. |
| `fft` | ✅ | 0.004 | 2.22× | 4.49× | OK | Sig: Y = fft(X). 1024-pt FFT on sin. 1000 iters. Custom fp (complex out). |
| `fft2` | ✅ | 1.127 | 0.60× | 0.58× | OK | 256x256 deterministic test signal, complex 2-D FFT. 50 iters. |
| `fftn` | ❌ |  |  |  |  | N-D FFT |
| `fftshift` | ✅ | 0.003 | 14.24× | 8.90× | OK | Sig: Y = fftshift(X). 1024-pt shift. 1000 iters. |
| `fftw` | ❌ |  |  |  |  | wisdom file |
| `filter` | ✅ | 1.154 | 0.05× | 0.11× | OK | Sig: Y = filter(B, A, X). FIR-1 [1 -0.5] on 100k. 100 iters. |
| `filter2` | ✅ | 0.141 | 0.51× | 0.34× | OK | 128x128 image with 3x3 Laplacian kernel. 100 iters. |
| `ifft` | ✅ | 0.010 | 0.67× | 4.15× | OK | Sig: y = ifft(Y). 1024-pt inverse. 1000 iters. |
| `ifft2` | ✅ | 1.840 | 0.38× | 0.57× | OK | 256x256 inverse 2-D FFT (after fft2 of deterministic signal). 50 iters. |
| `ifftn` | ❌ |  |  |  |  | N-D FFT |
| `ifftshift` | ✅ | 0.003 | 4.08× | 8.09× | OK | Sig: Y = ifftshift(X). 1024-pt unshift. 1000 iters. |
| `interpft` | ✅ | 0.012 | 2.33× | 16.15× | OK | 256-pt band-limited signal interpolated to 1024 points. 200 iters, element-wise. |
| `nextpow2` | ✅ |  |  |  | N/A | Sig: Y = nextpow2(X). 1M-pt integer-ish on [1, 1e6]. 20 iters. Element-wise SAVE. |
| `nufft` | ❌ |  |  |  |  | non-uniform |
| `nufftn` | ❌ |  |  |  |  | non-uniform |
| `padecoef` | ✅ | 0.000 | 3.03× | 158.05× | OK | Pade(10,10) of e^{-1.5s} numerator coefficients. 10k iters. Octave's padecoef (control pkg) uses a different normalization — comparison reference is MATLAB. |
| `ss2tf` | ✅ |  |  |  |  | inverse |

## Descriptive Statistics

**Namespace:** `stats.descriptive.*` / `stats.moving.*` / `stats.nan.*`. Exception: `xcorr/xcov/rms/rssq/peak2peak/peak2rms` → `signal.*` (signal-side stats) — 14 ✅ + 0 ⚠️ / 33 = 42%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bounds` | ✅ | 6.271 | 0.02× | 0.25× | OK | Sig: [lo,hi] = bounds(X). 1M-pt min/max. 100 iters. |
| `corrcoef` | ✅ | 0.070 | 2.30× | 5.01× | OK | Sig: R = corrcoef(M). 2-col 10k matrix. 100 iters. |
| `cov` | ✅ |  |  |  |  |  |
| `cummax` | ✅ | 2.385 | 1.08× | 1.17× | OK | Sig: M = cummax(X). 1M-pt cumulative max. 100 iters. Element-wise SAVE. |
| `cummin` | ✅ | 2.504 | 1.05× | 1.04× | OK | Sig: M = cummin(X). 1M-pt cumulative min. 100 iters. Element-wise SAVE. |
| `iqr` | ✅ | 69.423 | 0.11× | 0.35× | MISMATCH | Sig: R = iqr(X). 1M-pt inter-quartile. 50 iters. |
| `kde` | ❌ |  |  |  |  |  |
| `mape` | ✅ | 9.431 | 0.28× | 0.98× | OK | 1M-point MAPE. 50 iters. numkit needs `import compat.*`; MATLAB+Octave have it flat. |
| `max` | ✅ | 1.462 | 0.04× | 0.54× | OK | Sig: M = max(X). 1M-pt. 100 iters. Scalar fp. |
| `maxk` | ✅ | 71.463 | 0.01× |  | OK | Sig: B = maxk(X, K). Top 10 of 1M. 100 iters. |
| `mean` | ✅ | 1.357 | 0.06× | 0.74× | OK | Sig: M = mean(X). 1M-pt sin reduction. 100 iters. Scalar fp. |
| `median` | ✅ | 3.330 | 1.47× | 2.30× | OK | Sig: M = median(X). 1M-pt full sort + middle. 50 iters. Scalar fp. |
| `min` | ✅ | 1.435 | 0.03× | 0.55× | OK | Sig: M = min(X). 1M-pt. 100 iters. Scalar fp. |
| `mink` | ✅ | 71.151 | 0.01× |  | OK | Sig: B = mink(X, K). Bot 10 of 1M. 100 iters. |
| `mode` | ✅ | 18.749 | 0.48× | 2.75× | OK | Sig: M = mode(X). 1M-pt with ~7919 distinct vals. 50 iters. Scalar fp. |
| `movmad` | ✅ |  |  |  |  | moving mad |
| `movmax` | ✅ | 4.771 | 0.29× | 19.05× | OK | Sig: M = movmax(X, K). 1M-pt window=5. 100 iters. Element-wise SAVE. |
| `movmean` | ✅ | 4.885 | 0.29× | 19.38× | OK | Sig: M = movmean(X, K). 1M-pt window=5. 100 iters. Element-wise SAVE. |
| `movmedian` | ✅ |  |  |  |  | moving median |
| `movmin` | ✅ | 4.650 | 0.29× | 18.33× | OK | Sig: M = movmin(X, K). 1M-pt window=5. 100 iters. Element-wise SAVE. |
| `movprod` | ✅ |  |  |  |  | moving prod |
| `movstd` | ✅ | 7.371 | 0.20× | 17.26× | OK | Sig: M = movstd(X, K). 1M-pt window=5. 100 iters. Element-wise SAVE. |
| `movsum` | ✅ | 4.686 | 0.35× | 19.19× | OK | Sig: Y = movsum(X, K). 1M-pt moving window K=5. 20 iters. Element-wise SAVE. |
| `movvar` | ✅ | 6.842 | 0.22× | 19.23× | OK | Sig: M = movvar(X, K). 1M-pt window=5. 100 iters. Element-wise SAVE. |
| `prctile` | ✅ | 33.491 | 0.23× | 0.72× | MISMATCH | Sig: Y = prctile(X, P). 1M-pt at 4 percentiles. 50 iters. |
| `quantile` | ✅ | 33.734 | 0.23× | 0.72× | MISMATCH | Sig: Y = quantile(X, Q). 1M-pt at 4 quantiles. 50 iters. |
| `rms` | ✅ | 2.673 | 0.50× | 0.17× | OK | Sig: R = rms(X). 1M-pt sin RMS. 100 iters. Scalar fp. |
| `rmse` | ✅ |  |  |  |  |  |
| `std` | ✅ | 0.323 | 4.70× | 26.33× | OK | Sig: S = std(X). 1M-pt. 100 iters. Scalar fp. |
| `summary` | ❌ |  |  |  |  |  |
| `var` | ✅ | 0.326 | 4.46× | 26.48× | OK | Sig: V = var(X). 1M-pt. 100 iters. Scalar fp. |
| `xcorr` | ✅ |  |  |  |  | cross-correlation |
| `xcov` | ✅ | 0.984 | 0.41× | — | OK | Cross-cov of 5k-pt sine. 50 iters. |

## Workspace

**Namespace:** core — 8 ✅ + 0 ⚠️ / 10 = 80%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `clear` | ✅ |  |  |  |  |  |
| `clearvars` | ✅ |  |  |  |  |  |
| `disp` | ✅ |  |  |  |  |  |
| `formatteddisplaytext` | ✅ |  |  |  |  |  |
| `load` | ✅ |  |  |  |  |  |
| `openvar` | ❌ |  |  |  |  | IDE |
| `save` | ✅ |  |  |  |  |  |
| `who` | ✅ |  |  |  |  |  |
| `whos` | ✅ |  |  |  |  |  |
| `workspacebrowser` | ❌ |  |  |  |  |  |

## Error Handling (basic)

**Namespace:** core — 4 ✅ + 0 ⚠️ / 6 = 66%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `assert` | ✅ |  |  |  |  |  |
| `error` | ✅ |  |  |  |  |  |
| `lastwarn` | ✅ | 0.000 | 1.20× | 8.46× | OK | Read last warning state. 100k iters, scalar timing. |
| `oncleanup` | ❌ |  |  |  |  |  |
| `try` | ✅ |  |  |  |  | keyword (`try/catch`) |
| `warning` | ✅ |  |  |  |  |  |

## Exception Handling

**Namespace:** core (keyword + class) — 2 ✅ + 0 ⚠️ / 2 = 100%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `mexception` | ✅ |  |  |  |  | MATLAB exception class — registered as `MException` |
| `try` | ✅ |  |  |  |  | keyword (`try/catch`) |

## Line Plots

**Namespace:** `graphics.line.*` — 2 ✅ + 0 ⚠️ / 12 = 16%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `area` | ❌ |  |  |  |  |  |
| `errorbar` | ❌ |  |  |  |  |  |
| `fimplicit` | ❌ |  |  |  |  |  |
| `fplot` | ❌ |  |  |  |  |  |
| `fplot3` | ❌ |  |  |  |  |  |
| `loglog` | ❌ |  |  |  |  |  |
| `plot` | ✅ |  |  |  |  |  |
| `plot3` | ❌ |  |  |  |  | 3-D |
| `semilogx` | ❌ |  |  |  |  |  |
| `semilogy` | ❌ |  |  |  |  |  |
| `stackedplot` | ❌ |  |  |  |  |  |
| `stairs` | ✅ |  |  |  |  |  |

## Polar Plots

**Namespace:** `graphics.polar.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `compassplot` | ❌ |  |  |  |  |  |
| `fpolarplot` | ❌ |  |  |  |  |  |
| `polaraxes` | ❌ |  |  |  |  |  |
| `polarbubblechart` | ❌ |  |  |  |  |  |
| `polarhistogram` | ❌ |  |  |  |  |  |
| `polarplot` | ✅ |  |  |  |  |  |
| `polarregion` | ❌ |  |  |  |  |  |
| `polarscatter` | ❌ |  |  |  |  |  |
| `radiusregion` | ❌ |  |  |  |  |  |
| `rlim` | ✅ |  |  |  |  |  |
| `rtickangle` | ❌ |  |  |  |  |  |
| `rtickformat` | ❌ |  |  |  |  |  |
| `rticklabels` | ❌ |  |  |  |  |  |
| `rticks` | ❌ |  |  |  |  |  |
| `thetalim` | ✅ |  |  |  |  |  |
| `thetaregion` | ❌ |  |  |  |  |  |
| `thetatickformat` | ❌ |  |  |  |  |  |
| `thetaticklabels` | ❌ |  |  |  |  |  |
| `thetaticks` | ❌ |  |  |  |  |  |

## Contour Plots

**Namespace:** `graphics.contour.*` — 2 ✅ + 0 ⚠️ / 7 = 28%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `clabel` | ❌ |  |  |  |  |  |
| `contour` | ✅ |  |  |  |  |  |
| `contour3` | ❌ |  |  |  |  |  |
| `contourc` | ❌ |  |  |  |  |  |
| `contourf` | ✅ |  |  |  |  |  |
| `contourslice` | ❌ |  |  |  |  |  |
| `fcontour` | ❌ |  |  |  |  |  |

## Vector Fields

**Namespace:** `graphics.vector_fields.*` — 0 ✅ + 0 ⚠️ / 6 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `compassplot` | ❌ |  |  |  |  |  |
| `feather` | ❌ |  |  |  |  |  |
| `quiver` | ❌ |  |  |  |  |  |
| `quiver3` | ❌ |  |  |  |  |  |
| `streamline` | ❌ |  |  |  |  |  |
| `streamslice` | ❌ |  |  |  |  |  |

## Surface and Mesh Plots

**Namespace:** `graphics.surface.*` — 3 ✅ + 0 ⚠️ / 21 = 14%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `contour3` | ❌ |  |  |  |  |  |
| `cylinder` | ✅ | 0.125 | 0.48× | 2.75× | OK | 201-point sin-shaped profile, 50 angular samples. 200 iters. |
| `ellipsoid` | ✅ | 0.094 | 1.00× | 4.19× | OK | 101x101 ellipsoid (1,2,3) center, (4,5,6) semi-axes. 200 iters. |
| `fimplicit3` | ❌ |  |  |  |  |  |
| `fmesh` | ❌ |  |  |  |  |  |
| `fsurf` | ❌ |  |  |  |  |  |
| `hidden` | ❌ |  |  |  |  |  |
| `mesh` | ✅ |  |  |  |  |  |
| `meshc` | ❌ |  |  |  |  |  |
| `meshz` | ❌ |  |  |  |  |  |
| `pcolor` | ✅ |  |  |  |  |  |
| `peaks` | ✅ | 0.365 | 1.71× | 5.05× | OK | 200x200 peaks() surface. 50 iters, element-wise. |
| `ribbon` | ❌ |  |  |  |  |  |
| `sphere` | ✅ | 0.090 | 0.55× | 3.55× | OK | Unit sphere on 101x101 grid. 200 iters, element-wise on Z. |
| `surf` | ✅ |  |  |  |  |  |
| `surf2patch` | ❌ |  |  |  |  |  |
| `surface` | ❌ |  |  |  |  |  |
| `surfc` | ❌ |  |  |  |  |  |
| `surfl` | ❌ |  |  |  |  |  |
| `surfnorm` | ❌ |  |  |  |  |  |
| `waterfall` | ❌ |  |  |  |  |  |

## Volume Visualization

**Namespace:** `graphics.volume.*` — 0 ✅ + 0 ⚠️ / 24 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `coneplot` | ❌ |  |  |  |  |  |
| `contourslice` | ❌ |  |  |  |  |  |
| `curl` | ❌ |  |  |  |  |  |
| `divergence` | ❌ |  |  |  |  |  |
| `flow` | ❌ |  |  |  |  |  |
| `interpstreamspeed` | ❌ |  |  |  |  |  |
| `isocaps` | ❌ |  |  |  |  |  |
| `isocolors` | ❌ |  |  |  |  |  |
| `isonormals` | ❌ |  |  |  |  |  |
| `isosurface` | ❌ |  |  |  |  |  |
| `reducepatch` | ❌ |  |  |  |  |  |
| `reducevolume` | ❌ |  |  |  |  |  |
| `shrinkfaces` | ❌ |  |  |  |  |  |
| `slice` | ❌ |  |  |  |  |  |
| `smooth3` | ❌ |  |  |  |  |  |
| `stream2` | ❌ |  |  |  |  |  |
| `stream3` | ❌ |  |  |  |  |  |
| `streamline` | ❌ |  |  |  |  |  |
| `streamparticles` | ❌ |  |  |  |  |  |
| `streamribbon` | ❌ |  |  |  |  |  |
| `streamslice` | ❌ |  |  |  |  |  |
| `streamtube` | ❌ |  |  |  |  |  |
| `subvolume` | ❌ |  |  |  |  |  |
| `volumebounds` | ❌ |  |  |  |  |  |

## Geographic Plots

**Namespace:** `graphics.geographic.*` — 0 ✅ + 0 ⚠️ / 8 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `geoaxes` | ❌ |  |  |  |  |  |
| `geobasemap` | ❌ |  |  |  |  |  |
| `geobubble` | ❌ |  |  |  |  |  |
| `geodensityplot` | ❌ |  |  |  |  |  |
| `geolimits` | ❌ |  |  |  |  |  |
| `geoplot` | ❌ |  |  |  |  |  |
| `geoscatter` | ❌ |  |  |  |  |  |
| `geotickformat` | ❌ |  |  |  |  |  |

## Low-Level File I/O

**Namespace:** `io.file_io.*` — 13 ✅ + 0 ⚠️ / 15 = 86%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fclose` | ✅ |  |  |  |  |  |
| `feof` | ✅ |  |  |  |  |  |
| `ferror` | ✅ |  |  |  |  |  |
| `fgetl` | ✅ |  |  |  |  |  |
| `fgets` | ✅ |  |  |  |  |  |
| `fileread` | ✅ |  |  |  |  | whole-file read |
| `fopen` | ✅ |  |  |  |  |  |
| `fprintf` | ✅ |  |  |  |  |  |
| `fread` | ✅ |  |  |  |  |  |
| `frewind` | ✅ |  |  |  |  |  |
| `fscanf` | ✅ |  |  |  |  |  |
| `fseek` | ✅ |  |  |  |  |  |
| `ftell` | ✅ |  |  |  |  |  |
| `fwrite` | ✅ |  |  |  |  |  |
| `openedfiles` | ❌ |  |  |  |  |  |

## Text Files (CSV / dlm / readtable)

**Namespace:** `io.text.*`. Exception: `readtable/writetable/readtimetable/writetimetable` → `table.*` (future) — 1 ✅ + 0 ⚠️ / 16 = 6%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `fileread` | ✅ |  |  |  |  | whole-file read |
| `importdatatask` | ❌ |  |  |  |  |  |
| `importtool` | ❌ |  |  |  |  |  |
| `readcell` | ❌ |  |  |  |  |  |
| `readlines` | ✅ |  |  |  |  |  |
| `readmatrix` | ✅ |  |  |  |  | modern CSV |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `readtimetable` | ❌ |  |  |  |  |  |
| `readvars` | ❌ |  |  |  |  |  |
| `textscan` | ✅ |  |  |  |  |  |
| `type` | ✅ |  |  |  |  |  |
| `writecell` | ❌ |  |  |  |  |  |
| `writelines` | ✅ |  |  |  |  |  |
| `writematrix` | ✅ |  |  |  |  | modern CSV |
| `writetable` | ❌ |  |  |  |  | needs table type |
| `writetimetable` | ❌ |  |  |  |  |  |

## Spreadsheets

**Namespace:** `io.text.*`. Table-shaped readers (`readtable`/`writetable`) → `table.*` (future) — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `importdata` | ❌ |  |  |  |  | auto-detect |
| `importdatatask` | ❌ |  |  |  |  |  |
| `importtool` | ❌ |  |  |  |  |  |
| `readcell` | ❌ |  |  |  |  |  |
| `readmatrix` | ✅ |  |  |  |  | modern CSV |
| `readtable` | ❌ |  |  |  |  | needs table type |
| `readtimetable` | ❌ |  |  |  |  |  |
| `readvars` | ❌ |  |  |  |  |  |
| `sheetnames` | ❌ |  |  |  |  |  |
| `writecell` | ❌ |  |  |  |  |  |
| `writematrix` | ✅ |  |  |  |  | modern CSV |
| `writetable` | ❌ |  |  |  |  | needs table type |
| `writetimetable` | ❌ |  |  |  |  |  |

## Workspace Save / Load

**Namespace:** `io.workspace.*` — 0 ✅ + 0 ⚠️ / 2 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `loadobj` | ❌ |  |  |  |  |  |
| `saveobj` | ❌ |  |  |  |  |  |

## File Name Construction

**Namespace:** `io.paths.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `filemarker` | ❌ |  |  |  |  |  |
| `fileparts` | ✅ |  |  |  |  | split path |
| `filesep` | ✅ |  |  |  |  | path sep |
| `fullfile` | ✅ |  |  |  |  | OS path join |
| `matlabdrive` | ❌ |  |  |  |  |  |
| `matlabroot` | ❌ |  |  |  |  |  |
| `tempdir` | ✅ |  |  |  |  |  |
| `tempname` | ✅ |  |  |  |  |  |
| `toolboxdir` | ❌ |  |  |  |  |  |

## Waveform Generation

**Namespace:** `signal.waveform_generation.*` — 5 ✅ + 0 ⚠️ / 21 = 23%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `buffer` | ❌ |  |  |  |  | reshape with overlap |
| `chirp` | ✅ |  |  |  |  |  |
| `demod` | ❌ |  |  |  |  |  |
| `diric` | ✅ |  |  |  |  | Dirichlet |
| `framelbl` | ❌ |  |  |  |  |  |
| `framesig` | ❌ |  |  |  |  |  |
| `gauspuls` | ✅ |  |  |  |  | Gaussian pulse |
| `gmonopuls` | ✅ |  |  |  |  | Gaussian monopulse |
| `marcumq` | ❌ |  |  |  |  |  |
| `modulate` | ❌ |  |  |  |  |  |
| `pulstran` | ✅ |  |  |  |  | pulse train |
| `rectpuls` | ✅ |  |  |  |  | rectangular pulse |
| `sawtooth` | ✅ |  |  |  |  |  |
| `shiftdata` | ❌ |  |  |  |  |  |
| `sinc` | ✅ |  |  |  |  | sin(πx)/(πx) |
| `square` | ✅ |  |  |  |  |  |
| `tripuls` | ✅ |  |  |  |  | triangular |
| `udecode` | ❌ |  |  |  |  |  |
| `uencode` | ❌ |  |  |  |  |  |
| `unshiftdata` | ❌ |  |  |  |  |  |
| `vco` | ❌ |  |  |  |  | VCO |

## Filter Design (FIR / IIR coefficient generators)

**Namespace:** `signal.filter_design.*` — 6 ✅ + 0 ⚠️ / 37 = 16%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `butter` | ✅ |  |  |  |  | IIR Butterworth |
| `buttord` | ❌ |  |  |  |  | order estimator |
| `cfirpm` | ❌ |  |  |  |  | complex Parks-McClellan |
| `cheb1ord` | ❌ |  |  |  |  | order estimator |
| `cheb2ord` | ❌ |  |  |  |  | order estimator |
| `cheby1` | ❌ |  |  |  |  | IIR Chebyshev I |
| `cheby2` | ❌ |  |  |  |  | IIR Chebyshev II |
| `designfilt` | ❌ |  |  |  |  |  |
| `designfilter` | ❌ |  |  |  |  |  |
| `digitalfilter` | ❌ |  |  |  |  |  |
| `double` | ✅ | 3.606 | 0.04× | 0.57× | OK | Sig: Y = double(X). 1M single → double. 50 iters. Element-wise SAVE. |
| `dspfwiz` | ❌ |  |  |  |  |  |
| `ellip` | ❌ |  |  |  |  | IIR elliptic |
| `ellipord` | ❌ |  |  |  |  | order estimator |
| `filt2block` | ❌ |  |  |  |  |  |
| `filteranalyzer` | ❌ |  |  |  |  |  |
| `fir1` | ✅ |  |  |  |  | FIR window-design |
| `fir2` | ❌ |  |  |  |  | arbitrary-response FIR |
| `fircls` | ❌ |  |  |  |  | constrained-LS FIR |
| `fircls1` | ❌ |  |  |  |  |  |
| `firls` | ❌ |  |  |  |  | least-squares FIR |
| `firpm` | ❌ |  |  |  |  | Parks-McClellan FIR |
| `firpmord` | ❌ |  |  |  |  | order estimator |
| `gaussdesign` | ❌ |  |  |  |  |  |
| `info` | ❌ |  |  |  |  |  |
| `intfilt` | ✅ |  |  |  |  | interpolating FIR |
| `isdouble` | ❌ |  |  |  |  |  |
| `issingle` | ✅ |  |  |  |  |  |
| `kaiserord` | ❌ |  |  |  |  | Kaiser window order |
| `maxflat` | ❌ |  |  |  |  |  |
| `polyscale` | ❌ |  |  |  |  |  |
| `polystab` | ❌ |  |  |  |  |  |
| `rcosdesign` | ❌ |  |  |  |  |  |
| `scalefiltersections` | ❌ |  |  |  |  |  |
| `sgolay` | ✅ |  |  |  |  | Savitzky-Golay |
| `single` | ✅ | 2.755 | 0.06× | 0.43× | OK | Sig: Y = single(X). 1M double → single. 50 iters. Element-wise SAVE. |
| `yulewalk` | ❌ |  |  |  |  | recursive YW |

## Analog Filters (prototype + analog response)

**Namespace:** `signal.filter_design.*` — 1 ✅ + 0 ⚠️ / 17 = 5%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `besselap` | ❌ |  |  |  |  | analog prototype |
| `besself` | ❌ |  |  |  |  | IIR Bessel |
| `bilinear` | ❌ |  |  |  |  |  |
| `buttap` | ❌ |  |  |  |  | analog prototype |
| `butter` | ✅ |  |  |  |  | IIR Butterworth |
| `cheb1ap` | ❌ |  |  |  |  | analog prototype |
| `cheb2ap` | ❌ |  |  |  |  | analog prototype |
| `cheby1` | ❌ |  |  |  |  | IIR Chebyshev I |
| `cheby2` | ❌ |  |  |  |  | IIR Chebyshev II |
| `ellip` | ❌ |  |  |  |  | IIR elliptic |
| `ellipap` | ❌ |  |  |  |  | analog prototype |
| `freqs` | ❌ |  |  |  |  | analog freq response |
| `impinvar` | ❌ |  |  |  |  |  |
| `lp2bp` | ❌ |  |  |  |  |  |
| `lp2bs` | ❌ |  |  |  |  |  |
| `lp2hp` | ❌ |  |  |  |  |  |
| `lp2lp` | ❌ |  |  |  |  |  |

## Digital Filter Analysis (freqz / phasez / grpdelay / impz / ...)

**Namespace:** `signal.filter_analysis.*` — 3 ✅ + 0 ⚠️ / 19 = 15%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `filteranalyzer` | ❌ |  |  |  |  |  |
| `filternorm` | ❌ |  |  |  |  |  |
| `filtord` | ❌ |  |  |  |  |  |
| `firtype` | ❌ |  |  |  |  |  |
| `freqz` | ✅ |  |  |  |  | discrete freq response |
| `grpdelay` | ✅ |  |  |  |  | group delay |
| `impz` | ✅ |  |  |  |  | impulse response |
| `impzlength` | ✅ |  |  |  |  | impulse length |
| `isallpass` | ✅ |  |  |  |  | predicate |
| `isfir` | ✅ |  |  |  |  | predicate |
| `islinphase` | ✅ |  |  |  |  | predicate |
| `ismaxphase` | ✅ |  |  |  |  | predicate |
| `isminphase` | ✅ |  |  |  |  | predicate |
| `isstable` | ✅ |  |  |  |  | predicate |
| `phasedelay` | ✅ |  |  |  |  | phase delay |
| `phasez` | ✅ |  |  |  |  | phase response |
| `stepz` | ✅ |  |  |  |  | step response |
| `zerophase` | ✅ |  |  |  |  |  |
| `zplane` | ❌ |  |  |  |  |  |

## Digital Filtering (filter / filtfilt / sosfilt / lowpass / ...)

**Namespace:** `signal.digital_filtering.*` + `signal.filter_implementation.*` (TF/SOS/SS/ZP conversions) — 8 ✅ + 0 ⚠️ / 41 = 19%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bandpass` | ✅ |  |  |  |  | spec-driven BP |
| `bandstop` | ✅ |  |  |  |  | spec-driven BS |
| `cell2sos` | ❌ |  |  |  |  |  |
| `convmtx` | ✅ |  |  |  |  | convolution matrix |
| `ctf2zp` | ❌ |  |  |  |  | control TF → ZPK |
| `ctffilt` | ❌ |  |  |  |  | control TF filter |
| `dspfwiz` | ❌ |  |  |  |  |  |
| `eqtflength` | ❌ |  |  |  |  |  |
| `fftfilt` | ✅ |  |  |  |  | FFT-based overlap-add |
| `filt2block` | ❌ |  |  |  |  |  |
| `filtfilt` | ✅ |  |  |  |  | zero-phase forward+back |
| `filtic` | ❌ |  |  |  |  | init state |
| `hampel` | ✅ |  |  |  |  | outlier-resilient |
| `highpass` | ✅ |  |  |  |  | spec-driven HP |
| `latc2tf` | ❌ |  |  |  |  | inverse |
| `latcfilt` | ❌ |  |  |  |  |  |
| `lowpass` | ✅ |  |  |  |  | spec-driven LP |
| `medfilt1` | ✅ |  |  |  |  | median |
| `residuez` | ❌ |  |  |  |  |  |
| `scalefiltersections` | ❌ |  |  |  |  |  |
| `sgolayfilt` | ✅ |  |  |  |  | Savitzky-Golay |
| `sos2cell` | ❌ |  |  |  |  |  |
| `sos2ctf` | ❌ |  |  |  |  |  |
| `sos2ss` | ✅ |  |  |  |  | SOS → SS |
| `sos2tf` | ✅ |  |  |  |  | inverse |
| `sos2zp` | ✅ |  |  |  |  | SOS → ZPK |
| `sosfilt` | ✅ |  |  |  |  | SOS-cascade filter |
| `ss` | ❌ |  |  |  |  |  |
| `ss2sos` | ✅ |  |  |  |  | inverse |
| `ss2zp` | ✅ |  |  |  |  | SS → ZPK |
| `tf` | ❌ |  |  |  |  |  |
| `tf2latc` | ❌ |  |  |  |  | lattice |
| `tf2sos` | ✅ |  |  |  |  | TF → SOS |
| `tf2ss` | ✅ |  |  |  |  | TF → SS |
| `tf2zp` | ✅ |  |  |  |  | TF → ZPK |
| `tf2zpk` | ✅ |  |  |  |  |  |
| `zp2ctf` | ❌ |  |  |  |  |  |
| `zp2sos` | ✅ |  |  |  |  | ZPK → SOS |
| `zp2ss` | ✅ |  |  |  |  | inverse |
| `zp2tf` | ✅ |  |  |  |  | inverse |
| `zpk` | ❌ |  |  |  |  |  |

## Multirate Signal Processing (decimate / interp / resample / ...)

**Namespace:** `signal.multirate.*` — 4 ✅ + 0 ⚠️ / 8 = 50%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `decimate` | ✅ |  |  |  |  |  |
| `downsample` | ✅ |  |  |  |  |  |
| `fillgaps` | ❌ |  |  |  |  |  |
| `interp` | ✅ |  |  |  |  |  |
| `intfilt` | ✅ |  |  |  |  | interpolating FIR |
| `resample` | ✅ |  |  |  |  |  |
| `upfirdn` | ✅ |  |  |  |  |  |
| `upsample` | ✅ |  |  |  |  |  |

## Signal Modeling (AR / Burg / Yule-Walker / Levinson / Prony)

**Namespace:** `signal.parametric.*` — 0 ✅ + 0 ⚠️ / 25 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `ac2poly` | ❌ |  |  |  |  |  |
| `ac2rc` | ❌ |  |  |  |  |  |
| `arburg` | ❌ |  |  |  |  | Burg AR |
| `arcov` | ❌ |  |  |  |  | covariance AR |
| `armcov` | ❌ |  |  |  |  | modified cov AR |
| `aryule` | ❌ |  |  |  |  | Yule-Walker AR |
| `corrmtx` | ❌ |  |  |  |  | autocorr matrix |
| `invfreqs` | ❌ |  |  |  |  |  |
| `invfreqz` | ❌ |  |  |  |  | IIR sys-id |
| `is2rc` | ❌ |  |  |  |  |  |
| `lar2rc` | ❌ |  |  |  |  |  |
| `levinson` | ❌ |  |  |  |  | Levinson-Durbin |
| `lpc` | ❌ |  |  |  |  | linear prediction |
| `lsf2poly` | ❌ |  |  |  |  |  |
| `poly2ac` | ❌ |  |  |  |  |  |
| `poly2lsf` | ❌ |  |  |  |  |  |
| `poly2rc` | ❌ |  |  |  |  |  |
| `prony` | ❌ |  |  |  |  | Prony method |
| `rc2ac` | ❌ |  |  |  |  |  |
| `rc2is` | ❌ |  |  |  |  |  |
| `rc2lar` | ❌ |  |  |  |  |  |
| `rc2poly` | ❌ |  |  |  |  |  |
| `rlevinson` | ❌ |  |  |  |  | reverse Levinson |
| `schurrc` | ❌ |  |  |  |  | Schur recursion |
| `stmcb` | ❌ |  |  |  |  | Steiglitz-McBride |

## Correlation and Convolution (extras: alignsignals / finddelay / xcorr2 / cconv / convmtx)

**Namespace:** `signal.convolution.*` — 0 ✅ + 0 ⚠️ / 9 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `alignsignals` | ✅ |  |  |  |  | align via xcorr |
| `cconv` | ✅ |  |  |  |  | circular convolution |
| `convmtx` | ✅ |  |  |  |  | convolution matrix |
| `corrmtx` | ❌ |  |  |  |  | autocorr matrix |
| `dtw` | ❌ |  |  |  |  | dynamic time warp |
| `edr` | ❌ |  |  |  |  | edit distance on real |
| `finddelay` | ✅ |  |  |  |  | estimate delay |
| `findsignal` | ❌ |  |  |  |  | pattern search |
| `xcorr2` | ✅ |  |  |  |  | 2-D xcorr |

## Transforms (FFT / DCT / DWT / Hilbert / CZT / Cepstrum)

**Namespace:** `signal.transforms.*`. Promotions in core: `fft, ifft, fftshift, ifftshift`. Future wavelet split: `cwt/dwt/modwt/...` → `wavelet.*` — 6 ✅ + 0 ⚠️ / 32 = 18%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bitrevorder` | ✅ |  |  |  |  | bit-reverse permutation |
| `cceps` | ✅ |  |  |  |  | complex cepstrum |
| `czt` | ❌ |  |  |  |  | chirp Z-transform |
| `dct` | ✅ |  |  |  |  |  |
| `dftmtx` | ✅ |  |  |  |  | DFT matrix |
| `digitrevorder` | ❌ |  |  |  |  |  |
| `dlistft` | ❌ |  |  |  |  |  |
| `dlstft` | ❌ |  |  |  |  |  |
| `emd` | ❌ |  |  |  |  | empirical mode decomp |
| `envelope` | ✅ |  |  |  |  |  |
| `fsst` | ❌ |  |  |  |  | Fourier synchrosqueezed |
| `fwht` | ❌ |  |  |  |  | fast Walsh-Hadamard |
| `goertzel` | ✅ |  |  |  |  |  |
| `hht` | ❌ |  |  |  |  | Hilbert-Huang |
| `hilbert` | ✅ |  |  |  |  |  |
| `icceps` | ✅ |  |  |  |  | inverse complex cepstrum |
| `idct` | ✅ |  |  |  |  |  |
| `ifsst` | ❌ |  |  |  |  |  |
| `ifwht` | ❌ |  |  |  |  | inverse |
| `instfreq` | ❌ |  |  |  |  | instantaneous frequency |
| `istft` | ❌ |  |  |  |  | inverse |
| `istftlayer` | ❌ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `rceps` | ✅ |  |  |  |  | real cepstrum |
| `spectrogram` | ✅ |  |  |  |  |  |
| `stft` | ❌ |  |  |  |  | short-time FFT |
| `stftlayer` | ❌ |  |  |  |  |  |
| `stftmag2sig` | ❌ |  |  |  |  |  |
| `vmd` | ❌ |  |  |  |  | variational MD |
| `wvd` | ❌ |  |  |  |  | Wigner-Ville |
| `xspectrogram` | ❌ |  |  |  |  | cross-spectrogram |
| `xwvd` | ❌ |  |  |  |  | cross WVD |

## Windows (Hamming / Hann / Kaiser / Chebyshev / DPSS / ...)

**Namespace:** `signal.windows.*` — 6 ✅ + 0 ⚠️ / 24 = 25%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `barthannwin` | ✅ |  |  |  |  | Bartlett-Hann |
| `bartlett` | ✅ |  |  |  |  |  |
| `blackman` | ✅ |  |  |  |  |  |
| `blackmanharris` | ✅ |  |  |  |  |  |
| `bohmanwin` | ✅ |  |  |  |  | Bohman |
| `chebwin` | ✅ |  |  |  |  | Dolph-Chebyshev |
| `dpss` | ❌ |  |  |  |  | discrete prolate spheroidal |
| `dpssclear` | ❌ |  |  |  |  | cache |
| `dpssdir` | ❌ |  |  |  |  | cache |
| `dpssload` | ❌ |  |  |  |  | cache |
| `dpsssave` | ❌ |  |  |  |  | cache |
| `enbw` | ❌ |  |  |  |  | equivalent noise BW |
| `flattopwin` | ✅ |  |  |  |  |  |
| `gausswin` | ✅ |  |  |  |  | Gaussian |
| `hamming` | ✅ |  |  |  |  |  |
| `hann` | ✅ |  |  |  |  |  |
| `kaiser` | ✅ |  |  |  |  |  |
| `nuttallwin` | ✅ |  |  |  |  |  |
| `parzenwin` | ✅ |  |  |  |  | Parzen |
| `rectwin` | ✅ |  |  |  |  |  |
| `taylorwin` | ✅ |  |  |  |  | Taylor |
| `triang` | ✅ |  |  |  |  | triangular |
| `tukeywin` | ✅ |  |  |  |  | tapered cosine |
| `wvtool` | ❌ |  |  |  |  | GUI |

## Parametric Spectral Estimation (pburg / pmtm / pmusic / ...)

**Namespace:** `signal.spectral_analysis.*`. Magnitude utils (`db/db2mag/mag2db/pow2db`) → core (cross-cutting math) — 1 ✅ + 0 ⚠️ / 10 = 10%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `db` | ✅ |  |  |  |  | magnitude → dB |
| `db2mag` | ✅ |  |  |  |  |  |
| `db2pow` | ✅ |  |  |  |  |  |
| `findpeaks` | ✅ |  |  |  |  |  |
| `mag2db` | ✅ |  |  |  |  |  |
| `pburg` | ❌ |  |  |  |  | Burg AR |
| `pcov` | ❌ |  |  |  |  |  |
| `pmcov` | ❌ |  |  |  |  |  |
| `pow2db` | ✅ |  |  |  |  |  |
| `pyulear` | ❌ |  |  |  |  | Yule-Walker AR |

## Nonparametric Spectral Estimation (pwelch / periodogram / cpsd / ...)

**Namespace:** `signal.spectral_analysis.*` — 3 ✅ + 0 ⚠️ / 17 = 17%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `cpsd` | ❌ |  |  |  |  | cross-PSD |
| `db` | ✅ |  |  |  |  | magnitude → dB |
| `db2mag` | ✅ |  |  |  |  |  |
| `db2pow` | ✅ |  |  |  |  |  |
| `findpeaks` | ✅ |  |  |  |  |  |
| `mag2db` | ✅ |  |  |  |  |  |
| `mscohere` | ❌ |  |  |  |  | magnitude-squared coherence |
| `periodogram` | ✅ |  |  |  |  |  |
| `plomb` | ❌ |  |  |  |  | Lomb-Scargle |
| `pmtm` | ❌ |  |  |  |  | multi-taper |
| `poctave` | ❌ |  |  |  |  |  |
| `pow2db` | ✅ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `pwelch` | ✅ |  |  |  |  | Welch PSD |
| `refinepeaks` | ❌ |  |  |  |  |  |
| `spectralentropy` | ❌ |  |  |  |  |  |
| `tfestimate` | ❌ |  |  |  |  | TF estimate |

## Spectral Measurements (bandpower / snr / sinad / thd / ...)

**Namespace:** `signal.spectral_analysis.*` — 0 ✅ + 0 ⚠️ / 18 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `bandpower` | ❌ |  |  |  |  |  |
| `enbw` | ❌ |  |  |  |  | equivalent noise BW |
| `instbw` | ❌ |  |  |  |  |  |
| `instfreq` | ❌ |  |  |  |  | instantaneous frequency |
| `meanfreq` | ❌ |  |  |  |  | mean frequency |
| `medfreq` | ❌ |  |  |  |  | median frequency |
| `obw` | ❌ |  |  |  |  |  |
| `powerbw` | ❌ |  |  |  |  |  |
| `sfdr` | ❌ |  |  |  |  | spurious-free dynamic range |
| `sinad` | ❌ |  |  |  |  | signal-noise-distortion |
| `snr` | ❌ |  |  |  |  | signal-to-noise |
| `spectralcrest` | ❌ |  |  |  |  |  |
| `spectralentropy` | ❌ |  |  |  |  |  |
| `spectralflatness` | ❌ |  |  |  |  |  |
| `spectralkurtosis` | ❌ |  |  |  |  |  |
| `spectralskewness` | ❌ |  |  |  |  |  |
| `thd` | ❌ |  |  |  |  | total harmonic distortion |
| `toi` | ❌ |  |  |  |  | third-order intercept |

## Time-Frequency Analysis (spectrogram / stft / cwt / wvd / ...)

**Namespace:** `signal.time_frequency.*`. Wavelet/EMD subset (`cwt/wsst/vmd/hht/emd/fsst/ifsst`) → `wavelet.*` (future) — 1 ✅ + 0 ⚠️ / 27 = 3%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dlistft` | ❌ |  |  |  |  |  |
| `dlstft` | ❌ |  |  |  |  |  |
| `emd` | ❌ |  |  |  |  | empirical mode decomp |
| `fsst` | ❌ |  |  |  |  | Fourier synchrosqueezed |
| `hht` | ❌ |  |  |  |  | Hilbert-Huang |
| `ifsst` | ❌ |  |  |  |  |  |
| `instbw` | ❌ |  |  |  |  |  |
| `instfreq` | ❌ |  |  |  |  | instantaneous frequency |
| `iscola` | ❌ |  |  |  |  |  |
| `istft` | ❌ |  |  |  |  | inverse |
| `istftlayer` | ❌ |  |  |  |  |  |
| `kurtogram` | ❌ |  |  |  |  |  |
| `pspectrum` | ❌ |  |  |  |  | easy spectral analysis |
| `spectralcrest` | ❌ |  |  |  |  |  |
| `spectralentropy` | ❌ |  |  |  |  |  |
| `spectralflatness` | ❌ |  |  |  |  |  |
| `spectralkurtosis` | ❌ |  |  |  |  |  |
| `spectralskewness` | ❌ |  |  |  |  |  |
| `spectrogram` | ✅ |  |  |  |  |  |
| `stft` | ❌ |  |  |  |  | short-time FFT |
| `stftlayer` | ❌ |  |  |  |  |  |
| `stftmag2sig` | ❌ |  |  |  |  |  |
| `tfridge` | ❌ |  |  |  |  |  |
| `vmd` | ❌ |  |  |  |  | variational MD |
| `wvd` | ❌ |  |  |  |  | Wigner-Ville |
| `xspectrogram` | ❌ |  |  |  |  | cross-spectrogram |
| `xwvd` | ❌ |  |  |  |  | cross WVD |

## Pulse and Transition Metrics (risetime / dutycycle / overshoot / ...)

**Namespace:** `signal.measurements.*` — 0 ✅ + 0 ⚠️ / 12 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `dutycycle` | ❌ |  |  |  |  | duty cycle |
| `falltime` | ❌ |  |  |  |  |  |
| `midcross` | ❌ |  |  |  |  | mid-ref crossings |
| `overshoot` | ❌ |  |  |  |  |  |
| `pulseperiod` | ❌ |  |  |  |  |  |
| `pulsesep` | ❌ |  |  |  |  |  |
| `pulsewidth` | ❌ |  |  |  |  |  |
| `risetime` | ❌ |  |  |  |  |  |
| `settlingtime` | ❌ |  |  |  |  |  |
| `slewrate` | ❌ |  |  |  |  |  |
| `statelevels` | ❌ |  |  |  |  |  |
| `undershoot` | ❌ |  |  |  |  |  |

## Signal Descriptive Statistics (rms / peak2peak / envelope / sigROIs / ...)

**Namespace:** `signal.measurements.*` — 2 ✅ + 0 ⚠️ / 30 = 6%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `alignsignals` | ✅ |  |  |  |  | align via xcorr |
| `binmask2sigroi` | ❌ |  |  |  |  |  |
| `countlabels` | ❌ |  |  |  |  |  |
| `cusum` | ❌ |  |  |  |  | CUSUM change detection |
| `dtw` | ❌ |  |  |  |  | dynamic time warp |
| `edr` | ❌ |  |  |  |  | edit distance on real |
| `envelope` | ✅ |  |  |  |  |  |
| `extendsigroi` | ❌ |  |  |  |  |  |
| `extractsigroi` | ❌ |  |  |  |  |  |
| `filenames2labels` | ❌ |  |  |  |  |  |
| `findchangepts` | ❌ |  |  |  |  | change-point detection |
| `finddelay` | ✅ |  |  |  |  | estimate delay |
| `findpeaks` | ✅ |  |  |  |  |  |
| `findsignal` | ❌ |  |  |  |  | pattern search |
| `folders2labels` | ❌ |  |  |  |  |  |
| `framelbl` | ❌ |  |  |  |  |  |
| `framesig` | ❌ |  |  |  |  |  |
| `meanfreq` | ❌ |  |  |  |  | mean frequency |
| `medfreq` | ❌ |  |  |  |  | median frequency |
| `mergesigroi` | ❌ |  |  |  |  |  |
| `peak2peak` | ✅ |  |  |  |  | p-p amplitude |
| `peak2rms` | ✅ |  |  |  |  |  |
| `removesigroi` | ❌ |  |  |  |  |  |
| `rssq` | ✅ |  |  |  |  | root-sum-squared |
| `seqperiod` | ❌ |  |  |  |  |  |
| `shortensigroi` | ❌ |  |  |  |  |  |
| `sigrangebinmask` | ❌ |  |  |  |  |  |
| `sigroi2binmask` | ❌ |  |  |  |  |  |
| `splitlabels` | ❌ |  |  |  |  |  |
| `zerocrossrate` | ❌ |  |  |  |  |  |

## Smoothing and Denoising (smoothdata / hampel / sgolayfilt / ...)

**Namespace:** `signal.smoothing.*` + `signal.digital_filtering.*` (medfilt1, sgolayfilt). `smoothdata` itself → `stats.moving.*` — 3 ✅ + 0 ⚠️ / 4 = 75%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `hampel` | ✅ |  |  |  |  | outlier-resilient |
| `medfilt1` | ✅ |  |  |  |  | median |
| `sgolay` | ✅ |  |  |  |  | Savitzky-Golay |
| `sgolayfilt` | ✅ |  |  |  |  | Savitzky-Golay |

## Vibration Analysis (envspectrum / order tracking / modal)

**Namespace:** `signal.vibration.*` — 0 ✅ + 0 ⚠️ / 13 = 0%

| function | status | numkit_ms | vs_MATLAB | vs_Octave | correctness | comment |
|---|:---:|---:|---:|---:|:---:|---|
| `envspectrum` | ❌ |  |  |  |  | envelope spectrum |
| `modalfit` | ❌ |  |  |  |  | modal-fit |
| `modalfrf` | ❌ |  |  |  |  |  |
| `modalsd` | ❌ |  |  |  |  |  |
| `orderspectrum` | ❌ |  |  |  |  |  |
| `ordertrack` | ❌ |  |  |  |  |  |
| `orderwaveform` | ❌ |  |  |  |  |  |
| `rainflow` | ❌ |  |  |  |  |  |
| `rpmfreqmap` | ❌ |  |  |  |  |  |
| `rpmordermap` | ❌ |  |  |  |  |  |
| `rpmtrack` | ❌ |  |  |  |  | order tracking |
| `tachorpm` | ❌ |  |  |  |  | tachometer→RPM |
| `tsa` | ❌ |  |  |  |  |  |
