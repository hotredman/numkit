% toolboxes/linalg/tests/smoke/schur_complex_smoke.m
%
% Smoke demo for complex Schur decomposition and complex eig.

disp('--- Complex Schur & Eig Smoke ---');
B = [1+1i, 2; 3, 4-1i];

[U, T] = schur(B);
disp('Schur decomposition U * T * U'' - B residual:');
disp(max(max(abs(U*T*U' - B))));

disp('Eigenvalues e = eig(B):');
e = eig(B);
disp(e);

disp('Eigenvectors [V, D] = eig(B):');
[V, D] = eig(B);
disp('B * V - V * D residual:');
disp(max(max(abs(B*V - V*D))));
