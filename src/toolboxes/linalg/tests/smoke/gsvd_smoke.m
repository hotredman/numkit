% toolboxes/linalg/tests/smoke/gsvd_smoke.m
%
% Smoke demo for gsvd (generalized SVD).

disp('--- gsvd Smoke ---');

A = [1 2; 3 4];
B = [1 0; 0 1];
s = gsvd(A, B);

disp('Generalized singular values (expected ~0.365966, ~5.46499):');
disp(s);
