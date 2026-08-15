% toolboxes/linalg/tests/smoke/matrix_functions_general_smoke.m
%
% Smoke demo for general sqrtm, sylvester, and logm.

disp('--- General Matrix Functions Smoke ---');

A = [1 2; 3 4];
R = sqrtm(A);
disp('sqrtm([1 2; 3 4])^2 residual:');
disp(max(max(abs(R*R - A))));

B = [5 6; 7 8];
C = [1 0; 0 1];
X = sylvester(A, B, C);
disp('sylvester(A, B, C) residual A*X + X*B - C:');
disp(max(max(abs(A*X + X*B - C))));

L = logm([2 1; 0 3]);
E = expm(L);
disp('expm(logm([2 1; 0 3])) residual:');
disp(max(max(abs(E - [2 1; 0 3]))));
