% toolboxes/linalg/tests/smoke/ordqz_smoke.m
%
% Smoke demo for ordqz.

disp('--- ordqz Smoke ---');

A = [4 1; 0 2];
B = [1 0; 0 1];
[AA, BB, Q, Z] = qz(A, B);
[AAS, BBS, QS, ZS] = ordqz(AA, BB, Q, Z, 'lhp');

disp('Reordered AA diagonal:');
disp(diag(AAS));
