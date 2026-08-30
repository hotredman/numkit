clear

fprintf('=== condeig ===\n');

% Symmetric A → every s_i == 1.
fprintf('\nsymmetric 3x3:\n');
A = [4 1 0; 1 3 0; 0 0 2];
s = condeig(A);
fprintf('  s = [%.4f %.4f %.4f]   (expect [1 1 1])\n', s(1), s(2), s(3));

% Non-symmetric upper-triangular [2 1; 0 3]: both s = sqrt(2).
fprintf('\nupper-tri [2 1; 0 3]:\n');
A2 = [2 1; 0 3];
s2 = condeig(A2);
fprintf('  s = [%.4f %.4f]   (expect [1.414 1.414])\n', s2(1), s2(2));

% 3-output form: [V, D, s] = condeig(A) — verify eigendecomposition.
fprintf('\n[V, D, s] = condeig(A):\n');
[V, D, s3] = condeig(A);
fprintf('  V shape: %dx%d, D shape: %dx%d, s shape: %dx%d\n', ...
        size(V,1), size(V,2), size(D,1), size(D,2), size(s3,1), size(s3,2));
fprintf('  max|A*V - V*D| = %.2e   (expect ~ulp)\n', max(max(abs(A*V - V*D))));

% Identity — perfectly conditioned trivially.
fprintf('\neye(5):\n');
sI = condeig(eye(5));
fprintf('  max(s) = %.4f  (expect 1)\n', max(sI));
