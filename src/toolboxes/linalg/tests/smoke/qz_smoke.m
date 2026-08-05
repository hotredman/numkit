% toolboxes/linalg/tests/smoke/qz_smoke.m
%
% Smoke demo for qz (generalized Schur decomposition).

disp('--- qz Smoke ---');

A = [1 2; 3 4];
B = [5 6; 7 8];
[AA, BB, Q, Z] = qz(A, B);

disp('AA (upper triangular):');
disp(AA);
disp('BB (upper triangular):');
disp(BB);

disp('Reconstruction residual Q*A*Z - AA:');
disp(max(max(abs(Q*A*Z - AA))));
disp('Reconstruction residual Q*B*Z - BB:');
disp(max(max(abs(Q*B*Z - BB))));
