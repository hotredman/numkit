% toolboxes/linalg/tests/smoke/blocked_lu_smoke.m
%
% Smoke demo for blocked LU decomposition.

disp('--- blocked LU decomposition Smoke ---');

A = rand(128, 128);
[L, U, P] = lu(A);

err = max(max(abs(P * A - L * U)));
disp('Reconstruction error ||P*A - L*U|| (expected < 1e-12):');
disp(err);
