clear

% cross(A, B, dim) — the dimension argument (DEEP-PROBE 2026-05-31). numkit
% IGNORED dim and always crossed along the FIRST length-3 dimension; for a
% 3x3 (ambiguous) input it picked dim 1 regardless of the requested dim.
% MATLAB crosses the length-3 vectors running along `dim`. vs MATLAB R2025b.

A = [1 2 3; 4 5 6; 7 8 9];  B = [1 0 0; 0 1 0; 0 0 1];

fprintf('=== 3x3 (ambiguous): dim selects the operating dimension ===\n');
disp('dim 1 (each column a 3-vec) — expect [0 -8 6; 7 0 -3; -4 2 0]:');
disp(cross(A, B, 1));
disp('dim 2 (each row a 3-vec) — expect [0 3 -2; -6 0 4; 8 -7 0]:');
disp(cross(A, B, 2));
disp('default (no dim) — picks dim 1:');
disp(cross(A, B));

fprintf('=== row vector with explicit dim 2 ===\n');
rv = cross([1 2 3], [4 5 6], 2);
fprintf('cross([1 2 3],[4 5 6],2) = [%g %g %g]  (expect [-3 6 -3])\n', rv(1), rv(2), rv(3));
