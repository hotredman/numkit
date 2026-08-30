clear

fprintf('=== cholcov (Cholesky-like factor of covariance) ===\n');

% PD
SIGMA = [4 2 0; 2 5 1; 0 1 3];
[T, p] = cholcov(SIGMA);
fprintf('  PD case [4 2 0;2 5 1;0 1 3]:\n');
disp(T)
fprintf('  expect upper-triangular:\n    2 1 0\n    0 2 0.5\n    0 0 1.6583\n');
fprintf('  p = %d (expect 0)\n', p);
fprintf('  T''*T residual: %g (expect ~0)\n', max(max(abs(T'*T - SIGMA))));

% PSD rank-1
S2 = [4 2; 2 1];
[T2, p2] = cholcov(S2);
fprintf('\n  PSD rank-1 [4 2; 2 1]:\n');
disp(T2)
fprintf('  size(T2) = [%d %d] (expect 1×2)\n', size(T2,1), size(T2,2));
fprintf('  p2 = %d (expect 0)\n', p2);
fprintf('  T2''*T2 residual: %g (expect ~0)\n', max(max(abs(T2'*T2 - S2))));

% Indefinite
S3 = [1 0; 0 -1];
[T3, p3] = cholcov(S3);
fprintf('\n  Indefinite [1 0; 0 -1]: p=%d (expect 1), T size=[%d %d] (expect 0×0)\n', ...
        p3, size(T3,1), size(T3,2));

% Negative def
S4 = -eye(2);
[T4, p4] = cholcov(S4);
fprintf('  Negative def -eye(2): p=%d (expect 2), T size=[%d %d] (expect 0×0)\n', ...
        p4, size(T4,1), size(T4,2));
