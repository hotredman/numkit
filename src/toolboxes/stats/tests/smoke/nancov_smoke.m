clear

fprintf('=== nancov — NaN-aware covariance ===\n');

% Single-input form.
X = [1 2; 3 4; NaN 6; 5 8];
fprintf('X:\n'); disp(X);
C = nancov(X);
fprintf('nancov(X) (NaN row dropped → cov of [1 2; 3 4; 5 8]):\n'); disp(C);
fprintf('   expected:  cov([1 2; 3 4; 5 8]) =\n'); disp(cov([1 2; 3 4; 5 8]));

% Normalization flag.
C0 = nancov(X, 0);
C1 = nancov(X, 1);
fprintf('nancov(X, 0) and nancov(X, 1):\n');
fprintf('   ratio C0(1,1)/C1(1,1) = %.4f  (expect 1.5 = n/(n-1) for n=3)\n', C0(1,1)/C1(1,1));

% Two-vector form.
v1 = [1; 2; NaN; 4; 5];
v2 = [10; 20; 30; NaN; 50];
fprintf('\nnancov(v1, v2): drops rows 3 and 4\n');
disp(nancov(v1, v2));

% Vector input → scalar variance.
v = [1; 2; NaN; 4; 5];
fprintf('nancov(vector) = %g  (var of non-NaN: %g)\n', nancov(v), var([1; 2; 4; 5]));

% All-NaN edge case.
fprintf('\nnancov([NaN 1; 2 NaN]):\n'); disp(nancov([NaN 1; 2 NaN]));
