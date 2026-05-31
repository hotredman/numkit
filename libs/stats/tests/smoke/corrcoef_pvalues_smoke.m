clear

import compat.*

% corrcoef second output P (two-sided p-values for rho == 0). Bug fixed
% 2026-05-30: corrcoef returned only R. P(i,j)=2*tcdf(-|t|,n-2) with
% t=r*sqrt((n-2)/(1-r^2)); diagonal=1. vs MATLAB R2025b.

format long

fprintf('=== two-variable [R,P] ===\n');
[R, P] = corrcoef([1 2 3 4]', [2 4 5 9]');
fprintf('R = %s\n', mat2str(R, 8));
fprintf('P = %s (expect [1 0.035236179; 0.035236179 1])\n', mat2str(P, 8));

fprintf('\n=== matrix [R,P] ===\n');
[Rm, Pm] = corrcoef([1 2 3; 4 5 7; 2 1 0]);
fprintf('P = %s\n', mat2str(Pm, 6));
fprintf('  (expect [1 0.366717 0.49324; 0.366717 1 0.126523; 0.49324 0.126523 1])\n');

fprintf('\n=== diagonal is 1, P symmetric ===\n');
fprintf('P(1,1)=%g P(2,2)=%g P(2,1)=%g\n', P(1,1), P(2,2), P(2,1));

fprintf('\n=== single output R unchanged ===\n');
fprintf('R = %s\n', mat2str(corrcoef([1 2 3 4]', [2 4 5 9]'), 6));
