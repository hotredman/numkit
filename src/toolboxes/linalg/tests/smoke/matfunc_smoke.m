clear

fprintf('=== expm (matrix exponential, general) ===\n');
A = [0 1; -1 0];
E = expm(A);
fprintf('  expm([0 1; -1 0]) -- rotation by 1 rad:\n');
disp(E);
fprintf('  expect [%.4f %.4f; %.4f %.4f]\n', cos(1), sin(1), -sin(1), cos(1));

fprintf('\n=== logm (sym SPD) ===\n');
S = [4 1 2; 1 3 0; 2 0 5];
L = logm(S);
fprintf('  logm(S):\n'); disp(L);
fprintf('  round-trip expm(logm(S)) - S: %g\n', max(max(abs(expm(L) - S))));

fprintf('\n=== sqrtm (sym PSD) ===\n');
R = sqrtm(S);
fprintf('  R*R - S: %g\n', max(max(abs(R*R - S))));

fprintf('\n=== schur (sym -> diag T) ===\n');
[U, T] = schur(S);
fprintf('  U*T*U'' - S: %g\n', max(max(abs(U*T*U' - S))));
fprintf('  T(1,1)=%g T(2,2)=%g T(3,3)=%g (eigenvalues, ascending)\n', T(1,1), T(2,2), T(3,3));
