clear

fprintf('=== eig (symmetric only, this revision) ===\n');
A = [4 1 2; 1 3 0; 2 0 5];
e = eig(A);
fprintf('  eig([4 1 2; 1 3 0; 2 0 5]):\n  '); disp(e');

[V, D] = eig(A);
fprintf('  A*V - V*D max err: %g\n', max(max(abs(A*V - V*D))));
fprintf('  V''*V - I max err: %g\n', max(max(abs(V'*V - eye(3)))));
fprintf('  sum(eig) - trace(A): %g (expect 0)\n', sum(e) - trace(A));
fprintf('  prod(eig) - det(A): %g (expect 0)\n', prod(e) - det(A));

fprintf('\n=== diagonal ===\n');
fprintf('  eig(diag([3 7 1 9 5])): '); disp(eig(diag([3 7 1 9 5]))');
fprintf('  expect ascending: 1 3 5 7 9\n');

fprintf('\n=== SPD (Cholesky-decomposable) ===\n');
S = [4 12 -16; 12 37 -43; -16 -43 98];
es = eig(S);
fprintf('  eig(SPD): '); disp(es');
fprintf('  all positive: %d\n', all(es > 0));
