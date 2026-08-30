clear

% dot(A, B, dim) — the dimension argument (DEEP-PROBE 2026-05-31). numkit
% IGNORED the dim arg and always reduced along dim 1; MATLAB reduces along
% the requested dim: dim 2 across rows (Hx1), dim 1 down columns (1xW). A
% vector with explicit dim follows the general sum(conj(A).*B,dim) rule.
% vs MATLAB R2025b.

A = [1 2 3; 4 5 6];  B = [1 1 1; 2 2 2];

fprintf('=== matrix ===\n');
d2 = dot(A, B, 2);
fprintf('dot(A,B,2) = [%g; %g]  (expect [6; 30], per-row)\n', d2(1), d2(2));
d1 = dot(A, B, 1);
fprintf('dot(A,B,1) = [%g %g %g]  (expect [9 12 15], per-column)\n', d1(1), d1(2), d1(3));
fprintf('dot(A,B)   = [%g %g %g]  (default = per-column)\n', dot(A,B));

fprintf('\n=== vector with explicit dim ===\n');
rv = dot([1 2 3], [4 5 6], 1);
fprintf('dot([1 2 3],[4 5 6],1) = [%g %g %g]  (expect [4 10 18], identity)\n', rv(1), rv(2), rv(3));
fprintf('dot([1 2 3],[4 5 6],2) = %g  (expect 32, full sum)\n', dot([1 2 3],[4 5 6],2));

fprintf('\n=== complex (conjugates first arg) ===\n');
dc = dot([1+1i 2; 3 4], [1 1; 1 1], 2);
fprintf('dot(...,2) = %s  (expect [3-1i;7])\n', mat2str(dc));
