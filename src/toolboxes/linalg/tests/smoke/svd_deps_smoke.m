clear

A = [1 2 3; 4 5 6; 7 8 10];
B = [1 2; 2 4; 3 6];

fprintf('=== rank ===\n');
fprintf('  rank(A=full3x3): %d (expect 3)\n', rank(A));
fprintf('  rank(B=rank-1):  %d (expect 1)\n', rank(B));
fprintf('  rank(zeros(4)):  %d (expect 0)\n', rank(zeros(4)));

fprintf('\n=== pinv (Moore-Penrose) ===\n');
P = pinv(A);
fprintf('  A*pinv(A)*A - A: %g (must be 0)\n', max(max(abs(A*P*A - A))));
PB = pinv(B);
fprintf('  B*pinv(B)*B - B: %g (must be 0)\n', max(max(abs(B*PB*B - B))));

fprintf('\n=== cond (2-norm) ===\n');
fprintf('  cond(eye(5)): %g (expect 1)\n', cond(eye(5)));
fprintf('  cond(A):       %g\n', cond(A));
fprintf('  cond(hilb(8)): %g (~1.5e10, very ill-conditioned)\n', cond(hilb(8)));

fprintf('\n=== orth + null ===\n');
Q = orth(A);
N = null(B);
fprintf('  size(orth(A)): [%d %d]\n', size(Q,1), size(Q,2));
fprintf('  size(null(B)): [%d %d]\n', size(N,1), size(N,2));
fprintf('  B*null(B): %g (must be 0)\n', max(abs(B*N)));

fprintf('\n=== normest ===\n');
fprintf('  normest(A) = %g (= largest sigma)\n', normest(A));
fprintf('  svd(A)(1)  = %g (matches)\n', svd(A)(1));
