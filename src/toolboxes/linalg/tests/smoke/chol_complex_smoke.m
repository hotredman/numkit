% toolboxes/linalg/tests/smoke/chol_complex_smoke.m
%
% Smoke demo for complex Hermitian Cholesky factorization.

disp('--- Complex Cholesky Smoke ---');
A = [2, 1i; -1i, 2];

R = chol(A);
disp('Upper Cholesky factor R:');
disp(R);

disp('Reconstruction R'' * R - A residual:');
disp(max(max(abs(R'*R - A))));
