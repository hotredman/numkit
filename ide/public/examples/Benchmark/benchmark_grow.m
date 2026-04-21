% Benchmark: incremental array grow patterns
%
% Compares the two canonical MATLAB patterns for building a vector
% element-by-element:
%
%   Pattern A:  A = [A, i]     — concatenation (HORZCAT in numkit-m)
%   Pattern B:  A(end+1) = i   — indexed assign with grow-by-one
%   Pattern C:  A(i) = i       — same as B but with an explicit counter
%
% In MATLAB / Octave both paths are JIT-optimised so the cost is similar.
% In numkit-m, pattern B and C route through VM INDEX_SET which uses
% appendScalar with geometric capacity — amortised O(1) per append.
% Pattern A still goes through HORZCAT, which allocates a fresh buffer
% each iteration (O(N²) total). The ratio between A and B here is a
% good proxy for how much a compiler-level `A = [A, x]` rewrite would
% buy us in numkit-m.
clear

fprintf('\n=== Array Grow Benchmark ===\n\n')

N = 10000;

% ── Pattern A: A = [A, i] — horzcat each iteration ─────────
A = [];
tic
for i = 1:N
    A = [A, i];
end
tA = toc;
fprintf(' A. A = [A, i]       N=%d : %.4f s  (len=%d)\n', N, tA, length(A))

% ── Pattern B: A(end+1) = i ────────────────────────────────
B = [];
tic
for i = 1:N
    B(end+1) = i;
end
tB = toc;
fprintf(' B. A(end+1) = i     N=%d : %.4f s  (len=%d)\n', N, tB, length(B))

% ── Pattern C: A(i) = i with explicit index ───────────────
C = [];
tic
for i = 1:N
    C(i) = i;
end
tC = toc;
fprintf(' C. A(i) = i         N=%d : %.4f s  (len=%d)\n', N, tC, length(C))

% ── Pattern D: pre-allocate — the fast reference ──────────
tic
D = zeros(1, N);
for i = 1:N
    D(i) = i;
end
tD = toc;
fprintf(' D. pre-alloc +A(i)  N=%d : %.4f s  (len=%d)\n', N, tD, length(D))

fprintf('\n A/B = %.1fx      A/C = %.1fx      A/D = %.1fx\n', tA/tB, tA/tC, tA/tD)
