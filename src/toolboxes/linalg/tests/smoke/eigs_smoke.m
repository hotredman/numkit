% toolboxes/linalg/tests/smoke/eigs_smoke.m
%
% Smoke demo for eigs and svds.

disp('--- eigs and svds Smoke ---');

A = [1 0 0; 0 5 0; 0 0 3];
d = eigs(A, 2);
disp('Top 2 eigenvalues of diag(1, 5, 3) (expected 5, 3):');
disp(d);

s = svds(A, 2);
disp('Top 2 singular values of diag(1, 5, 3) (expected 5, 3):');
disp(s);
