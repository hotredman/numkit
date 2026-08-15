% toolboxes/linalg/tests/smoke/balance_perm_smoke.m
%
% Smoke demo for balance permutation phase.

disp('--- balance permutation phase Smoke ---');

A = [1 2 3; 0 4 5; 0 0 6];
[B_perm, T_perm] = balance(A);
[B_noperm, T_noperm] = balance(A, 'noperm');

disp('balance(A) result with permutation:');
disp(B_perm);

disp('balance(A, ''noperm'') result without permutation:');
disp(B_noperm);
