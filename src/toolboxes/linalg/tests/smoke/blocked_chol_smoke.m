% toolboxes/linalg/tests/smoke/blocked_chol_smoke.m
%
% Smoke demo for blocked Cholesky decomposition.

disp('--- blocked Cholesky decomposition Smoke ---');

B = rand(256, 256);
A = B * B' + 256 * eye(256);
R = chol(A);

err = max(max(abs(A - R' * R)));
disp('Reconstruction error ||A - R''*R|| (expected < 1e-12):');
disp(err);
