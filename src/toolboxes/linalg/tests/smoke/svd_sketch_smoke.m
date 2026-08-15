% toolboxes/linalg/tests/smoke/svd_sketch_smoke.m
%
% Smoke demo for svdsketch and svdappend.

disp('--- svdsketch and svdappend Smoke ---');

A = [1 2; 3 4];
[U, S, V] = svdsketch(A, 1e-3);
disp('svdsketch(A) singular values S:');
disp(S);

A_new = [5; 6];
[U_new, S_new, V_new] = svdappend(U, S, V, A_new);
disp('svdappend updated singular values S_new:');
disp(S_new);
